/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Qualcomm Mobile QSEECOM Interface Driver (/dev/qseecom_mobile)
 *
 * Copyright (C) 2026 Nitescu Ion Laurentiu <contact@nyl.ro>
 */

/**
 * DOC: Qualcomm Mobile QSEECOM Transport Architecture
 *
 * Modern Qualcomm Snapdragon mobile SoCs (such as SM7225 / Fairphone 4) host
 * security-critical peripheral drivers and cryptographic services inside the
 * Qualcomm Secure Execution Environment (QSEE / TrustZone). Biometric sensors,
 * hardware-backed keystores (Keymaster), and DRM engines are packaged as
 * signed split-ELF Trusted Applications (TAs) residing in /lib/firmware/.
 *
 * Unlike UEFI-based Windows-on-Snapdragon laptops that only interact with a
 * pre-loaded 'uefisecapp' trustlet, mobile platforms require four capabilities
 * not exposed to userspace by the upstream laptop QSEECOM driver:
 *
 * 1. Dynamic Split-ELF TA Loading (PIL / App Manager):
 *    Mobile TAs (e.g. 'keymaster64', 'fingerprint' / 'fngap64') are stored on
 *    the root filesystem as a metadata header (.mdt) and per-segment binaries
 *    (.b00 .. .bNN). This driver reconstructs the contiguous image inside a
 *    DMA-coherent TrustZone memory pool (qcom_tzmem / SHMBridge) and invokes
 *    QSEECOM_TZ_CMD_APP_START (Owner 50, Service 1, Command 1).
 *
 * 2. Direct ARM64 SMC Marshaling with Extended Arguments and Wait-Queue Resume:
 *    Commands directed at loaded TAs use Owner 48 (QSEECOM_TZ_OWNER_TZ_APPS),
 *    Service 0, Command 1 (QSEECOM_TZ_CMD_APP_SEND) with 5 arguments. Under the
 *    Qualcomm SMCCC convention, when more than 4 arguments are passed, register
 *    x5 holds the physical address of a secondary TZ-accessible buffer holding
 *    arguments 3..N. If TrustZone blocks on an internal event and returns
 *    QCOM_SCM_WAITQ_SLEEP (2), the normal world must resume the sleeping SMC
 *    context via QCOM_SCM_SVC_WAITQ / QCOM_SCM_WAITQ_RESUME without clobbering
 *    the original SMC descriptor in case a subsequent QCOM_SCM_V2_EBUSY (-12)
 *    requires re-issuing the original call.
 *
 * 3. Re-Entrant Normal-World Listener Upcalls (FS & GPFS):
 *    During TA initialization (CMD_INIT), database loading (CMD_LOAD_DB), or
 *    template persistence (CMD_STORE_DB), QSEE suspends the active TA execution
 *    and returns QSEECOM_RESULT_INCOMPLETE (1) with response type 0xEE02 and a
 *    target listener ID (e.g. 10 for legacy File Service, 28672 / 0x7000 for
 *    GlobalPlatform File Table Service). The kernel driver wakes the matching
 *    userspace listener worker thread, waits for userspace to populate the
 *    shared bounce buffer, and issues QSEECOM_TZ_CMD_LISTENER_RESPONSE (Owner
 *    50, Service 2, Command 3) in a loop until the TA finishes its command and
 *    returns QSEECOM_RESULT_SUCCESS (0).
 *
 * 4. Multi-TA Residency and Serialization:
 *    Inter-TA cryptographic protocols (such as qsee_encapsulate() in
 *    keymaster64 and qsee_decapsulate() in fngap64) require both trustlets to
 *    remain simultaneously resident in QSEE memory. All SMC transitions and
 *    shared request/response DMA buffers are serialized via a device-level
 *    mutex (smc_lock) to guarantee race-free operation across concurrent
 *    userspace worker threads.
 */

#include <linux/arm-smccc.h>
#include <linux/completion.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/elf.h>
#include <linux/errno.h>
#include <linux/firmware.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/wait.h>
#include <linux/kprobes.h>

#include <linux/firmware/qcom/qcom_scm.h>
#include <linux/firmware/qcom/qcom_tzmem.h>

#if __has_include(<uapi/linux/qseecom_mobile.h>)
#include <uapi/linux/qseecom_mobile.h>
#else
#include "../include/uapi/qseecom_mobile.h"
#endif

/* Maximum request and response payload sizes for TA command buffers (64 KiB) */
#define QSEE_MAX_REQ_SIZE			SZ_64K
#define QSEE_MAX_RSP_SIZE			SZ_64K

/* Maximum length of a TrustZone application name including NUL terminator */
#define QSEE_MAX_APP_NAME_SIZE			64

/* Upper bound on consecutive listener upcalls per TA command to prevent hangs */
#define QSEE_MAX_LISTENER_ROUNDS		256

/* Qualcomm SMCCC return codes and wait-queue service identifiers */
#define QCOM_SCM_V2_EBUSY			-12
#define QCOM_SCM_INTERRUPTED			1
#define QCOM_SCM_WAITQ_SLEEP			2
#define QCOM_SCM_SVC_WAITQ			0x24
#define QCOM_SCM_WAITQ_RESUME			0x02
#define QCOM_SCM_EBUSY_WAIT_MS			30
#define QCOM_SCM_EBUSY_MAX_RETRY		20

/* Construct 16-bit SMCCC function identifier from 8-bit service and command */
#define SCM_SMC_FNID(s, c)			((((s) & 0xFF) << 8) | ((c) & 0xFF))

/* Qualcomm SMCCC register argument packing constants */
#define SCM_SMC_N_REG_ARGS			4
#define SCM_SMC_FIRST_REG_IDX			2
#define SCM_SMC_FIRST_EXT_IDX			(SCM_SMC_N_REG_ARGS - 1)
#define SCM_SMC_N_EXT_ARGS			7

/**
 * enum qcom_scm_qseecom_tz_owner - ARM SMCCC Owner IDs for Qualcomm QSEE
 * @QSEECOM_TZ_OWNER_SIP:     Silicon Provider calls (wait-queue resume).
 * @QSEECOM_TZ_OWNER_TZ_APPS: Direct calls targeting loaded Trusted Applications.
 * @QSEECOM_TZ_OWNER_QSEE_OS: Calls targeting the QSEE OS kernel (App Mgr / Listener).
 */
enum qcom_scm_qseecom_tz_owner {
	QSEECOM_TZ_OWNER_SIP			= 2,
	QSEECOM_TZ_OWNER_TZ_APPS		= 48,
	QSEECOM_TZ_OWNER_QSEE_OS		= 50,
};

/**
 * enum qcom_scm_qseecom_tz_svc - QSEE OS Service IDs encoded in SMCCC Function ID
 * @QSEECOM_TZ_SVC_APP_ID_PLACEHOLDER: Service 0 used when dispatching to TZ_APPS.
 * @QSEECOM_TZ_SVC_APP_MGR:            Application lifecycle management service.
 * @QSEECOM_TZ_SVC_LISTENER:           Normal-world listener registration & response.
 * @QSEECOM_TZ_SVC_INFO:               QSEE feature and version query service.
 */
enum qcom_scm_qseecom_tz_svc {
	QSEECOM_TZ_SVC_APP_ID_PLACEHOLDER	= 0,
	QSEECOM_TZ_SVC_APP_MGR			= 1,
	QSEECOM_TZ_SVC_LISTENER			= 2,
	QSEECOM_TZ_SVC_INFO			= 6,
};

/**
 * enum qcom_scm_qseecom_tz_cmd_app - QSEE Application Manager & TA Commands
 * @QSEECOM_TZ_CMD_APP_START:          Load and start a split-ELF TA image.
 * @QSEECOM_TZ_CMD_APP_SHUTDOWN:       Unload a running TA instance by ID.
 * @QSEECOM_TZ_CMD_APP_LOOKUP:         Query the numeric app_id of a loaded TA.
 * @QSEECOM_TZ_CMD_APP_SEND:           Send a synchronous request/response to a TA.
 * @QSEECOM_TZ_CMD_APP_SEND_WHITELIST: Send request with scatter-gather whitelist.
 */
enum qcom_scm_qseecom_tz_cmd_app {
	QSEECOM_TZ_CMD_APP_START		= 1,
	QSEECOM_TZ_CMD_APP_SHUTDOWN		= 2,
	QSEECOM_TZ_CMD_APP_LOOKUP		= 3,
	QSEECOM_TZ_CMD_APP_SEND			= 1,
	QSEECOM_TZ_CMD_APP_SEND_WHITELIST	= 6,
};

/**
 * enum qcom_scm_qseecom_tz_cmd_listener - QSEE Listener Service Commands
 * @QSEECOM_TZ_CMD_LISTENER_REGISTER:   Register a normal-world listener and shared buffer.
 * @QSEECOM_TZ_CMD_LISTENER_DEREGISTER: Unregister a previously registered listener ID.
 * @QSEECOM_TZ_CMD_LISTENER_RESPONSE:   Return normal-world listener completion to QSEE.
 */
enum qcom_scm_qseecom_tz_cmd_listener {
	QSEECOM_TZ_CMD_LISTENER_REGISTER	= 1,
	QSEECOM_TZ_CMD_LISTENER_DEREGISTER	= 2,
	QSEECOM_TZ_CMD_LISTENER_RESPONSE	= 3,
};

/**
 * enum qcom_scm_qseecom_result - QSEE OS execution status codes returned in x1
 * @QSEECOM_RESULT_SUCCESS:             Command completed; final output in rsp_buf.
 * @QSEECOM_RESULT_INCOMPLETE:          TA yielded to normal-world listener in x3.
 * @QSEECOM_RESULT_BLOCKED_ON_LISTENER: TA is blocked waiting on listener completion.
 * @QSEECOM_RESULT_FAILURE:             Command failed inside QSEE OS.
 */
enum qcom_scm_qseecom_result {
	QSEECOM_RESULT_SUCCESS			= 0,
	QSEECOM_RESULT_INCOMPLETE		= 1,
	QSEECOM_RESULT_BLOCKED_ON_LISTENER	= 2,
	QSEECOM_RESULT_FAILURE			= 0xFFFFFFFF,
};

/**
 * enum qcom_scm_resp_type - Descriptor types returned in x2 by QSEE OS
 * @QSEECOM_SCM_RES_APP_ID:            x3 contains the allocated TA app_id.
 * @QSEECOM_SCM_RES_QSEOS_LISTENER_ID: x3 contains the requested listener_id.
 */
enum qcom_scm_resp_type {
	QSEECOM_SCM_RES_APP_ID			= 0xEE01,
	QSEECOM_SCM_RES_QSEOS_LISTENER_ID	= 0xEE02,
};

/* Argument type specifiers encoded into smc_args[1] (arginfo) */
#define QCOM_SCM_VAL		0
#define QCOM_SCM_RO		1
#define QCOM_SCM_RW		2
#define QCOM_SCM_BUFVAL		3

#define QCOM_SCM_ARGS_IMPL(num, a, b, c, d, e, f, g, h, i, j, ...) (\
			   (((a) & 0x3) << 4) | \
			   (((b) & 0x3) << 6) | \
			   (((c) & 0x3) << 8) | \
			   (((d) & 0x3) << 10) | \
			   (((e) & 0x3) << 12) | \
			   (((f) & 0x3) << 14) | \
			   (((g) & 0x3) << 16) | \
			   (((h) & 0x3) << 18) | \
			   (((i) & 0x3) << 20) | \
			   (((j) & 0x3) << 22) | \
			   ((num) & 0xf))

#define QCOM_SCM_ARGS(...) QCOM_SCM_ARGS_IMPL(__VA_ARGS__, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0)

/**
 * struct qsee_scm_desc - High-level descriptor for a Qualcomm QSEE SMC call
 * @svc:     8-bit service identifier (enum qcom_scm_qseecom_tz_svc).
 * @cmd:     8-bit command identifier.
 * @arginfo: Packed argument count and parameter type bitmask.
 * @args:    Up to 10 64-bit command arguments (scalars or physical addresses).
 * @owner:   6-bit SMCCC owner identifier (enum qcom_scm_qseecom_tz_owner).
 */
struct qsee_scm_desc {
	u32 svc;
	u32 cmd;
	u32 arginfo;
	u64 args[10];
	u32 owner;
};

/**
 * struct qsee_scm_resp - Output registers returned by a QSEE SMC invocation
 * @result:    Execution status returned in x1 (enum qcom_scm_qseecom_result).
 * @resp_type: Response payload discriminator returned in x2 (enum qcom_scm_resp_type).
 * @data:      Response value returned in x3 (app_id or listener_id).
 */
struct qsee_scm_resp {
	u64 result;
	u64 resp_type;
	u64 data;
};

/**
 * struct qsee_mobile_listener - Registered normal-world listener context
 * @node:        List node linked into qsee_mobile_dev.listeners.
 * @id:          Numeric listener service ID (e.g. 10 for FS, 28672 for GPFS).
 * @sb_virt:     Kernel virtual address of the TZMEM shared bounce buffer.
 * @sb_len:      Size in bytes of the shared bounce buffer.
 * @pending:     Set to true when TrustZone requests service from this listener.
 * @resp_status: Status word returned by userspace when answering the request.
 * @req_wq:      Wait queue where the userspace worker blocks awaiting requests.
 * @resp_wq:     Wait queue where the kernel SMC thread blocks awaiting userspace.
 * @answered:    Set to true when userspace submits QSEECOM_MOBILE_IOC_RESPOND_LISTENER.
 * @dead:        Set to true during teardown to wake and abort blocked waiters.
 * @ref:         Reference count protecting the structure across blocking waits.
 */
struct qsee_mobile_listener {
	struct list_head node;
	u32 id;
	void *sb_virt;
	size_t sb_len;
	bool pending;
	u32 resp_status;
	wait_queue_head_t req_wq;
	wait_queue_head_t resp_wq;
	bool answered;
	bool dead;
	struct kref ref;
};

/**
 * qsee_listener_free() - Release TZMEM bounce buffer and free listener struct
 * @kref: Pointer to embedded struct kref reaching zero references.
 */
static void qsee_listener_free(struct kref *kref)
{
	struct qsee_mobile_listener *listener =
		container_of(kref, struct qsee_mobile_listener, ref);

	qcom_tzmem_free(listener->sb_virt);
	kfree(listener);
}

/**
 * struct qsee_mobile_dev - Singleton device state for /dev/qseecom_mobile
 * @miscdev:         Misc character device registration structure.
 * @mempool:         DMA-coherent / SHMBridge memory pool shared with TrustZone.
 * @req_buf:         Pre-allocated 64 KiB TZMEM buffer for TA command requests.
 * @rsp_buf:         Pre-allocated 64 KiB TZMEM buffer for TA command responses.
 * @sglist_buf:      Auxiliary TZMEM buffer for scatter-gather metadata.
 * @sglist_phys:     Physical address of sglist_buf.
 * @smc_lock:        Mutex serializing all SMC calls and req_buf/rsp_buf access.
 * @listener_lock:   Mutex protecting the registered listeners linked list.
 * @listeners:       Head of linked list of active struct qsee_mobile_listener.
 * @loaded_app_id:   Most recently loaded TA app_id cached by the driver.
 * @loaded_app_name: Name of the most recently loaded TA.
 */
struct qsee_mobile_dev {
	struct miscdevice miscdev;
	struct qcom_tzmem_pool *mempool;
	void *req_buf;
	void *rsp_buf;
	void *sglist_buf;
	phys_addr_t sglist_phys;
	struct mutex smc_lock;
	struct mutex listener_lock;
	struct list_head listeners;
	u32 loaded_app_id;
	char loaded_app_name[64];
};

static struct qsee_mobile_dev *qdev;

struct qsee_scm_kernel_res {
	u64 result[3];
};

typedef unsigned long (*qsee_kallsyms_lookup_name_t)(const char *name);

/*
 * kCFI hashes indirect call targets by type name, so the prototype must use
 * the kernel's own type names (struct qcom_scm_desc/res, enum qcom_scm_convention).
 */
struct qcom_scm_desc;
struct qcom_scm_res;
enum qcom_scm_convention {
	SMC_CONVENTION_UNKNOWN,
	SMC_CONVENTION_LEGACY,
	SMC_CONVENTION_ARM_32,
	SMC_CONVENTION_ARM_64,
};

typedef int (*qsee_scm_smc_call_t)(struct device *dev,
				   const struct qcom_scm_desc *desc,
				   enum qcom_scm_convention qcom_convention,
				   struct qcom_scm_res *res,
				   bool atomic);

static qsee_scm_smc_call_t qsee_kernel_scm_call;
static struct mutex *qsee_kernel_scm_lock;
static struct device *qsee_kernel_scm_dev;

/**
 * qsee_resolve_scm_symbols() - Dynamically bind to kernel SCM subsystem
 *
 * All upstream kernel subsystems (Modem PAS, Wi-Fi ath10k, IPA, PIL, TZMEM)
 * serialize their TrustZone SMC transactions through qcom_scm_lock in
 * drivers/firmware/qcom/qcom_scm-smc.c.
 *
 * To avoid race conditions and EL3 watchdog bites when userspace biometrics
 * interact with TrustZone concurrently with modem/Wi-Fi operations, we
 * resolve __scm_smc_call and __scm via kprobes/kallsyms_lookup_name so that
 * all /dev/qseecom_mobile operations share the exact same kernel mutex, wait-queue
 * completion handler, and EBUSY backoff mechanism.
 *
 * Return: 0 on success, negative errno on failure.
 */
static int qsee_resolve_scm_symbols(void)
{
	struct kprobe kp = {
		.symbol_name = "kallsyms_lookup_name",
	};
	qsee_kallsyms_lookup_name_t lookup_fn;
	void **p_scm;
	int ret;

	ret = register_kprobe(&kp);
	if (ret < 0) {
		pr_warn("qseecom_mobile: could not hook kallsyms_lookup_name: %d\n", ret);
		return ret;
	}
	lookup_fn = (qsee_kallsyms_lookup_name_t)kp.addr;
	unregister_kprobe(&kp);

	if (!lookup_fn)
		return -ENOENT;

	qsee_kernel_scm_call = (qsee_scm_smc_call_t)lookup_fn("__scm_smc_call");
	qsee_kernel_scm_lock = (struct mutex *)lookup_fn("qcom_scm_lock");

	p_scm = (void **)lookup_fn("__scm");
	if (p_scm && *p_scm)
		qsee_kernel_scm_dev = *(struct device **)(*p_scm);

	pr_info("qseecom_mobile: SCM synchronization active: smc_call=%ps scm_lock=%p scm_dev=%s\n",
		qsee_kernel_scm_call, qsee_kernel_scm_lock,
		qsee_kernel_scm_dev ? dev_name(qsee_kernel_scm_dev) : "none");

	return 0;
}

/**
 * qsee_smc_do_quirk() - Execute low-level ARM64 SMC with Qualcomm x6 session quirk
 * @args: Array of 8 input register values (x0..x7).
 * @res:  Output structure receiving return registers (x0..x3).
 *
 * When TrustZone is preempted by a normal-world interrupt during an SMC call,
 * QSEE returns QCOM_SCM_INTERRUPTED (1) in x0 and places a session continuation
 * token in x6. This helper transparently loops and re-enters EL3 with the
 * updated x0/x6 registers until the call either completes or sleeps on a waitq.
 */
static void qsee_smc_do_quirk(const unsigned long *args,
			      struct arm_smccc_res *res)
{
	unsigned long a0 = args[0];
	struct arm_smccc_quirk quirk = { .id = ARM_SMCCC_QUIRK_QCOM_A6 };

	quirk.state.a6 = 0;

	do {
		arm_smccc_smc_quirk(a0, args[1], args[2], args[3],
				    args[4], args[5], quirk.state.a6,
				    args[7], res, &quirk);
		if (res->a0 == QCOM_SCM_INTERRUPTED)
			a0 = res->a0;
	} while (res->a0 == QCOM_SCM_INTERRUPTED);
}

/**
 * qsee_smc_call() - Marshal arguments and execute a synchronous QSEE SMC call
 * @qd:   Pointer to the qsee_mobile_dev driver instance.
 * @desc: High-level SMC descriptor specifying owner, service, command, and args.
 * @res:  Optional output structure populated with x1..x3 upon success.
 *
 * Handles three critical protocol details of Qualcomm QSEE 5.x:
 * 1. Centralized kernel synchronization: when bound to __scm_smc_call, executes
 *    under qcom_scm_lock to guarantee zero concurrency with Modem, Wi-Fi, and PIL.
 * 2. Extended argument buffer allocation: when a command carries > 4 arguments
 *    (such as QSEECOM_TZ_CMD_APP_SEND with 5 arguments), arguments 3..N are
 *    marshaled into a TZMEM buffer passed in x5.
 * 3. Wait-queue and busy retries: transparently handles QCOM_SCM_WAITQ_SLEEP (2)
 *    and QCOM_SCM_V2_EBUSY (-12).
 *
 * Return: 0 on success, negative errno on allocation or SMC failure.
 */
static int qsee_smc_call(struct qsee_mobile_dev *qd,
			 const struct qsee_scm_desc *desc,
			 struct qsee_scm_resp *res)
{
	int arglen;
	struct arm_smccc_res smc_res;
	unsigned long smc_args[8] = {0};
	unsigned long resume_args[8] = {0};
	const unsigned long *cur_args;
	__le64 *ext_args = NULL;
	int retry_count = 0;
	int i;

	/*
	 * Centralized kernel SCM execution path:
	 * If __scm_smc_call and scm_dev were successfully bound via kallsyms,
	 * delegate the SMC to upstream's engine. This automatically:
	 * 1. Locks the kernel-wide qcom_scm_lock (preventing race conditions
	 *    with Modem PAS, PIL, Wi-Fi ath10k, and IPA).
	 * 2. Manages extended argument TZMEM allocation if arglen > 4.
	 * 3. Handles QCOM_SCM_WAITQ_SLEEP with qcom_scm_wait_for_wq_completion.
	 * 4. Handles QCOM_SCM_V2_EBUSY backoff retries.
	 */
	if (qsee_kernel_scm_call && qsee_kernel_scm_dev) {
		struct qsee_scm_kernel_res kernel_res = {};
		int ret;

		ret = qsee_kernel_scm_call(qsee_kernel_scm_dev,
					   (const void *)desc,
					   3 /* SMC_CONVENTION_ARM_64 */,
					   (void *)&kernel_res, false);
		if (res) {
			res->result = kernel_res.result[0];
			res->resp_type = kernel_res.result[1];
			res->data = kernel_res.result[2];
		}
		if (ret)
			pr_err("qseecom_mobile: __scm_smc_call error %d (owner=%u svc=%u cmd=%u)\n",
			       ret, desc->owner, desc->svc, desc->cmd);
		return ret;
	}

	/*
	 * Fallback direct SMCCC execution path:
	 * Used if __scm_smc_call could not be dynamically resolved. If the
	 * global qcom_scm_lock symbol was found, serialize access through it.
	 */
	arglen = desc->arginfo & 0xf;
	smc_args[0] = ARM_SMCCC_CALL_VAL(
		ARM_SMCCC_STD_CALL,
		ARM_SMCCC_SMC_64,
		desc->owner,
		SCM_SMC_FNID(desc->svc, desc->cmd));
	smc_args[1] = desc->arginfo;

	for (i = 0; i < SCM_SMC_N_REG_ARGS; i++)
		smc_args[i + SCM_SMC_FIRST_REG_IDX] = desc->args[i];

	if (arglen > SCM_SMC_N_REG_ARGS) {
		ext_args = qcom_tzmem_alloc(qd->mempool,
					    SCM_SMC_N_EXT_ARGS * sizeof(u64),
					    GFP_KERNEL);
		if (!ext_args)
			return -ENOMEM;

		for (i = 0; i < SCM_SMC_N_EXT_ARGS; i++)
			ext_args[i] = cpu_to_le64(desc->args[i + SCM_SMC_FIRST_EXT_IDX]);

		smc_args[5] = qcom_tzmem_to_phys(ext_args);
	}

	if (qsee_kernel_scm_lock)
		mutex_lock(qsee_kernel_scm_lock);

	do {
		cur_args = smc_args;
		do {
			qsee_smc_do_quirk(cur_args, &smc_res);
			if (smc_res.a0 == QCOM_SCM_WAITQ_SLEEP) {
				usleep_range(1000, 2000);
				memset(resume_args, 0, sizeof(resume_args));
				resume_args[0] = ARM_SMCCC_CALL_VAL(
					ARM_SMCCC_STD_CALL,
					ARM_SMCCC_SMC_64,
					ARM_SMCCC_OWNER_SIP,
					SCM_SMC_FNID(QCOM_SCM_SVC_WAITQ,
						     QCOM_SCM_WAITQ_RESUME));
				resume_args[1] = QCOM_SCM_ARGS(1);
				resume_args[2] = smc_res.a2;
				cur_args = resume_args;
			}
		} while (smc_res.a0 == QCOM_SCM_WAITQ_SLEEP);

		if (smc_res.a0 == QCOM_SCM_V2_EBUSY) {
			if (++retry_count > QCOM_SCM_EBUSY_MAX_RETRY)
				break;
			if (qsee_kernel_scm_lock)
				mutex_unlock(qsee_kernel_scm_lock);
			msleep(QCOM_SCM_EBUSY_WAIT_MS);
			if (qsee_kernel_scm_lock)
				mutex_lock(qsee_kernel_scm_lock);
		}
	} while (smc_res.a0 == QCOM_SCM_V2_EBUSY);

	if (qsee_kernel_scm_lock)
		mutex_unlock(qsee_kernel_scm_lock);

	if (ext_args)
		qcom_tzmem_free(ext_args);

	if ((long)smc_res.a0 != 0) {
		pr_err("qseecom_mobile: SMC failed a0=0x%lx (owner=%u svc=%u cmd=%u)\n",
		       smc_res.a0, desc->owner, desc->svc, desc->cmd);
		return (long)smc_res.a0 == QCOM_SCM_V2_EBUSY ? -EBUSY : -EIO;
	}

	if (res) {
		res->result = smc_res.a1;
		res->resp_type = smc_res.a2;
		res->data = smc_res.a3;
	}

	return 0;
}

/**
 * qsee_find_listener() - Look up a registered listener by its numeric ID
 * @qd: Driver instance.
 * @id: Listener service ID to locate.
 *
 * Context: Must be called with qd->listener_lock held.
 * Return: Pointer to matching struct qsee_mobile_listener, or NULL if not found.
 */
static struct qsee_mobile_listener *qsee_find_listener(struct qsee_mobile_dev *qd, u32 id)
{
	struct qsee_mobile_listener *entry;

	list_for_each_entry(entry, &qd->listeners, node) {
		if (entry->id == id)
			return entry;
	}
	return NULL;
}

/**
 * qsee_listener_respond() - Notify QSEE OS that a listener request was serviced
 * @qd:     Driver instance.
 * @id:     Serviced listener ID.
 * @status: Completion status returned by normal-world listener (0 on success).
 * @res:    Output SMC response (may indicate another INCOMPLETE listener round).
 *
 * Return: 0 on SMC transport success, negative errno on failure.
 */
static int qsee_listener_respond(struct qsee_mobile_dev *qd, u32 id, u32 status,
				 struct qsee_scm_resp *res)
{
	struct qsee_scm_desc desc = {};

	desc.owner = QSEECOM_TZ_OWNER_QSEE_OS;
	desc.svc = QSEECOM_TZ_SVC_LISTENER;
	desc.cmd = QSEECOM_TZ_CMD_LISTENER_RESPONSE;
	desc.arginfo = QCOM_SCM_ARGS(2, QCOM_SCM_VAL, QCOM_SCM_VAL);
	desc.args[0] = id;
	desc.args[1] = status;

	return qsee_smc_call(qd, &desc, res);
}

/**
 * qsee_mobile_dispatch_listener() - Wake userspace listener and await completion
 * @qd: Driver instance.
 * @id: Listener ID requested by TrustZone.
 *
 * Marks the target listener as pending, wakes the userspace worker thread
 * blocked in QSEECOM_MOBILE_IOC_WAIT_LISTENER, and sleeps until userspace
 * writes the response payload and invokes QSEECOM_MOBILE_IOC_RESPOND_LISTENER.
 *
 * Return: 0 (or listener status word) on completion, 0xFFFFFFFF on error.
 */
static u32 qsee_mobile_dispatch_listener(struct qsee_mobile_dev *qd, u32 id)
{
	struct qsee_mobile_listener *listener;
	int ret;

	mutex_lock(&qd->listener_lock);
	listener = qsee_find_listener(qd, id);
	if (!listener) {
		mutex_unlock(&qd->listener_lock);
		pr_warn_once("qseecom_mobile: no listener registered for id %u\n", id);
		return 0xFFFFFFFF;
	}

	listener->answered = false;
	listener->pending = true;
	wake_up_interruptible(&listener->req_wq);
	mutex_unlock(&qd->listener_lock);

	ret = wait_event_interruptible(listener->resp_wq, listener->answered);
	if (ret) {
		pr_err("qseecom_mobile: listener %u interrupted while waiting for userspace\n", id);
		return 0xFFFFFFFF;
	}

	return listener->resp_status;
}

/**
 * qsee_service_listeners() - Drive the re-entrant QSEE listener upcall loop
 * @qd:  Driver instance.
 * @res: In/out SMC response structure updated after each listener round.
 *
 * While QSEE returns QSEECOM_RESULT_INCOMPLETE (1), dispatches the requested
 * listener ID in res->data to userspace and feeds the completion back to
 * TrustZone via qsee_listener_respond() until the TA finishes its command.
 *
 * Return: 0 when res->result transitions out of INCOMPLETE, negative errno on error.
 */
static int qsee_service_listeners(struct qsee_mobile_dev *qd, struct qsee_scm_resp *res)
{
	unsigned int rounds = 0;
	u32 status;
	int ret;
	u32 id;

	while (res->result == QSEECOM_RESULT_INCOMPLETE) {
		if (res->resp_type != QSEECOM_SCM_RES_QSEOS_LISTENER_ID || res->data > U32_MAX) {
			pr_err("qseecom_mobile: unexpected response type 0x%llx data 0x%llx\n",
			       res->resp_type, res->data);
			return -EPROTO;
		}

		id = res->data;
		if (++rounds > QSEE_MAX_LISTENER_ROUNDS) {
			pr_err("qseecom_mobile: listener %u exceeded max rounds\n", id);
			qsee_listener_respond(qd, id, 0xFFFFFFFF, res);
			return -ELOOP;
		}

		status = qsee_mobile_dispatch_listener(qd, id);

		ret = qsee_listener_respond(qd, id, status, res);
		if (ret) {
			pr_err("qseecom_mobile: failed to answer listener %u: %d\n", id, ret);
			return ret;
		}
	}

	return 0;
}

/**
 * qsee_app_get_id() - Query QSEE OS for the app_id of an already-loaded TA
 * @qd:       Driver instance.
 * @app_name: Null-terminated ASCII name of the TA (e.g. "keymaster64").
 * @app_id:   Output pointer receiving the numeric TA identifier if running.
 *
 * Return: 0 if the TA is already resident in QSEE, -ENOENT if not loaded,
 *         or negative errno on failure.
 */
static int qsee_app_get_id(struct qsee_mobile_dev *qd, const char *app_name, u32 *app_id)
{
	unsigned long name_buf_size = QSEE_MAX_APP_NAME_SIZE;
	unsigned long app_name_len = strlen(app_name);
	struct qsee_scm_desc desc = {};
	struct qsee_scm_resp res = {};
	char *name_buf;
	int ret;

	if (app_name_len >= name_buf_size)
		return -EINVAL;

	name_buf = qcom_tzmem_alloc(qd->mempool, name_buf_size, GFP_KERNEL);
	if (!name_buf)
		return -ENOMEM;

	memcpy(name_buf, app_name, app_name_len);

	desc.owner = QSEECOM_TZ_OWNER_QSEE_OS;
	desc.svc = QSEECOM_TZ_SVC_APP_MGR;
	desc.cmd = QSEECOM_TZ_CMD_APP_LOOKUP;
	desc.arginfo = QCOM_SCM_ARGS(2, QCOM_SCM_RW, QCOM_SCM_VAL);
	desc.args[0] = qcom_tzmem_to_phys(name_buf);
	desc.args[1] = app_name_len;

	mutex_lock(&qd->smc_lock);
	ret = qsee_smc_call(qd, &desc, &res);
	mutex_unlock(&qd->smc_lock);
	qcom_tzmem_free(name_buf);

	pr_info("qseecom_mobile: app_get_id('%s') SMC ret=%d res: result=0x%llx type=0x%llx data=0x%llx\n",
		app_name, ret, res.result, res.resp_type, res.data);

	if (ret)
		return ret;

	if (res.result != QSEECOM_RESULT_SUCCESS || res.resp_type != QSEECOM_SCM_RES_APP_ID)
		return -ENOENT;

	*app_id = res.data;
	return 0;
}

/**
 * qsee_app_load() - Load and start a split-ELF TrustZone application
 * @qd:       Driver instance.
 * @dev:      Device pointer used for request_firmware() lookups.
 * @app_name: Base firmware name of the TA (e.g. "keymaster64", "fingerprint").
 * @app_id:   Output pointer receiving the newly started (or existing) TA ID.
 *
 * First queries QSEE via qsee_app_get_id() to check if the TA is already
 * loaded. If not, loads '<app_name>.mdt' and all '<app_name>.b00' .. '.bNN'
 * ELF program segments into a single contiguous TZMEM buffer and invokes
 * QSEECOM_TZ_CMD_APP_START. Services any listener upcalls triggered during TA
 * initialization.
 *
 * Return: 0 on success, negative errno on firmware or QSEE failure.
 */
static int qsee_app_load(struct qsee_mobile_dev *qd, struct device *dev,
			 const char *app_name, u32 *app_id)
{
	const struct firmware *mdt;
	const struct firmware *seg_fw;
	char fw_name[128];
	void *fw_buf;
	phys_addr_t fw_phys;
	struct qsee_scm_desc desc = {};
	struct qsee_scm_resp res = {};
	size_t total_len, mdt_len;
	u16 num_segments;
	u8 *buf_ptr;
	int ret, i;

	if (qd->loaded_app_id && strcmp(qd->loaded_app_name, app_name) == 0) {
		*app_id = qd->loaded_app_id;
		pr_info("qseecom_mobile: app '%s' already loaded in driver with id %u\n", app_name, *app_id);
		return 0;
	}

	ret = qsee_app_get_id(qd, app_name, app_id);
	if (!ret) {
		qd->loaded_app_id = *app_id;
		strscpy(qd->loaded_app_name, app_name, sizeof(qd->loaded_app_name));
		pr_info("qseecom_mobile: app '%s' already loaded with id %u\n", app_name, *app_id);
		return 0;
	}

	snprintf(fw_name, sizeof(fw_name), "%s.mdt", app_name);
	ret = request_firmware(&mdt, fw_name, dev);
	if (ret) {
		snprintf(fw_name, sizeof(fw_name), "qcom/sm7225/fairphone4/%s.mdt", app_name);
		ret = request_firmware(&mdt, fw_name, dev);
	}
	if (ret) {
		pr_err("qseecom_mobile: failed to request firmware for '%s': %d\n", app_name, ret);
		return ret;
	}

	if (mdt->size < 16 || memcmp(mdt->data, ELFMAG, SELFMAG) != 0) {
		pr_err("qseecom_mobile: invalid ELF header in '%s'\n", app_name);
		release_firmware(mdt);
		return -EINVAL;
	}

	if (mdt->data[EI_CLASS] == ELFCLASS32) {
		struct elf32_hdr *ehdr = (struct elf32_hdr *)mdt->data;
		num_segments = ehdr->e_phnum;
	} else if (mdt->data[EI_CLASS] == ELFCLASS64) {
		struct elf64_hdr *ehdr = (struct elf64_hdr *)mdt->data;
		num_segments = ehdr->e_phnum;
	} else {
		pr_err("qseecom_mobile: unsupported ELF class %u in '%s'\n", mdt->data[EI_CLASS], app_name);
		release_firmware(mdt);
		return -EINVAL;
	}

	mdt_len = mdt->size;
	total_len = mdt_len;

	/* Pre-check segments and compute total buffer size */
	for (i = 0; i < num_segments; i++) {
		snprintf(fw_name, sizeof(fw_name), "%s.b%02d", app_name, i);
		ret = request_firmware(&seg_fw, fw_name, dev);
		if (ret) {
			snprintf(fw_name, sizeof(fw_name), "qcom/sm7225/fairphone4/%s.b%02d", app_name, i);
			ret = request_firmware(&seg_fw, fw_name, dev);
		}
		if (ret) {
			pr_err("qseecom_mobile: failed to locate segment %s.b%02d: %d\n", app_name, i, ret);
			release_firmware(mdt);
			return ret;
		}
		total_len += seg_fw->size;
		release_firmware(seg_fw);
	}

	pr_info("qseecom_mobile: loading '%s': mdt_len=%zu total_len=%zu segments=%u\n",
		app_name, mdt_len, total_len, num_segments);

	fw_buf = qcom_tzmem_alloc(qd->mempool, total_len, GFP_KERNEL);
	if (!fw_buf) {
		release_firmware(mdt);
		return -ENOMEM;
	}

	fw_phys = qcom_tzmem_to_phys(fw_buf);
	buf_ptr = (u8 *)fw_buf;

	/* Copy MDT header */
	memcpy(buf_ptr, mdt->data, mdt_len);
	buf_ptr += mdt_len;
	release_firmware(mdt);

	/* Copy each segment into contiguous TZMEM buffer */
	for (i = 0; i < num_segments; i++) {
		snprintf(fw_name, sizeof(fw_name), "%s.b%02d", app_name, i);
		ret = request_firmware(&seg_fw, fw_name, dev);
		if (ret) {
			snprintf(fw_name, sizeof(fw_name), "qcom/sm7225/fairphone4/%s.b%02d", app_name, i);
			ret = request_firmware(&seg_fw, fw_name, dev);
		}
		if (ret) {
			pr_err("qseecom_mobile: failed to reload segment %s.b%02d: %d\n", app_name, i, ret);
			qcom_tzmem_free(fw_buf);
			return ret;
		}
		memcpy(buf_ptr, seg_fw->data, seg_fw->size);
		buf_ptr += seg_fw->size;
		release_firmware(seg_fw);
	}

	desc.owner = QSEECOM_TZ_OWNER_QSEE_OS;
	desc.svc = QSEECOM_TZ_SVC_APP_MGR;
	desc.cmd = QSEECOM_TZ_CMD_APP_START;
	desc.arginfo = QCOM_SCM_ARGS(3, QCOM_SCM_VAL, QCOM_SCM_VAL, QCOM_SCM_VAL);
	desc.args[0] = mdt_len;
	desc.args[1] = total_len;
	desc.args[2] = fw_phys;

	mutex_lock(&qd->smc_lock);
	ret = qsee_smc_call(qd, &desc, &res);
	if (!ret && res.result == QSEECOM_RESULT_INCOMPLETE)
		ret = qsee_service_listeners(qd, &res);
	mutex_unlock(&qd->smc_lock);

	qcom_tzmem_free(fw_buf);

	if (ret) {
		pr_err("qseecom_mobile: SMC call to start '%s' failed: %d\n", app_name, ret);
		return ret;
	}

	if (res.result != QSEECOM_RESULT_SUCCESS || res.resp_type != QSEECOM_SCM_RES_APP_ID) {
		pr_err("qseecom_mobile: start '%s' returned result=0x%llx type=0x%llx data=0x%llx\n",
		       app_name, res.result, res.resp_type, res.data);
		return -EIO;
	}

	*app_id = res.data;
	qd->loaded_app_id = *app_id;
	strscpy(qd->loaded_app_name, app_name, sizeof(qd->loaded_app_name));
	pr_info("qseecom_mobile: successfully loaded '%s' with id %u\n", app_name, *app_id);
	return 0;
}

/**
 * qsee_app_shutdown() - Unload a running TrustZone application by ID
 * @qd:     Driver instance.
 * @app_id: Numeric TA identifier returned by qsee_app_load().
 *
 * Return: 0 on success, negative errno on failure.
 */
static int qsee_app_shutdown(struct qsee_mobile_dev *qd, u32 app_id)
{
	struct qsee_scm_desc desc = {};
	struct qsee_scm_resp res = {};
	int ret;

	if (qd->loaded_app_id == app_id) {
		qd->loaded_app_id = 0;
		qd->loaded_app_name[0] = '\0';
	}

	desc.owner = QSEECOM_TZ_OWNER_QSEE_OS;
	desc.svc = QSEECOM_TZ_SVC_APP_MGR;
	desc.cmd = QSEECOM_TZ_CMD_APP_SHUTDOWN;
	desc.arginfo = QCOM_SCM_ARGS(1, QCOM_SCM_VAL);
	desc.args[0] = app_id;

	mutex_lock(&qd->smc_lock);
	ret = qsee_smc_call(qd, &desc, &res);
	mutex_unlock(&qd->smc_lock);

	if (ret)
		return ret;

	if (res.result != QSEECOM_RESULT_SUCCESS)
		return -EIO;

	return 0;
}

/**
 * qsee_listener_register() - Register a shared bounce buffer for a listener ID
 * @qd:     Driver instance.
 * @id:     Listener service ID (e.g. 10 for FS, 28672 for GPFS).
 * @sb:     Virtual address of TZMEM shared buffer.
 * @sb_len: Length of shared buffer in bytes.
 *
 * Return: 0 on success, negative errno on SMC or registration failure.
 */
static int qsee_listener_register(struct qsee_mobile_dev *qd, u32 id, void *sb, size_t sb_len)
{
	struct qsee_scm_desc desc = {};
	struct qsee_scm_resp res = {};
	phys_addr_t sb_phys;
	int ret;

	sb_phys = qcom_tzmem_to_phys(sb);
	if (!sb_phys)
		return -EINVAL;

	desc.owner = QSEECOM_TZ_OWNER_QSEE_OS;
	desc.svc = QSEECOM_TZ_SVC_LISTENER;
	desc.cmd = QSEECOM_TZ_CMD_LISTENER_REGISTER;
	desc.arginfo = QCOM_SCM_ARGS(3, QCOM_SCM_VAL, QCOM_SCM_RW, QCOM_SCM_VAL);
	desc.args[0] = id;
	desc.args[1] = sb_phys;
	desc.args[2] = sb_len;

	mutex_lock(&qd->smc_lock);
	ret = qsee_smc_call(qd, &desc, &res);
	mutex_unlock(&qd->smc_lock);

	if (ret) {
		pr_err("qseecom_mobile: listener_register(%u) SMC failed: %d\n", id, ret);
		return ret;
	}

	if (res.result != QSEECOM_RESULT_SUCCESS) {
		pr_err("qseecom_mobile: listener_register(%u) TZ returned result=0x%llx type=0x%llx data=0x%llx\n",
		       id, res.result, res.resp_type, res.data);
		return -EIO;
	}

	pr_info("qseecom_mobile: registered listener %u (sb_phys=0x%llx len=%zu)\n",
		id, (u64)sb_phys, sb_len);
	return 0;
}

/**
 * qsee_listener_deregister() - Unregister a listener service ID from QSEE OS
 * @qd: Driver instance.
 * @id: Listener service ID to deregister.
 *
 * Return: 0 on success, negative errno on failure.
 */
static int qsee_listener_deregister(struct qsee_mobile_dev *qd, u32 id)
{
	struct qsee_scm_desc desc = {};
	struct qsee_scm_resp res = {};
	int ret;

	desc.owner = QSEECOM_TZ_OWNER_QSEE_OS;
	desc.svc = QSEECOM_TZ_SVC_LISTENER;
	desc.cmd = QSEECOM_TZ_CMD_LISTENER_DEREGISTER;
	desc.arginfo = QCOM_SCM_ARGS(1, QCOM_SCM_VAL);
	desc.args[0] = id;

	mutex_lock(&qd->smc_lock);
	ret = qsee_smc_call(qd, &desc, &res);
	mutex_unlock(&qd->smc_lock);

	if (ret)
		return ret;

	if (res.result != QSEECOM_RESULT_SUCCESS)
		return -EIO;

	pr_info("qseecom_mobile: deregistered listener %u\n", id);
	return 0;
}

/**
 * qsee_app_send() - Send a synchronous command to a TA and service listeners
 * @qd:       Driver instance.
 * @app_id:   Target TA identifier.
 * @req:      Virtual address of TZMEM request buffer.
 * @req_size: Size of request payload in bytes.
 * @rsp:      Virtual address of TZMEM response buffer.
 * @rsp_size: Capacity of response buffer in bytes.
 *
 * Context: Must be called with qd->smc_lock held so req_buf, rsp_buf, and the
 * multi-round listener state machine are serialized against concurrent threads.
 * Return: 0 on success, negative errno on SMC or TA error.
 */
static int qsee_app_send(struct qsee_mobile_dev *qd, u32 app_id,
			 void *req, size_t req_size,
			 void *rsp, size_t rsp_size)
{
	struct qsee_scm_desc desc = {};
	struct qsee_scm_resp res = {};
	size_t actual_req_size = max_t(size_t, req_size, 256);
	size_t actual_rsp_size = max_t(size_t, rsp_size, QSEE_MAX_RSP_SIZE);
	int ret;

	pr_debug("qseecom_mobile: app_send app_id=%u req_size=%zu (actual=%zu) rsp_size=%zu (actual=%zu)\n",
		 app_id, req_size, actual_req_size, rsp_size, actual_rsp_size);

	/* Standard QSEE app send (Owner 48, Service 0, Command 1) */
	desc.owner = QSEECOM_TZ_OWNER_TZ_APPS;
	desc.svc = QSEECOM_TZ_SVC_APP_ID_PLACEHOLDER;
	desc.cmd = QSEECOM_TZ_CMD_APP_SEND;
	desc.arginfo = QCOM_SCM_ARGS(5, QCOM_SCM_VAL,
				     QCOM_SCM_RW, QCOM_SCM_VAL,
				     QCOM_SCM_RW, QCOM_SCM_VAL);
	desc.args[0] = app_id;
	desc.args[1] = qcom_tzmem_to_phys(req);
	desc.args[2] = actual_req_size;
	desc.args[3] = qcom_tzmem_to_phys(rsp);
	desc.args[4] = actual_rsp_size;

	ret = qsee_smc_call(qd, &desc, &res);
	if (!ret && res.result == QSEECOM_RESULT_INCOMPLETE)
		ret = qsee_service_listeners(qd, &res);

	if (ret)
		return ret;

	if (res.result != QSEECOM_RESULT_SUCCESS)
		return -EIO;

	return 0;
}

/**
 * qsee_mobile_ioctl() - Character device ioctl dispatcher for /dev/qseecom_mobile
 * @file: Open file pointer.
 * @cmd:  Ioctl command code (QSEECOM_MOBILE_IOC_*).
 * @arg:  Userspace pointer argument.
 *
 * Return: 0 on success, negative errno on failure.
 */
static long qsee_mobile_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	int ret = 0;

	switch (cmd) {
	case QSEECOM_MOBILE_IOC_LOAD_APP: {
		struct qseecom_mobile_load_app req;
		char name[64];

		if (copy_from_user(&req, argp, sizeof(req)))
			return -EFAULT;

		req.app_name[sizeof(req.app_name) - 1] = '\0';
		strscpy(name, req.app_name, sizeof(name));

		pr_info("qseecom_mobile: IOC_LOAD_APP '%s'\n", name);
		ret = qsee_app_load(qdev, qdev->miscdev.this_device, name, &req.app_id);
		if (ret) {
			pr_err("qseecom_mobile: qsee_app_load('%s') failed: %d\n", name, ret);
			return ret;
		}

		if (copy_to_user(argp, &req, sizeof(req)))
			return -EFAULT;
		break;
	}

	case QSEECOM_MOBILE_IOC_UNLOAD_APP: {
		__u32 app_id;

		if (copy_from_user(&app_id, argp, sizeof(app_id)))
			return -EFAULT;

		ret = qsee_app_shutdown(qdev, app_id);
		break;
	}

	case QSEECOM_MOBILE_IOC_SEND_CMD: {
		struct qseecom_mobile_send_cmd req;

		if (copy_from_user(&req, argp, sizeof(req)))
			return -EFAULT;

		if (req.req_len > QSEE_MAX_REQ_SIZE || req.rsp_len > QSEE_MAX_RSP_SIZE)
			return -EINVAL;

		mutex_lock(&qdev->smc_lock);

		memset(qdev->req_buf, 0, QSEE_MAX_REQ_SIZE);
		if (copy_from_user(qdev->req_buf, (void __user *)req.req_ptr, req.req_len)) {
			mutex_unlock(&qdev->smc_lock);
			return -EFAULT;
		}

		memset(qdev->rsp_buf, 0, QSEE_MAX_RSP_SIZE);

		ret = qsee_app_send(qdev, req.app_id,
				    qdev->req_buf, req.req_len,
				    qdev->rsp_buf, req.rsp_len);
		if (ret) {
			mutex_unlock(&qdev->smc_lock);
			return ret;
		}

		if (copy_to_user((void __user *)req.rsp_ptr, qdev->rsp_buf, req.rsp_len)) {
			mutex_unlock(&qdev->smc_lock);
			return -EFAULT;
		}

		mutex_unlock(&qdev->smc_lock);
		break;
	}

	case QSEECOM_MOBILE_IOC_SEND_MODIFIED_CMD: {
		struct qseecom_mobile_send_mod_cmd req;
		void *sb_virt;
		phys_addr_t sb_phys;
		u64 phys64;

		if (copy_from_user(&req, argp, sizeof(req)))
			return -EFAULT;

		if (req.req_len == 0 || req.req_len > QSEE_MAX_REQ_SIZE ||
		    req.rsp_len == 0 || req.rsp_len > QSEE_MAX_RSP_SIZE ||
		    req.sb_len == 0 || req.sb_len > SZ_1M ||
		    req.cmd_buf_off + sizeof(u64) > req.req_len)
			return -EINVAL;

		sb_virt = qcom_tzmem_alloc(qdev->mempool, PAGE_ALIGN(req.sb_len), GFP_KERNEL);
		if (!sb_virt)
			return -ENOMEM;

		if (copy_from_user(sb_virt, (void __user *)req.sb_ptr, req.sb_len)) {
			qcom_tzmem_free(sb_virt);
			return -EFAULT;
		}
		sb_phys = qcom_tzmem_to_phys(sb_virt);
		phys64 = (u64)sb_phys;

		mutex_lock(&qdev->smc_lock);

		memset(qdev->req_buf, 0, QSEE_MAX_REQ_SIZE);
		if (copy_from_user(qdev->req_buf, (void __user *)req.req_ptr, req.req_len)) {
			mutex_unlock(&qdev->smc_lock);
			qcom_tzmem_free(sb_virt);
			return -EFAULT;
		}
		memcpy((u8 *)qdev->req_buf + req.cmd_buf_off, &phys64, sizeof(phys64));
		memset(qdev->rsp_buf, 0, QSEE_MAX_RSP_SIZE);

		pr_info("qseecom_mobile: mod_cmd app=%u sb_phys=0x%llx sb_len=%u off=%u\n",
			req.app_id, phys64, req.sb_len, req.cmd_buf_off);
		ret = qsee_app_send(qdev, req.app_id,
				    qdev->req_buf, req.req_len,
				    qdev->rsp_buf, req.rsp_len);
		if (!ret) {
			if (copy_to_user((void __user *)req.rsp_ptr, qdev->rsp_buf, req.rsp_len) ||
			    copy_to_user((void __user *)req.sb_ptr, sb_virt, req.sb_len))
				ret = -EFAULT;
		}

		mutex_unlock(&qdev->smc_lock);
		qcom_tzmem_free(sb_virt);
		break;
	}

	case QSEECOM_MOBILE_IOC_REGISTER_LISTENER: {
		struct qseecom_mobile_reg_listener req;
		struct qsee_mobile_listener *listener;

		if (copy_from_user(&req, argp, sizeof(req)))
			return -EFAULT;

		if (req.sb_size > SZ_1M || req.sb_size == 0)
			return -EINVAL;

		mutex_lock(&qdev->listener_lock);
		if (qsee_find_listener(qdev, req.listener_id)) {
			mutex_unlock(&qdev->listener_lock);
			return -EBUSY;
		}

		listener = kzalloc(sizeof(*listener), GFP_KERNEL);
		if (!listener) {
			mutex_unlock(&qdev->listener_lock);
			return -ENOMEM;
		}

		listener->id = req.listener_id;
		listener->sb_len = req.sb_size;
		listener->sb_virt = qcom_tzmem_alloc(qdev->mempool, req.sb_size, GFP_KERNEL);
		if (!listener->sb_virt) {
			kfree(listener);
			mutex_unlock(&qdev->listener_lock);
			return -ENOMEM;
		}

		kref_init(&listener->ref);
		init_waitqueue_head(&listener->req_wq);
		init_waitqueue_head(&listener->resp_wq);

		ret = qsee_listener_register(qdev, listener->id, listener->sb_virt, listener->sb_len);
		if (ret) {
			qcom_tzmem_free(listener->sb_virt);
			kfree(listener);
			mutex_unlock(&qdev->listener_lock);
			return ret;
		}

		list_add_tail(&listener->node, &qdev->listeners);
		mutex_unlock(&qdev->listener_lock);
		break;
	}

	case QSEECOM_MOBILE_IOC_UNREGISTER_LISTENER: {
		__u32 listener_id;
		struct qsee_mobile_listener *listener;

		if (copy_from_user(&listener_id, argp, sizeof(listener_id)))
			return -EFAULT;

		mutex_lock(&qdev->listener_lock);
		listener = qsee_find_listener(qdev, listener_id);
		if (!listener) {
			mutex_unlock(&qdev->listener_lock);
			return -ENOENT;
		}

		list_del(&listener->node);
		listener->dead = true;
		wake_up_all(&listener->req_wq);
		wake_up_all(&listener->resp_wq);
		mutex_unlock(&qdev->listener_lock);

		qsee_listener_deregister(qdev, listener->id);
		kref_put(&listener->ref, qsee_listener_free);
		break;
	}

	case QSEECOM_MOBILE_IOC_WAIT_LISTENER: {
		struct qseecom_mobile_listener_req req;
		struct qsee_mobile_listener *listener;

		if (copy_from_user(&req, argp, sizeof(req)))
			return -EFAULT;

		mutex_lock(&qdev->listener_lock);
		listener = qsee_find_listener(qdev, req.listener_id);
		if (!listener) {
			mutex_unlock(&qdev->listener_lock);
			return -ENOENT;
		}
		kref_get(&listener->ref);
		mutex_unlock(&qdev->listener_lock);

		ret = wait_event_interruptible(listener->req_wq, listener->pending || listener->dead);
		if (ret) {
			kref_put(&listener->ref, qsee_listener_free);
			return ret;
		}

		if (listener->dead) {
			kref_put(&listener->ref, qsee_listener_free);
			return -ENODEV;
		}

		listener->pending = false;

		if (req.buf_ptr && req.buf_len) {
			size_t to_copy = min_t(size_t, req.buf_len, listener->sb_len);
			if (copy_to_user((void __user *)req.buf_ptr, listener->sb_virt, to_copy)) {
				kref_put(&listener->ref, qsee_listener_free);
				return -EFAULT;
			}
		}
		kref_put(&listener->ref, qsee_listener_free);
		break;
	}

	case QSEECOM_MOBILE_IOC_RESPOND_LISTENER: {
		struct qseecom_mobile_listener_resp req;
		struct qsee_mobile_listener *listener;

		if (copy_from_user(&req, argp, sizeof(req)))
			return -EFAULT;

		mutex_lock(&qdev->listener_lock);
		listener = qsee_find_listener(qdev, req.listener_id);
		mutex_unlock(&qdev->listener_lock);
		if (!listener)
			return -ENOENT;

		if (req.buf_ptr && req.buf_len) {
			size_t to_copy = min_t(size_t, req.buf_len, listener->sb_len);
			if (copy_from_user(listener->sb_virt, (void __user *)req.buf_ptr, to_copy))
				return -EFAULT;
		}

		listener->resp_status = req.status;
		listener->answered = true;
		wake_up_interruptible(&listener->resp_wq);
		break;
	}

	default:
		return -ENOTTY;
	}

	return ret;
}

/**
 * qsee_mobile_release() - Clean up listeners and loaded TAs when fd is closed
 * @inode: Character device inode.
 * @file:  Closed file pointer.
 *
 * Return: 0.
 */
static int qsee_mobile_release(struct inode *inode, struct file *file)
{
	struct qsee_mobile_listener *listener, *tmp;

	pr_info("qseecom_mobile: device closed\n");

	if (qdev->loaded_app_id) {
		pr_info("qseecom_mobile: auto-unloading app %u ('%s') on close\n",
			qdev->loaded_app_id, qdev->loaded_app_name);
		qsee_app_shutdown(qdev, qdev->loaded_app_id);
	}

	mutex_lock(&qdev->listener_lock);
	list_for_each_entry_safe(listener, tmp, &qdev->listeners, node) {
		list_del(&listener->node);
		listener->dead = true;
		wake_up_all(&listener->req_wq);
		wake_up_all(&listener->resp_wq);
		qsee_listener_deregister(qdev, listener->id);
		kref_put(&listener->ref, qsee_listener_free);
	}
	mutex_unlock(&qdev->listener_lock);

	return 0;
}

static const struct file_operations qsee_mobile_fops = {
	.owner          = THIS_MODULE,
	.release        = qsee_mobile_release,
	.unlocked_ioctl = qsee_mobile_ioctl,
	.compat_ioctl   = compat_ptr_ioctl,
};

/**
 * qsee_mobile_init() - Initialize TZMEM pool and register /dev/qseecom_mobile
 *
 * Return: 0 on success, negative errno on failure.
 */
static int __init qsee_mobile_init(void)
{
	struct qcom_tzmem_pool_config pool_config = {
		.initial_size = SZ_2M,
		.policy = QCOM_TZMEM_POLICY_MULTIPLIER,
		.increment = 2,
		.max_size = SZ_32M,
	};
	int ret;

	qdev = kzalloc(sizeof(*qdev), GFP_KERNEL);
	if (!qdev)
		return -ENOMEM;

	qsee_resolve_scm_symbols();

	mutex_init(&qdev->smc_lock);
	mutex_init(&qdev->listener_lock);
	INIT_LIST_HEAD(&qdev->listeners);

	qdev->mempool = qcom_tzmem_pool_new(&pool_config);
	if (IS_ERR(qdev->mempool)) {
		pr_err("qseecom_mobile: failed to create TZ memory pool: %ld\n", PTR_ERR(qdev->mempool));
		ret = PTR_ERR(qdev->mempool);
		goto err_free_qdev;
	}

	qdev->req_buf = qcom_tzmem_alloc(qdev->mempool, QSEE_MAX_REQ_SIZE, GFP_KERNEL);
	if (!qdev->req_buf) {
		ret = -ENOMEM;
		goto err_free_pool;
	}

	qdev->rsp_buf = qcom_tzmem_alloc(qdev->mempool, QSEE_MAX_RSP_SIZE, GFP_KERNEL);
	if (!qdev->rsp_buf) {
		ret = -ENOMEM;
		goto err_free_req;
	}

	qdev->sglist_buf = qcom_tzmem_alloc(qdev->mempool, 64, GFP_KERNEL);
	if (!qdev->sglist_buf) {
		ret = -ENOMEM;
		goto err_free_rsp;
	}
	memset(qdev->sglist_buf, 0, 64);
	qdev->sglist_phys = qcom_tzmem_to_phys(qdev->sglist_buf);

	qdev->miscdev.minor = MISC_DYNAMIC_MINOR;
	qdev->miscdev.name  = QSEECOM_MOBILE_DEV_NAME;
	qdev->miscdev.fops  = &qsee_mobile_fops;
	qdev->miscdev.mode  = 0660;

	ret = misc_register(&qdev->miscdev);
	if (ret) {
		pr_err("qseecom_mobile: failed to register misc device: %d\n", ret);
		goto err_free_sglist;
	}

	pr_info("qseecom_mobile: initialized /dev/%s\n", QSEECOM_MOBILE_DEV_NAME);
	return 0;

err_free_sglist:
	qcom_tzmem_free(qdev->sglist_buf);
err_free_rsp:
	qcom_tzmem_free(qdev->rsp_buf);
err_free_req:
	qcom_tzmem_free(qdev->req_buf);
err_free_pool:
	qcom_tzmem_pool_free(qdev->mempool);
err_free_qdev:
	kfree(qdev);
	return ret;
}

/**
 * qsee_mobile_exit() - Deregister /dev/qseecom_mobile and free TZMEM resources
 */
static void __exit qsee_mobile_exit(void)
{
	struct qsee_mobile_listener *listener, *tmp;

	if (qdev->loaded_app_id)
		qsee_app_shutdown(qdev, qdev->loaded_app_id);

	misc_deregister(&qdev->miscdev);

	mutex_lock(&qdev->listener_lock);
	list_for_each_entry_safe(listener, tmp, &qdev->listeners, node) {
		list_del(&listener->node);
		listener->dead = true;
		wake_up_all(&listener->req_wq);
		wake_up_all(&listener->resp_wq);
		qsee_listener_deregister(qdev, listener->id);
		kref_put(&listener->ref, qsee_listener_free);
	}
	mutex_unlock(&qdev->listener_lock);

	qcom_tzmem_free(qdev->sglist_buf);
	qcom_tzmem_free(qdev->rsp_buf);
	qcom_tzmem_free(qdev->req_buf);
	qcom_tzmem_pool_free(qdev->mempool);
	kfree(qdev);
	pr_info("qseecom_mobile: unloaded\n");
}

module_init(qsee_mobile_init);
module_exit(qsee_mobile_exit);

MODULE_AUTHOR("Nitescu Ion Laurentiu <contact@nyl.ro>");
MODULE_DESCRIPTION("Qualcomm Mobile QSEECOM Interface Driver (/dev/qseecom_mobile)");
MODULE_LICENSE("GPL");

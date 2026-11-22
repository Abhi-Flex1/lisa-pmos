/* SPDX-License-Identifier: GPL-2.0-or-later WITH Linux-syscall-note */
/*
 * Qualcomm Mobile QSEECOM Interface Driver (/dev/qseecom_mobile) User API
 *
 * Designed specifically for Qualcomm TrustZone mobile Trusted Applications
 * (such as fingerprint sensors) with proper support for synchronous command
 * execution and userspace listener dispatching (SFS/GPFS template storage).
 *
 * Copyright (C) 2026 Nitescu Ion Laurentiu <contact@nyl.ro>
 */

#ifndef _UAPI_LINUX_QSEECOM_MOBILE_H
#define _UAPI_LINUX_QSEECOM_MOBILE_H

#include <linux/ioctl.h>
#include <linux/types.h>

#define QSEECOM_MOBILE_DEV_NAME		"qseecom_mobile"
#define QSEECOM_MOBILE_IOC_MAGIC	'Q'

/**
 * struct qseecom_mobile_load_app - Load a QSEE application by name.
 * @app_name: ASCII string naming the application in /lib/firmware/.
 * @app_id:   Output: numeric ID allocated to the app by TrustZone.
 */
struct qseecom_mobile_load_app {
	char   app_name[64];
	__u32  app_id;
};

/**
 * struct qseecom_mobile_send_cmd - Send request to TA and receive response.
 * @app_id:   ID of the target application returned by LOAD_APP.
 * @req_ptr:  Userspace pointer to request payload to be copied into TZ memory.
 * @req_len:  Size of request buffer in bytes.
 * @rsp_ptr:  Userspace pointer to buffer receiving TA response.
 * @rsp_len:  Capacity of response buffer in bytes.
 */
struct qseecom_mobile_send_cmd {
	__u32  app_id;
	__u32  req_len;
	__u64  req_ptr;
	__u32  rsp_len;
	__u64  rsp_ptr;
};

/**
 * struct qseecom_mobile_send_mod_cmd - Send command with a TZMEM shared buffer.
 * @app_id:        ID of the target application returned by LOAD_APP.
 * @req_len:       Size of request buffer in bytes.
 * @req_ptr:       Userspace pointer to request payload.
 * @rsp_len:       Capacity of response buffer in bytes.
 * @rsp_ptr:       Userspace pointer to response payload.
 * @sb_len:        Size of auxiliary TZMEM shared buffer in bytes.
 * @sb_ptr:        Userspace pointer to auxiliary buffer (copied in & out).
 * @cmd_buf_off:   Byte offset inside req_buf where 64-bit phys addr of sb is patched.
 */
struct qseecom_mobile_send_mod_cmd {
	__u32  app_id;
	__u32  req_len;
	__u64  req_ptr;
	__u32  rsp_len;
	__u64  rsp_ptr;
	__u32  sb_len;
	__u32  cmd_buf_off;
	__u64  sb_ptr;
};

/**
 * struct qseecom_mobile_reg_listener - Register a listener service ID.
 * @listener_id: Numeric ID of listener (e.g. 28672 for GPFS, 10 for FS).
 * @sb_size:     Shared buffer size required by the listener in bytes.
 */
struct qseecom_mobile_reg_listener {
	__u32  listener_id;
	__u32  sb_size;
};

/**
 * struct qseecom_mobile_listener_req - Dequeue incoming request from TA.
 * @listener_id: Target listener ID to wait on.
 * @buf_ptr:     Userspace pointer to receive the incoming request payload.
 * @buf_len:     In: buffer capacity; Out: actual bytes received.
 */
struct qseecom_mobile_listener_req {
	__u32  listener_id;
	__u32  buf_len;
	__u64  buf_ptr;
};

/**
 * struct qseecom_mobile_listener_resp - Send completed answer back to TA.
 * @listener_id: Target listener ID.
 * @status:      0 on success, negative/positive failure code.
 * @buf_ptr:     Userspace pointer containing response payload.
 * @buf_len:     Length of response payload.
 */
struct qseecom_mobile_listener_resp {
	__u32  listener_id;
	__s32  status;
	__u32  buf_len;
	__u64  buf_ptr;
};

#define QSEECOM_MOBILE_IOC_LOAD_APP \
	_IOWR(QSEECOM_MOBILE_IOC_MAGIC, 1, struct qseecom_mobile_load_app)

#define QSEECOM_MOBILE_IOC_UNLOAD_APP \
	_IOW(QSEECOM_MOBILE_IOC_MAGIC, 2, __u32)

#define QSEECOM_MOBILE_IOC_SEND_CMD \
	_IOWR(QSEECOM_MOBILE_IOC_MAGIC, 3, struct qseecom_mobile_send_cmd)

#define QSEECOM_MOBILE_IOC_REGISTER_LISTENER \
	_IOW(QSEECOM_MOBILE_IOC_MAGIC, 4, struct qseecom_mobile_reg_listener)

#define QSEECOM_MOBILE_IOC_UNREGISTER_LISTENER \
	_IOW(QSEECOM_MOBILE_IOC_MAGIC, 5, __u32)

#define QSEECOM_MOBILE_IOC_WAIT_LISTENER \
	_IOWR(QSEECOM_MOBILE_IOC_MAGIC, 6, struct qseecom_mobile_listener_req)

#define QSEECOM_MOBILE_IOC_RESPOND_LISTENER \
	_IOW(QSEECOM_MOBILE_IOC_MAGIC, 7, struct qseecom_mobile_listener_resp)

#define QSEECOM_MOBILE_IOC_SEND_MODIFIED_CMD \
	_IOWR(QSEECOM_MOBILE_IOC_MAGIC, 8, struct qseecom_mobile_send_mod_cmd)

#endif /* _UAPI_LINUX_QSEECOM_MOBILE_H */


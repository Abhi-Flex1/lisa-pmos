// SPDX-License-Identifier: GPL-2.0-only
/*
 * Input force-feedback driver for the Awinic AW8624 LRA haptic controller.
 *
 * Supports both HyperOS/MIUI SRAM waveform playback (aw8624_haptic.bin) for
 * crisp iOS-like Taptic clicks/taps and CONT mode for long continuous rumble,
 * tuned for Xiaomi 11 Lite 5G NE (lisa).
 */

#include <linux/delay.h>
#include <linux/firmware.h>
#include <linux/gpio/consumer.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/regmap.h>
#include <linux/workqueue.h>

#define AW8624_REG_ID			0x00
#define AW8624_REG_SYSINTM		0x03
#define AW8624_REG_SYSCTRL		0x04
#define AW8624_REG_GO			0x05
#define AW8624_REG_WAVSEQ1		0x07
#define AW8624_REG_WAVSEQ2		0x08
#define AW8624_REG_WAVLOOP1		0x0f
#define AW8624_REG_MAIN_LOOP		0x13
#define AW8624_REG_BASE_ADDRH		0x21
#define AW8624_REG_BASE_ADDRL		0x22
#define AW8624_REG_FIFO_AEH		0x23
#define AW8624_REG_FIFO_AEL		0x24
#define AW8624_REG_FIFO_AFH		0x25
#define AW8624_REG_FIFO_AFL		0x26
#define AW8624_REG_DATCTRL		0x2b
#define AW8624_REG_PWMDBG		0x2e
#define AW8624_REG_BRAKE0_CTRL		0x32
#define AW8624_REG_BRAKE1_CTRL		0x33
#define AW8624_REG_BRAKE2_CTRL		0x34
#define AW8624_REG_BRAKE_NUM		0x35
#define AW8624_REG_SW_BRAKE		0x39
#define AW8624_REG_DATDBG		0x3b
#define AW8624_REG_PRLVL		0x3e
#define AW8624_REG_PRTIME		0x3f
#define AW8624_REG_RAMADDRH		0x40
#define AW8624_REG_RAMADDRL		0x41
#define AW8624_REG_RAMDATA		0x42
#define AW8624_REG_GLB_STATE		0x47
#define AW8624_REG_CONT_CTRL		0x48
#define AW8624_REG_F_PRE_H		0x49
#define AW8624_REG_F_PRE_L		0x4a
#define AW8624_REG_TD_H			0x4b
#define AW8624_REG_TD_L			0x4c
#define AW8624_REG_TSET			0x4d
#define AW8624_REG_R_SPARE		0x5d
#define AW8624_REG_DETCTRL		0x5f
#define AW8624_REG_ZC_THRSH_H		0x72
#define AW8624_REG_ZC_THRSH_L		0x73
#define AW8624_REG_BEMF_NUM		0x78
#define AW8624_REG_TIME_NZC		0x7a
#define AW8624_REG_DRV_LVL		0x7b
#define AW8624_REG_DRV_LVL_OV		0x7c

#define AW8624_CHIP_ID			0x24
#define AW8624_SOFTRST_VAL		0xaa

#define AW8624_SYSCTRL_RAMINIT_MASK	BIT(5)
#define AW8624_SYSCTRL_RAMINIT_EN	BIT(5)
#define AW8624_SYSCTRL_RAMINIT_OFF	0
#define AW8624_SYSCTRL_PLAY_MODE_MASK	GENMASK(3, 2)
#define AW8624_SYSCTRL_PLAY_MODE_RAM	(0 << 2)
#define AW8624_SYSCTRL_PLAY_MODE_CONT	(2 << 2)
#define AW8624_SYSCTRL_WORK_MODE_MASK	BIT(0)
#define AW8624_SYSCTRL_STANDBY		BIT(0)
#define AW8624_SYSCTRL_ACTIVE		0

#define AW8624_GO_MASK			BIT(0)
#define AW8624_GO_ENABLE		BIT(0)
#define AW8624_GO_DISABLE		0

#define AW8624_DATCTRL_FC_MASK		GENMASK(7, 6)
#define AW8624_DATCTRL_FC_1000HZ	(3 << 6)
#define AW8624_DATCTRL_LPF_EN		BIT(5)

#define AW8624_PWMDBG_PWM_MASK		GENMASK(6, 5)
#define AW8624_PWMDBG_PWM_24K		(2 << 5)

#define AW8624_CONT_CTRL_ZC_EN		BIT(7)
#define AW8624_CONT_CTRL_WAIT_1P	(0 << 5)
#define AW8624_CONT_CTRL_BY_GO		(0 << 4)
#define AW8624_CONT_CTRL_CLOSE_PB	BIT(3)
#define AW8624_CONT_CTRL_F0_DIS		(0 << 2)
#define AW8624_CONT_CTRL_O2C_DIS	(0 << 1)
#define AW8624_CONT_CTRL_AUTO_BRK_EN	BIT(0)

#define AW8624_DETCTRL_VBAT_HW_COMP	BIT(6)

#define AW8624_RAM_FW_NAME		"aw8624_haptic.bin"
#define AW8624_RAM_CHUNK_SIZE		64
#define AW8624_RAM_MAX_DURATION_MS	150
#define AW8624_MAX_EFFECTS		16

/* Default LRA parameters for Xiaomi 11 Lite 5G NE (from vendor DTBO) */
#define AW8624_DEFAULT_F0_PRE		2050
#define AW8624_DEFAULT_F0_COEFF		260
#define AW8624_DEFAULT_DRV_LVL		0x6a
#define AW8624_DEFAULT_DRV_LVL_OV	0x9b
#define AW8624_DEFAULT_CONT_TD		0xf06c
#define AW8624_DEFAULT_ZC_THR		0x08f8
#define AW8624_DEFAULT_NUM_BRK		0x03
#define AW8624_DEFAULT_TSET		0x11
#define AW8624_DEFAULT_R_SPARE		0x68
#define AW8624_DEFAULT_SW_BRAKE		0x2c

struct aw8624_effect {
	u16 magnitude;
	u16 duration_ms;
};

struct aw8624_data {
	struct i2c_client *client;
	struct device *dev;
	struct regmap *regmap;
	struct gpio_desc *reset_gpio;
	struct input_dev *input_dev;
	struct work_struct play_work;
	struct delayed_work stop_work;
	struct mutex lock;
	struct aw8624_effect effects[AW8624_MAX_EFFECTS];
	bool ram_loaded;
	bool cont_active;
	u16 magnitude;
	u16 duration_ms;
	u32 f0_pre;
	u32 f0_coeff;
	u32 drv_lvl_max;
	u32 drv_lvl_ov;
	u32 cont_td;
	u32 zc_thr;
	u32 num_brk;
};

static const struct regmap_config aw8624_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = 0x7f,
	.cache_type = REGCACHE_NONE,
};

static void aw8624_hw_reset(struct aw8624_data *haptics)
{
	if (!haptics->reset_gpio)
		return;

	/* Assert reset (GPIO_ACTIVE_LOW -> logical 1 drives pin low) */
	gpiod_set_value_cansleep(haptics->reset_gpio, 1);
	usleep_range(1500, 2500);
	/* Release reset (logical 0 drives pin high) */
	gpiod_set_value_cansleep(haptics->reset_gpio, 0);
	usleep_range(4000, 5000);
}

static int aw8624_wait_standby(struct aw8624_data *haptics)
{
	unsigned int val;

	return regmap_read_poll_timeout(haptics->regmap, AW8624_REG_GLB_STATE,
					val, (val & 0x0f) == 0,
					2000, 100000);
}

static int aw8624_stop(struct aw8624_data *haptics)
{
	haptics->cont_active = false;
	regmap_update_bits(haptics->regmap, AW8624_REG_GO,
			   AW8624_GO_MASK, AW8624_GO_DISABLE);
	aw8624_wait_standby(haptics);
	return regmap_update_bits(haptics->regmap, AW8624_REG_SYSCTRL,
				  AW8624_SYSCTRL_WORK_MODE_MASK,
				  AW8624_SYSCTRL_STANDBY);
}

/*
 * Map requested duration (ms) to one of Xiaomi lisa's 11 HyperOS Taptic SRAM
 * waveforms (aw8624_haptic.bin @ 24 kHz):
 *   #11:  4.9 ms (1.0 cyc) - ultra-crisp iOS micro-tick (keys, Short=10ms)
 *   #8 : 12.4 ms (2.5 cyc) - crisp iOS Taptic click (UI buttons, toggles)
 *   #5 : 16.8 ms (3.0 cyc) - firm Taptic tap (Medium=50ms, default Long=100ms)
 *   #6 : 21.6 ms (4.5 cyc) - medium-strong Taptic pulse (notifications)
 *   #2 : 24.4 ms (5.5 cyc) - strong Taptic thud (110..150ms)
 */
static u8 aw8624_select_ram_wav(u16 duration_ms)
{
	if (duration_ms <= 12)
		return 11;
	if (duration_ms <= 30)
		return 8;
	if (duration_ms <= 60)
		return 5;
	if (duration_ms <= 110)
		return 6;
	return 2;
}

static int aw8624_start_ram(struct aw8624_data *haptics, u16 magnitude, u16 duration_ms)
{
	u8 wav_id = aw8624_select_ram_wav(duration_ms);
	u8 gain;

	/* Scale 16-bit FF magnitude into AW8624 DATDBG gain [0x38..0x80] */
	gain = 0x38 + ((u32)magnitude * (0x80 - 0x38)) / 0xffff;

	if (haptics->cont_active) {
		aw8624_stop(haptics);
	} else {
		regmap_update_bits(haptics->regmap, AW8624_REG_GO,
				   AW8624_GO_MASK, AW8624_GO_DISABLE);
	}

	regmap_update_bits(haptics->regmap, AW8624_REG_SYSCTRL,
			   AW8624_SYSCTRL_PLAY_MODE_MASK |
			   AW8624_SYSCTRL_WORK_MODE_MASK,
			   AW8624_SYSCTRL_PLAY_MODE_RAM |
			   AW8624_SYSCTRL_ACTIVE);
	regmap_write(haptics->regmap, AW8624_REG_DATDBG, gain);
	regmap_write(haptics->regmap, AW8624_REG_WAVSEQ1, wav_id);
	regmap_write(haptics->regmap, AW8624_REG_WAVSEQ2, 0x00);
	regmap_write(haptics->regmap, AW8624_REG_WAVLOOP1, 0x00);
	regmap_write(haptics->regmap, AW8624_REG_MAIN_LOOP, 0x00);

	return regmap_update_bits(haptics->regmap, AW8624_REG_GO,
				  AW8624_GO_MASK, AW8624_GO_ENABLE);
}

static int aw8624_start_cont(struct aw8624_data *haptics, u16 magnitude)
{
	u32 f0_reg;
	u8 drv_lvl;

	/* Scale 16-bit FF magnitude (1..0xffff) into [0x18..drv_lvl_max] */
	drv_lvl = (u32)magnitude * haptics->drv_lvl_max / 0xffff;
	if (drv_lvl < 0x18)
		drv_lvl = 0x18;

	f0_reg = 1000000000U / (haptics->f0_pre * haptics->f0_coeff);

	/* Stop any in-flight playback cleanly before reconfiguring */
	regmap_update_bits(haptics->regmap, AW8624_REG_GO,
			   AW8624_GO_MASK, AW8624_GO_DISABLE);
	usleep_range(2000, 2500);

	/* Active + CONT mode */
	regmap_update_bits(haptics->regmap, AW8624_REG_SYSCTRL,
			   AW8624_SYSCTRL_PLAY_MODE_MASK |
			   AW8624_SYSCTRL_WORK_MODE_MASK,
			   AW8624_SYSCTRL_PLAY_MODE_CONT |
			   AW8624_SYSCTRL_ACTIVE);

	/* Preset LRA resonant frequency */
	regmap_write(haptics->regmap, AW8624_REG_F_PRE_H, (f0_reg >> 8) & 0xff);
	regmap_write(haptics->regmap, AW8624_REG_F_PRE_L, f0_reg & 0xff);

	/* Enable 1kHz LPF */
	regmap_update_bits(haptics->regmap, AW8624_REG_DATCTRL,
			   AW8624_DATCTRL_FC_MASK | AW8624_DATCTRL_LPF_EN,
			   AW8624_DATCTRL_FC_1000HZ | AW8624_DATCTRL_LPF_EN);

	/* CONT brake config (vendor lisa defaults) */
	regmap_write(haptics->regmap, AW8624_REG_SW_BRAKE, AW8624_DEFAULT_SW_BRAKE);
	regmap_write(haptics->regmap, AW8624_REG_BRAKE0_CTRL, 0x5a);
	regmap_write(haptics->regmap, AW8624_REG_BRAKE1_CTRL, BIT(7) | 0x2a);
	regmap_write(haptics->regmap, AW8624_REG_BRAKE2_CTRL, BIT(7) | 0x14);
	regmap_write(haptics->regmap, AW8624_REG_BRAKE_NUM, (2 << 6) | (2 << 3) | 5);

	/* CONT control: closed-loop playback controlled by GO bit */
	regmap_write(haptics->regmap, AW8624_REG_CONT_CTRL,
		     AW8624_CONT_CTRL_ZC_EN |
		     AW8624_CONT_CTRL_WAIT_1P |
		     AW8624_CONT_CTRL_BY_GO |
		     AW8624_CONT_CTRL_CLOSE_PB |
		     AW8624_CONT_CTRL_F0_DIS |
		     AW8624_CONT_CTRL_O2C_DIS |
		     AW8624_CONT_CTRL_AUTO_BRK_EN);

	regmap_write(haptics->regmap, AW8624_REG_TD_H, (haptics->cont_td >> 8) & 0xff);
	regmap_write(haptics->regmap, AW8624_REG_TD_L, haptics->cont_td & 0xff);
	regmap_write(haptics->regmap, AW8624_REG_ZC_THRSH_H, (haptics->zc_thr >> 8) & 0xff);
	regmap_write(haptics->regmap, AW8624_REG_ZC_THRSH_L, haptics->zc_thr & 0xff);
	regmap_update_bits(haptics->regmap, AW8624_REG_BEMF_NUM, 0x0f, haptics->num_brk);
	regmap_write(haptics->regmap, AW8624_REG_TIME_NZC, 0x1f);
	regmap_write(haptics->regmap, AW8624_REG_TSET, AW8624_DEFAULT_TSET);
	regmap_write(haptics->regmap, AW8624_REG_R_SPARE, AW8624_DEFAULT_R_SPARE);

	regmap_write(haptics->regmap, AW8624_REG_DRV_LVL, drv_lvl);
	regmap_write(haptics->regmap, AW8624_REG_DRV_LVL_OV, haptics->drv_lvl_ov);

	haptics->cont_active = true;
	return regmap_update_bits(haptics->regmap, AW8624_REG_GO,
				  AW8624_GO_MASK, AW8624_GO_ENABLE);
}

static void aw8624_ram_loaded(const struct firmware *fw, void *context)
{
	struct aw8624_data *haptics = context;
	u8 tx_buf[AW8624_RAM_CHUNK_SIZE + 1];
	u16 expected_sum, calc_sum = 0, base_addr;
	size_t i, pos;
	int ret;

	if (!fw) {
		dev_info(haptics->dev, "%s not found, using CONT mode only\n",
			 AW8624_RAM_FW_NAME);
		return;
	}

	if (fw->size < 6) {
		dev_err(haptics->dev, "Invalid %s size: %zu\n",
			AW8624_RAM_FW_NAME, fw->size);
		release_firmware(fw);
		return;
	}

	expected_sum = ((u16)fw->data[0] << 8) | fw->data[1];
	for (i = 2; i < fw->size; i++)
		calc_sum += fw->data[i];

	if (calc_sum != expected_sum) {
		dev_err(haptics->dev, "%s checksum mismatch (got 0x%04x, want 0x%04x)\n",
			AW8624_RAM_FW_NAME, calc_sum, expected_sum);
		release_firmware(fw);
		return;
	}

	base_addr = ((u16)fw->data[2] << 8) | fw->data[3];

	mutex_lock(&haptics->lock);

	aw8624_stop(haptics);

	/* Enable Active + RAMINIT */
	regmap_update_bits(haptics->regmap, AW8624_REG_SYSCTRL,
			   AW8624_SYSCTRL_WORK_MODE_MASK |
			   AW8624_SYSCTRL_RAMINIT_MASK,
			   AW8624_SYSCTRL_ACTIVE |
			   AW8624_SYSCTRL_RAMINIT_EN);

	/* Program SRAM base address and FIFO watermarks */
	regmap_write(haptics->regmap, AW8624_REG_BASE_ADDRH, fw->data[2]);
	regmap_write(haptics->regmap, AW8624_REG_BASE_ADDRL, fw->data[3]);
	regmap_write(haptics->regmap, AW8624_REG_FIFO_AEH, (base_addr >> 1) >> 8);
	regmap_write(haptics->regmap, AW8624_REG_FIFO_AEL, (base_addr >> 1) & 0xff);
	regmap_write(haptics->regmap, AW8624_REG_FIFO_AFH,
		     (base_addr - (base_addr >> 2)) >> 8);
	regmap_write(haptics->regmap, AW8624_REG_FIFO_AFL,
		     (base_addr - (base_addr >> 2)) & 0xff);

	/* Upload SRAM payload starting at fw->data[4] */
	regmap_write(haptics->regmap, AW8624_REG_RAMADDRH, fw->data[2]);
	regmap_write(haptics->regmap, AW8624_REG_RAMADDRL, fw->data[3]);

	tx_buf[0] = AW8624_REG_RAMDATA;
	ret = 0;
	for (pos = 4; pos < fw->size; pos += AW8624_RAM_CHUNK_SIZE) {
		size_t chunk = min_t(size_t, AW8624_RAM_CHUNK_SIZE, fw->size - pos);

		memcpy(&tx_buf[1], &fw->data[pos], chunk);
		ret = i2c_master_send(haptics->client, tx_buf, chunk + 1);
		if (ret < 0) {
			dev_err(haptics->dev, "SRAM write failed at %zu: %d\n", pos, ret);
			break;
		}
	}

	/* Disable RAMINIT and return to standby */
	regmap_update_bits(haptics->regmap, AW8624_REG_SYSCTRL,
			   AW8624_SYSCTRL_RAMINIT_MASK |
			   AW8624_SYSCTRL_WORK_MODE_MASK,
			   AW8624_SYSCTRL_RAMINIT_OFF |
			   AW8624_SYSCTRL_STANDBY);

	if (ret >= 0) {
		haptics->ram_loaded = true;
		dev_info(haptics->dev,
			 "Loaded %s into SRAM (%zu bytes, base=0x%04x, checksum=0x%04x)\n",
			 AW8624_RAM_FW_NAME, fw->size - 4, base_addr, calc_sum);
	}

	mutex_unlock(&haptics->lock);
	release_firmware(fw);
}

static void aw8624_stop_work(struct work_struct *work)
{
	struct aw8624_data *haptics =
		container_of(work, struct aw8624_data, stop_work.work);

	mutex_lock(&haptics->lock);
	if (haptics->cont_active)
		aw8624_stop(haptics);
	mutex_unlock(&haptics->lock);
}

static void aw8624_play_work(struct work_struct *work)
{
	struct aw8624_data *haptics = container_of(work, struct aw8624_data, play_work);
	u16 mag = READ_ONCE(haptics->magnitude);
	u16 dur = READ_ONCE(haptics->duration_ms);

	mutex_lock(&haptics->lock);
	if (mag) {
		if (haptics->ram_loaded && dur > 0 && dur <= AW8624_RAM_MAX_DURATION_MS) {
			aw8624_start_ram(haptics, mag, dur);
		} else {
			aw8624_start_cont(haptics, mag);
			if (dur > 0)
				mod_delayed_work(system_wq, &haptics->stop_work,
						 msecs_to_jiffies(dur));
		}
	} else if (haptics->cont_active) {
		aw8624_stop(haptics);
	}
	mutex_unlock(&haptics->lock);
}

static int aw8624_ff_upload(struct input_dev *dev, struct ff_effect *effect,
			    struct ff_effect *old)
{
	struct aw8624_data *haptics = input_get_drvdata(dev);
	u16 mag = 0;

	if (effect->id < 0 || effect->id >= AW8624_MAX_EFFECTS)
		return -EINVAL;

	if (effect->type == FF_RUMBLE) {
		mag = effect->u.rumble.strong_magnitude;
		if (!mag)
			mag = effect->u.rumble.weak_magnitude;
	} else if (effect->type == FF_PERIODIC) {
		mag = effect->u.periodic.magnitude;
		if (mag > 0x7fff)
			mag = 0x7fff;
		mag <<= 1;
	} else {
		return -EINVAL;
	}

	haptics->effects[effect->id].magnitude = mag;
	haptics->effects[effect->id].duration_ms = effect->replay.length;
	return 0;
}

static int aw8624_ff_playback(struct input_dev *dev, int effect_id, int value)
{
	struct aw8624_data *haptics = input_get_drvdata(dev);

	if (effect_id < 0 || effect_id >= AW8624_MAX_EFFECTS)
		return -EINVAL;

	if (value > 0) {
		cancel_delayed_work(&haptics->stop_work);
		WRITE_ONCE(haptics->magnitude, haptics->effects[effect_id].magnitude);
		WRITE_ONCE(haptics->duration_ms, haptics->effects[effect_id].duration_ms);
		schedule_work(&haptics->play_work);
	} else {
		cancel_delayed_work(&haptics->stop_work);
		WRITE_ONCE(haptics->magnitude, 0);
		schedule_work(&haptics->play_work);
	}

	return 0;
}

static void aw8624_close(struct input_dev *dev)
{
	struct aw8624_data *haptics = input_get_drvdata(dev);

	cancel_delayed_work_sync(&haptics->stop_work);
	cancel_work_sync(&haptics->play_work);
	mutex_lock(&haptics->lock);
	aw8624_stop(haptics);
	mutex_unlock(&haptics->lock);
}

static int aw8624_hw_init(struct aw8624_data *haptics)
{
	unsigned int chipid = 0;
	int ret, tries;

	for (tries = 0; tries < 3; tries++) {
		aw8624_hw_reset(haptics);
		ret = regmap_read(haptics->regmap, AW8624_REG_ID, &chipid);
		if (!ret && chipid == AW8624_CHIP_ID)
			break;
		usleep_range(2000, 3000);
	}

	if (ret)
		return dev_err_probe(haptics->dev, ret, "Failed to read AW8624 chip ID\n");

	if (chipid != AW8624_CHIP_ID)
		return dev_err_probe(haptics->dev, -ENODEV,
				     "Unexpected AW8624 chip ID: 0x%02x\n", chipid);

	/* Soft reset */
	regmap_write(haptics->regmap, AW8624_REG_ID, AW8624_SOFTRST_VAL);
	usleep_range(2000, 2500);

	/* Mask all interrupts */
	regmap_write(haptics->regmap, AW8624_REG_SYSINTM, 0x7f);

	/* 24kHz PWM, disable motor protect override, HW VBAT compensation, standby */
	regmap_update_bits(haptics->regmap, AW8624_REG_PWMDBG,
			   AW8624_PWMDBG_PWM_MASK, AW8624_PWMDBG_PWM_24K);
	regmap_write(haptics->regmap, AW8624_REG_PRLVL, 0x00);
	regmap_write(haptics->regmap, AW8624_REG_PRTIME, 0x00);
	regmap_update_bits(haptics->regmap, AW8624_REG_DETCTRL,
			   AW8624_DETCTRL_VBAT_HW_COMP,
			   AW8624_DETCTRL_VBAT_HW_COMP);
	aw8624_stop(haptics);

	dev_info(haptics->dev, "Awinic AW8624 haptic controller initialized (id=0x%02x)\n",
		 chipid);
	return 0;
}

static int aw8624_probe(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	struct aw8624_data *haptics;
	int ret;

	haptics = devm_kzalloc(dev, sizeof(*haptics), GFP_KERNEL);
	if (!haptics)
		return -ENOMEM;

	haptics->client = client;
	haptics->dev = dev;
	haptics->f0_pre = AW8624_DEFAULT_F0_PRE;
	haptics->f0_coeff = AW8624_DEFAULT_F0_COEFF;
	haptics->drv_lvl_max = AW8624_DEFAULT_DRV_LVL;
	haptics->drv_lvl_ov = AW8624_DEFAULT_DRV_LVL_OV;
	haptics->cont_td = AW8624_DEFAULT_CONT_TD;
	haptics->zc_thr = AW8624_DEFAULT_ZC_THR;
	haptics->num_brk = AW8624_DEFAULT_NUM_BRK;
	mutex_init(&haptics->lock);

	device_property_read_u32(dev, "awinic,f0-pre", &haptics->f0_pre);
	device_property_read_u32(dev, "awinic,cont-drv-lvl", &haptics->drv_lvl_max);
	device_property_read_u32(dev, "awinic,cont-drv-lvl-ov", &haptics->drv_lvl_ov);

	haptics->regmap = devm_regmap_init_i2c(client, &aw8624_regmap_config);
	if (IS_ERR(haptics->regmap))
		return dev_err_probe(dev, PTR_ERR(haptics->regmap),
				     "Failed to init regmap\n");

	haptics->reset_gpio = devm_gpiod_get_optional(dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(haptics->reset_gpio))
		return dev_err_probe(dev, PTR_ERR(haptics->reset_gpio),
				     "Failed to get reset GPIO\n");

	INIT_WORK(&haptics->play_work, aw8624_play_work);
	INIT_DELAYED_WORK(&haptics->stop_work, aw8624_stop_work);
	i2c_set_clientdata(client, haptics);

	ret = aw8624_hw_init(haptics);
	if (ret)
		return ret;

	haptics->input_dev = devm_input_allocate_device(dev);
	if (!haptics->input_dev)
		return -ENOMEM;

	haptics->input_dev->name = "aw8624_haptic";
	haptics->input_dev->id.bustype = BUS_I2C;
	haptics->input_dev->close = aw8624_close;
	input_set_drvdata(haptics->input_dev, haptics);
	input_set_capability(haptics->input_dev, EV_FF, FF_RUMBLE);
	input_set_capability(haptics->input_dev, EV_FF, FF_PERIODIC);
	input_set_capability(haptics->input_dev, EV_FF, FF_SINE);

	ret = input_ff_create(haptics->input_dev, AW8624_MAX_EFFECTS);
	if (ret)
		return dev_err_probe(dev, ret, "Failed to create FF device\n");

	haptics->input_dev->ff->upload = aw8624_ff_upload;
	haptics->input_dev->ff->playback = aw8624_ff_playback;

	ret = input_register_device(haptics->input_dev);
	if (ret)
		return dev_err_probe(dev, ret, "Failed to register input device\n");

	/* Load HyperOS Taptic SRAM waveform table asynchronously */
	ret = request_firmware_nowait(THIS_MODULE, true, AW8624_RAM_FW_NAME,
				      dev, GFP_KERNEL, haptics,
				      aw8624_ram_loaded);
	if (ret)
		dev_warn(dev, "Could not request %s: %d\n", AW8624_RAM_FW_NAME, ret);

	return 0;
}

static void aw8624_remove(struct i2c_client *client)
{
	struct aw8624_data *haptics = i2c_get_clientdata(client);

	cancel_delayed_work_sync(&haptics->stop_work);
	cancel_work_sync(&haptics->play_work);
	mutex_lock(&haptics->lock);
	aw8624_stop(haptics);
	mutex_unlock(&haptics->lock);
}

static const struct of_device_id aw8624_of_match[] = {
	{ .compatible = "awinic,aw8624" },
	{ }
};
MODULE_DEVICE_TABLE(of, aw8624_of_match);

static struct i2c_driver aw8624_driver = {
	.driver = {
		.name = "aw8624",
		.of_match_table = aw8624_of_match,
	},
	.probe = aw8624_probe,
	.remove = aw8624_remove,
};
module_i2c_driver(aw8624_driver);

MODULE_DESCRIPTION("Awinic AW8624 LRA haptic force-feedback driver");
MODULE_FIRMWARE(AW8624_RAM_FW_NAME);
MODULE_LICENSE("GPL");

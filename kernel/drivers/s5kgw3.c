// SPDX-License-Identifier: GPL-2.0
/*
 * V4L2 subdev driver for Samsung S5KGW3 64MP CMOS image sensor
 * Extracted register tables for Xiaomi 11 Lite 5G NE (lisa, 19.2 MHz MCLK)
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/pm_runtime.h>
#include <linux/regulator/consumer.h>
#include <media/v4l2-cci.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-fwnode.h>

#define S5KGW3_CHIP_ID_REG		CCI_REG16(0x0000)
#define S5KGW3_CHIP_ID			0x7309

#define S5KGW3_REG_MODE_SELECT		CCI_REG16(0x0100)
#define S5KGW3_MODE_STANDBY		0x0000
#define S5KGW3_MODE_STREAMING		0x0100

#define S5KGW3_REG_ORIENTATION		CCI_REG8(0x0101)

#define S5KGW3_REG_EXPOSURE		CCI_REG16(0x0202)
#define S5KGW3_EXPOSURE_MIN		4
#define S5KGW3_EXPOSURE_STEP		1
#define S5KGW3_EXPOSURE_MARGIN		10

#define S5KGW3_REG_AGAIN		CCI_REG16(0x0204)
#define S5KGW3_AGAIN_MIN		1
#define S5KGW3_AGAIN_MAX		16
#define S5KGW3_AGAIN_DEFAULT		1

#define S5KGW3_REG_VTS			CCI_REG16(0x0340)
#define S5KGW3_VTS_MAX			0xffff

#define S5KGW3_REG_TEST_PATTERN		CCI_REG16(0x0600)

#define S5KGW3_NATIVE_WIDTH		9280
#define S5KGW3_NATIVE_HEIGHT		6944

static const s64 s5kgw3_link_freq_menu[] = {
	835200000LL,
	200000000LL,
};

static const char * const s5kgw3_supply_names[] = {
	"vio",
	"vana",
	"vdig",
};

static const u32 s5kgw3_mbus_codes[] = {
	MEDIA_BUS_FMT_SGRBG10_1X10,
	MEDIA_BUS_FMT_SRGGB10_1X10,
	MEDIA_BUS_FMT_SBGGR10_1X10,
	MEDIA_BUS_FMT_SGBRG10_1X10,
};

static const struct cci_reg_sequence s5kgw3_init_regs[] = {
	{ CCI_REG16(0xfcfc), 0x4000 },
	{ CCI_REG16(0x6028), 0x4000 },
	{ CCI_REG16(0x0000), 0x0001 },
	{ CCI_REG16(0x0000), 0x7309 },
	{ CCI_REG16(0x6010), 0x0001 },
	{ CCI_REG16(0x6046), 0x0085 },
	{ CCI_REG16(0x6214), 0xff7d },
	{ CCI_REG16(0x6218), 0x0000 },
	{ CCI_REG16(0x0a02), 0xf400 },
	{ CCI_REG16(0x6028), 0x2400 },
	{ CCI_REG16(0x602a), 0x7cd0 },
	{ CCI_REG16(0x6f12), 0x4111 },
	{ CCI_REG16(0x6f12), 0x9716 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x6f12), 0x9386 },
	{ CCI_REG16(0x6f12), 0x6683 },
	{ CCI_REG16(0x6f12), 0x9787 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x6f12), 0x9387 },
	{ CCI_REG16(0x6f12), 0x27b2 },
	{ CCI_REG16(0x6f12), 0x06c6 },
	{ CCI_REG16(0x6f12), 0x17d7 },
	{ CCI_REG16(0x6f12), 0xffff },
	{ CCI_REG16(0x6f12), 0x1307 },
	{ CCI_REG16(0x6f12), 0xc7a3 },
	{ CCI_REG16(0x6f12), 0x958f },
	{ CCI_REG16(0x6f12), 0x2314 },
	{ CCI_REG16(0x6f12), 0xf760 },
	{ CCI_REG16(0x6f12), 0x2322 },
	{ CCI_REG16(0x6f12), 0xd760 },
	{ CCI_REG16(0x6f12), 0xed25 },
	{ CCI_REG16(0x6f12), 0xb240 },
	{ CCI_REG16(0x6f12), 0x9307 },
	{ CCI_REG16(0x6f12), 0xd003 },
	{ CCI_REG16(0x6f12), 0x1707 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x6f12), 0x2327 },
	{ CCI_REG16(0x6f12), 0xf700 },
	{ CCI_REG16(0x6f12), 0x4101 },
	{ CCI_REG16(0x6f12), 0x8280 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x6f12), 0x3800 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x6f12), 0x97f7 },
	{ CCI_REG16(0x6f12), 0xffff },
	{ CCI_REG16(0x6f12), 0x9387 },
	{ CCI_REG16(0x6f12), 0x475b },
	{ CCI_REG16(0x6f12), 0x9c43 },
	{ CCI_REG16(0x6f12), 0x1305 },
	{ CCI_REG16(0x6f12), 0x803e },
	{ CCI_REG16(0x6f12), 0x83d6 },
	{ CCI_REG16(0x6f12), 0x6713 },
	{ CCI_REG16(0x6f12), 0x03d7 },
	{ CCI_REG16(0x6f12), 0xe713 },
	{ CCI_REG16(0x6f12), 0x03c6 },
	{ CCI_REG16(0x6f12), 0xd713 },
	{ CCI_REG16(0x6f12), 0x9397 },
	{ CCI_REG16(0x6f12), 0x8600 },
	{ CCI_REG16(0x6f12), 0xa182 },
	{ CCI_REG16(0x6f12), 0xd58f },
	{ CCI_REG16(0x6f12), 0xc207 },
	{ CCI_REG16(0x6f12), 0xc183 },
	{ CCI_REG16(0x6f12), 0xb387 },
	{ CCI_REG16(0x6f12), 0xa702 },
	{ CCI_REG16(0x6f12), 0x1315 },
	{ CCI_REG16(0x6f12), 0x8700 },
	{ CCI_REG16(0x6f12), 0x2183 },
	{ CCI_REG16(0x6f12), 0x598d },
	{ CCI_REG16(0x6f12), 0x4205 },
	{ CCI_REG16(0x6f12), 0x4181 },
	{ CCI_REG16(0x6f12), 0xa187 },
	{ CCI_REG16(0x6f12), 0xb397 },
	{ CCI_REG16(0x6f12), 0xc700 },
	{ CCI_REG16(0x6f12), 0x3e95 },
	{ CCI_REG16(0x6f12), 0x8280 },
	{ CCI_REG16(0x6f12), 0x97d7 },
	{ CCI_REG16(0x6f12), 0xffff },
	{ CCI_REG16(0x6f12), 0x9387 },
	{ CCI_REG16(0x6f12), 0xc79c },
	{ CCI_REG16(0x6f12), 0x2397 },
	{ CCI_REG16(0x6f12), 0x0764 },
	{ CCI_REG16(0x6f12), 0x2384 },
	{ CCI_REG16(0x6f12), 0x0764 },
	{ CCI_REG16(0x6f12), 0x2385 },
	{ CCI_REG16(0x6f12), 0x0764 },
	{ CCI_REG16(0x6f12), 0x238e },
	{ CCI_REG16(0x6f12), 0x073a },
	{ CCI_REG16(0x6f12), 0x8280 },
	{ CCI_REG16(0x6f12), 0x4111 },
	{ CCI_REG16(0x6f12), 0x9707 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x6f12), 0x9387 },
	{ CCI_REG16(0x6f12), 0x4779 },
	{ CCI_REG16(0x6f12), 0x26c2 },
	{ CCI_REG16(0x6f12), 0x8443 },
	{ CCI_REG16(0x6f12), 0x0146 },
	{ CCI_REG16(0x6f12), 0x22c4 },
	{ CCI_REG16(0x6f12), 0xa685 },
	{ CCI_REG16(0x6f12), 0x2a84 },
	{ CCI_REG16(0x6f12), 0x1145 },
	{ CCI_REG16(0x6f12), 0x06c6 },
	{ CCI_REG16(0x6f12), 0x9780 },
	{ CCI_REG16(0x6f12), 0xfffb },
	{ CCI_REG16(0x6f12), 0xe780 },
	{ CCI_REG16(0x6f12), 0xe06f },
	{ CCI_REG16(0x6f12), 0x2285 },
	{ CCI_REG16(0x6f12), 0x97e0 },
	{ CCI_REG16(0x6f12), 0x00fc },
	{ CCI_REG16(0x6f12), 0xe780 },
	{ CCI_REG16(0x6f12), 0x40c9 },
	{ CCI_REG16(0x6f12), 0x97d7 },
	{ CCI_REG16(0x6f12), 0xffff },
	{ CCI_REG16(0x6f12), 0x9387 },
	{ CCI_REG16(0x6f12), 0x6798 },
	{ CCI_REG16(0x6f12), 0x03d7 },
	{ CCI_REG16(0x6f12), 0xe764 },
	{ CCI_REG16(0x6f12), 0x97f6 },
	{ CCI_REG16(0x6f12), 0xffff },
	{ CCI_REG16(0x6f12), 0x9386 },
	{ CCI_REG16(0x6f12), 0x2652 },
	{ CCI_REG16(0x6f12), 0x9442 },
	{ CCI_REG16(0x6f12), 0x058b },
	{ CCI_REG16(0x6f12), 0x03d6 },
	{ CCI_REG16(0x6f12), 0x2620 },
	{ CCI_REG16(0x6f12), 0x31e7 },
	{ CCI_REG16(0x6f12), 0x9317 },
	{ CCI_REG16(0x6f12), 0x8600 },
	{ CCI_REG16(0x6f12), 0x2182 },
	{ CCI_REG16(0x6f12), 0xd18f },
	{ CCI_REG16(0x6f12), 0xc207 },
	{ CCI_REG16(0x6f12), 0xc183 },
	{ CCI_REG16(0x6f12), 0x03c7 },
	{ CCI_REG16(0x6f12), 0x4670 },
	{ CCI_REG16(0x6f12), 0xb397 },
	{ CCI_REG16(0x6f12), 0xe700 },
	{ CCI_REG16(0x6f12), 0x1cc0 },
	{ CCI_REG16(0x6f12), 0x97e7 },
	{ CCI_REG16(0x6f12), 0xffff },
	{ CCI_REG16(0x6f12), 0x83c7 },
	{ CCI_REG16(0x6f12), 0x4772 },
	{ CCI_REG16(0x6f12), 0x230e },
	{ CCI_REG16(0x6f12), 0xf400 },
	{ CCI_REG16(0x6f12), 0x17d7 },
	{ CCI_REG16(0x6f12), 0xffff },
	{ CCI_REG16(0x6f12), 0x0347 },
	{ CCI_REG16(0x6f12), 0xf7f8 },
	{ CCI_REG16(0x6f12), 0x09c7 },
	{ CCI_REG16(0x6f12), 0x17d7 },
	{ CCI_REG16(0x6f12), 0xffff },
	{ CCI_REG16(0x6f12), 0x2300 },
	{ CCI_REG16(0x6f12), 0xf7ce },
	{ CCI_REG16(0x6f12), 0x2244 },
	{ CCI_REG16(0x6f12), 0xb240 },
	{ CCI_REG16(0x6f12), 0xa685 },
	{ CCI_REG16(0x6f12), 0x9244 },
	{ CCI_REG16(0x6f12), 0x0546 },
	{ CCI_REG16(0x6f12), 0x1145 },
	{ CCI_REG16(0x6f12), 0x4101 },
	{ CCI_REG16(0x6f12), 0x1783 },
	{ CCI_REG16(0x6f12), 0xfffb },
	{ CCI_REG16(0x6f12), 0x6700 },
	{ CCI_REG16(0x6f12), 0xc368 },
	{ CCI_REG16(0x6f12), 0x83a7 },
	{ CCI_REG16(0x6f12), 0x0765 },
	{ CCI_REG16(0x6f12), 0x17d7 },
	{ CCI_REG16(0x6f12), 0xffff },
	{ CCI_REG16(0x6f12), 0x0347 },
	{ CCI_REG16(0x6f12), 0xe7f6 },
	{ CCI_REG16(0x6f12), 0x0607 },
	{ CCI_REG16(0x6f12), 0xba97 },
	{ CCI_REG16(0x6f12), 0x83d7 },
	{ CCI_REG16(0x6f12), 0x0700 },
	{ CCI_REG16(0x6f12), 0x7db7 },
	{ CCI_REG16(0x6f12), 0x0111 },
	{ CCI_REG16(0x6f12), 0x06ce },
	{ CCI_REG16(0x6f12), 0x22cc },
	{ CCI_REG16(0x6f12), 0x26ca },
	{ CCI_REG16(0x6f12), 0x4ac8 },
	{ CCI_REG16(0x6f12), 0x52c4 },
	{ CCI_REG16(0x6f12), 0x3289 },
	{ CCI_REG16(0x6f12), 0x56c2 },
	{ CCI_REG16(0x6f12), 0x5ac0 },
	{ CCI_REG16(0x6f12), 0xb68a },
	{ CCI_REG16(0x6f12), 0x2e8b },
	{ CCI_REG16(0x6f12), 0x4ec6 },
	{ CCI_REG16(0x6f12), 0xaa84 },
	{ CCI_REG16(0x6f12), 0x97f0 },
	{ CCI_REG16(0x6f12), 0x00fc },
	{ CCI_REG16(0x6f12), 0xe780 },
	{ CCI_REG16(0x6f12), 0xa0b6 },
	{ CCI_REG16(0x6f12), 0x2a8a },
	{ CCI_REG16(0x6f12), 0x4a85 },
	{ CCI_REG16(0x6f12), 0x9780 },
	{ CCI_REG16(0x6f12), 0xfffb },
	{ CCI_REG16(0x6f12), 0xe780 },
	{ CCI_REG16(0x6f12), 0xc025 },
	{ CCI_REG16(0x6f12), 0x2a84 },
	{ CCI_REG16(0x6f12), 0x5285 },
	{ CCI_REG16(0x6f12), 0x9780 },
	{ CCI_REG16(0x6f12), 0xfffb },
	{ CCI_REG16(0x6f12), 0xe780 },
	{ CCI_REG16(0x6f12), 0x0025 },
	{ CCI_REG16(0x6f12), 0x2295 },
	{ CCI_REG16(0x6f12), 0x9307 },
	{ CCI_REG16(0x6f12), 0x0002 },
	{ CCI_REG16(0x6f12), 0x6374 },
	{ CCI_REG16(0x6f12), 0xf500 },
	{ CCI_REG16(0x6f12), 0x1305 },
	{ CCI_REG16(0x6f12), 0x0002 },
	{ CCI_REG16(0x6f12), 0x0115 },
	{ CCI_REG16(0x6f12), 0x1375 },
	{ CCI_REG16(0x6f12), 0xf50f },
	{ CCI_REG16(0x6f12), 0x2380 },
	{ CCI_REG16(0x6f12), 0xa402 },
	{ CCI_REG16(0x6f12), 0x17c7 },
	{ CCI_REG16(0x6f12), 0xffff },
	{ CCI_REG16(0x6f12), 0x0357 },
	{ CCI_REG16(0x6f12), 0x871e },
	{ CCI_REG16(0x6f12), 0x2312 },
	{ CCI_REG16(0x6f12), 0xeb00 },
	{ CCI_REG16(0x6f12), 0x631d },
	{ CCI_REG16(0x6f12), 0x0510 },
	{ CCI_REG16(0x6f12), 0xb309 },
	{ CCI_REG16(0x6f12), 0x2a03 },
	{ CCI_REG16(0x6f12), 0x97c7 },
	{ CCI_REG16(0x6f12), 0xffff },
	{ CCI_REG16(0x6f12), 0x9387 },
	{ CCI_REG16(0x6f12), 0x271b },
	{ CCI_REG16(0x6f12), 0x9c43 },
	{ CCI_REG16(0x6f12), 0x93da },
	{ CCI_REG16(0x6f12), 0x4a00 },
	{ CCI_REG16(0x6f12), 0xd699 },
	{ CCI_REG16(0x6f12), 0x63f5 },
	{ CCI_REG16(0x6f12), 0xf900 },
	{ CCI_REG16(0x6f12), 0x33d9 },
	{ CCI_REG16(0x6f12), 0x4703 },
	{ CCI_REG16(0x6f12), 0xbe89 },
	{ CCI_REG16(0x6f12), 0x97c7 },
	{ CCI_REG16(0x6f12), 0xffff },
	{ CCI_REG16(0x6f12), 0x83d7 },
	{ CCI_REG16(0x6f12), 0x671a },
	{ CCI_REG16(0x6f12), 0xb387 },
	{ CCI_REG16(0x6f12), 0xf940 },
	{ CCI_REG16(0x6f12), 0x97c6 },
	{ CCI_REG16(0x6f12), 0xffff },
	{ CCI_REG16(0x6f12), 0x83d6 },
	{ CCI_REG16(0x6f12), 0x0624 },
	{ CCI_REG16(0x6f12), 0x9705 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x6f12), 0x83c5 },
	{ CCI_REG16(0x6f12), 0x5565 },
	{ CCI_REG16(0x6f12), 0x0546 },
	{ CCI_REG16(0x6f12), 0x33d4 },
	{ CCI_REG16(0x6f12), 0xd702 },
	{ CCI_REG16(0x6f12), 0xb3f7 },
	{ CCI_REG16(0x6f12), 0xd702 },
	{ CCI_REG16(0x6f12), 0x6385 },
	{ CCI_REG16(0x6f12), 0xc50c },
	{ CCI_REG16(0x6f12), 0xb3b7 },
	{ CCI_REG16(0x6f12), 0xe700 },
	{ CCI_REG16(0x6f12), 0x1d8c },
	{ CCI_REG16(0x6f12), 0x97e7 },
	{ CCI_REG16(0x6f12), 0xffff },
	{ CCI_REG16(0x6f12), 0x83d7 },
	{ CCI_REG16(0x6f12), 0xe766 },
	{ CCI_REG16(0x6f12), 0x99c7 },
	{ CCI_REG16(0x6f12), 0x8546 },
	{ CCI_REG16(0x6f12), 0x6389 },
	{ CCI_REG16(0x6f12), 0xd710 },
	{ CCI_REG16(0x6f12), 0x8946 },
	{ CCI_REG16(0x6f12), 0x638a },
	{ CCI_REG16(0x6f12), 0xd710 },
	{ CCI_REG16(0x6f12), 0x2320 },
	{ CCI_REG16(0x6f12), 0x8b00 },
	{ CCI_REG16(0x6f12), 0x2392 },
	{ CCI_REG16(0x6f12), 0xe400 },
	{ CCI_REG16(0x6f12), 0x97d0 },
	{ CCI_REG16(0x6f12), 0x00fc },
	{ CCI_REG16(0x6f12), 0xe780 },
	{ CCI_REG16(0x6f12), 0x60bf },
	{ CCI_REG16(0x6f12), 0x3314 },
	{ CCI_REG16(0x6f12), 0xa400 },
	{ CCI_REG16(0x6f12), 0x97d0 },
	{ CCI_REG16(0x6f12), 0x00fc },
	{ CCI_REG16(0x6f12), 0xe780 },
	{ CCI_REG16(0x6f12), 0x60c0 },
	{ CCI_REG16(0x6f12), 0x2a94 },
	{ CCI_REG16(0x6f12), 0x97d0 },
	{ CCI_REG16(0x6f12), 0x00fc },
	{ CCI_REG16(0x6f12), 0xe780 },
	{ CCI_REG16(0x6f12), 0xc0bf },
	{ CCI_REG16(0x6f12), 0x8327 },
	{ CCI_REG16(0x6f12), 0x0b00 },
	{ CCI_REG16(0x6f12), 0x3354 },
	{ CCI_REG16(0x6f12), 0xa400 },
	{ CCI_REG16(0x6f12), 0x80c0 },
	{ CCI_REG16(0x6f12), 0x1706 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x6f12), 0x0346 },
	{ CCI_REG16(0x6f12), 0x265f },
	{ CCI_REG16(0x6f12), 0x17c7 },
	{ CCI_REG16(0x6f12), 0xffff },
	{ CCI_REG16(0x6f12), 0x0357 },
	{ CCI_REG16(0x6f12), 0xe71c },
	{ CCI_REG16(0x6f12), 0x3386 },
	{ CCI_REG16(0x6f12), 0xc740 },
	{ CCI_REG16(0x6f12), 0x8357 },
	{ CCI_REG16(0x6f12), 0x4b00 },
	{ CCI_REG16(0x6f12), 0x0565 },
	{ CCI_REG16(0x6f12), 0x3306 },
	{ CCI_REG16(0x6f12), 0xe602 },
	{ CCI_REG16(0x6f12), 0x17c7 },
	{ CCI_REG16(0x6f12), 0xffff },
	{ CCI_REG16(0x6f12), 0x0357 },
	{ CCI_REG16(0x6f12), 0x2711 },
	{ CCI_REG16(0x6f12), 0xba97 },
	{ CCI_REG16(0x6f12), 0x3e96 },
	{ CCI_REG16(0x6f12), 0x90c8 },
	{ CCI_REG16(0x6f12), 0x97e7 },
	{ CCI_REG16(0x6f12), 0xffff },
	{ CCI_REG16(0x6f12), 0x83c7 },
	{ CCI_REG16(0x6f12), 0x2760 },
	{ CCI_REG16(0x6f12), 0x85c3 },
	{ CCI_REG16(0x6f12), 0x97e7 },
	{ CCI_REG16(0x6f12), 0xffff },
	{ CCI_REG16(0x6f12), 0x83c7 },
	{ CCI_REG16(0x6f12), 0x975f },
	{ CCI_REG16(0x6f12), 0x636b },
	{ CCI_REG16(0x6f12), 0xf900 },
	{ CCI_REG16(0x6f12), 0x8146 },
	{ CCI_REG16(0x6f12), 0x1395 },
	{ CCI_REG16(0x6f12), 0xc900 },
	{ CCI_REG16(0x6f12), 0x93d5 },
	{ CCI_REG16(0x6f12), 0x4901 },
	{ CCI_REG16(0x6f12), 0x97a0 },
	{ CCI_REG16(0x6f12), 0x01fc },
	{ CCI_REG16(0x6f12), 0xe780 },
	{ CCI_REG16(0x6f12), 0x80a9 },
	{ CCI_REG16(0x6f12), 0xd858 },
	{ CCI_REG16(0x6f12), 0xdc5c },
	{ CCI_REG16(0x6f12), 0xf240 },
	{ CCI_REG16(0x6f12), 0x3305 },
	{ CCI_REG16(0x6f12), 0xe502 },
	{ CCI_REG16(0x6f12), 0x6244 },
	{ CCI_REG16(0x6f12), 0x4249 },
	{ CCI_REG16(0x6f12), 0xb249 },
	{ CCI_REG16(0x6f12), 0x224a },
	{ CCI_REG16(0x6f12), 0x924a },
	{ CCI_REG16(0x6f12), 0x024b },
	{ CCI_REG16(0x6f12), 0x3181 },
	{ CCI_REG16(0x6f12), 0x3305 },
	{ CCI_REG16(0x6f12), 0xf502 },
	{ CCI_REG16(0x6f12), 0x3181 },
	{ CCI_REG16(0x6f12), 0xc8c8 },
	{ CCI_REG16(0x6f12), 0xd244 },
	{ CCI_REG16(0x6f12), 0x0561 },
	{ CCI_REG16(0x6f12), 0x8280 },
	{ CCI_REG16(0x6f12), 0x63fd },
	{ CCI_REG16(0x6f12), 0xe704 },
	{ CCI_REG16(0x6f12), 0x0504 },
	{ CCI_REG16(0x6f12), 0x2dbf },
	{ CCI_REG16(0x6f12), 0x97d0 },
	{ CCI_REG16(0x6f12), 0x00fc },
	{ CCI_REG16(0x6f12), 0xe780 },
	{ CCI_REG16(0x6f12), 0x80b6 },
	{ CCI_REG16(0x6f12), 0x3314 },
	{ CCI_REG16(0x6f12), 0xa900 },
	{ CCI_REG16(0x6f12), 0x97d0 },
	{ CCI_REG16(0x6f12), 0x00fc },
	{ CCI_REG16(0x6f12), 0xe780 },
	{ CCI_REG16(0x6f12), 0x00b4 },
	{ CCI_REG16(0x6f12), 0x3355 },
	{ CCI_REG16(0x6f12), 0xa400 },
	{ CCI_REG16(0x6f12), 0x2320 },
	{ CCI_REG16(0x6f12), 0xab00 },
	{ CCI_REG16(0x6f12), 0x97e7 },
	{ CCI_REG16(0x6f12), 0xffff },
	{ CCI_REG16(0x6f12), 0x83d7 },
	{ CCI_REG16(0x6f12), 0xa758 },
	{ CCI_REG16(0x6f12), 0x99c7 },
	{ CCI_REG16(0x6f12), 0x0547 },
	{ CCI_REG16(0x6f12), 0x638d },
	{ CCI_REG16(0x6f12), 0xe702 },
	{ CCI_REG16(0x6f12), 0x0947 },
	{ CCI_REG16(0x6f12), 0x638d },
	{ CCI_REG16(0x6f12), 0xe702 },
	{ CCI_REG16(0x6f12), 0x83c7 },
	{ CCI_REG16(0x6f12), 0x0402 },
	{ CCI_REG16(0x6f12), 0x0357 },
	{ CCI_REG16(0x6f12), 0x4b00 },
	{ CCI_REG16(0x6f12), 0x0565 },
	{ CCI_REG16(0x6f12), 0xb357 },
	{ CCI_REG16(0x6f12), 0xf900 },
	{ CCI_REG16(0x6f12), 0x338a },
	{ CCI_REG16(0x6f12), 0x4703 },
	{ CCI_REG16(0x6f12), 0x2392 },
	{ CCI_REG16(0x6f12), 0xe400 },
	{ CCI_REG16(0x6f12), 0x23a0 },
	{ CCI_REG16(0x6f12), 0x2401 },
	{ CCI_REG16(0x6f12), 0x23a8 },
	{ CCI_REG16(0x6f12), 0x4401 },
	{ CCI_REG16(0x6f12), 0x49b7 },
	{ CCI_REG16(0x6f12), 0x0d04 },
	{ CCI_REG16(0x6f12), 0xd5b5 },
	{ CCI_REG16(0x6f12), 0x9377 },
	{ CCI_REG16(0x6f12), 0x1400 },
	{ CCI_REG16(0x6f12), 0xe39a },
	{ CCI_REG16(0x6f12), 0x07ee },
	{ CCI_REG16(0x6f12), 0x7d14 },
	{ CCI_REG16(0x6f12), 0xfdb5 },
	{ CCI_REG16(0x6f12), 0x9377 },
	{ CCI_REG16(0x6f12), 0x1500 },
	{ CCI_REG16(0x6f12), 0xf1f7 },
	{ CCI_REG16(0x6f12), 0x7d15 },
	{ CCI_REG16(0x6f12), 0x2320 },
	{ CCI_REG16(0x6f12), 0xab00 },
	{ CCI_REG16(0x6f12), 0xd1b7 },
	{ CCI_REG16(0x6f12), 0x97c7 },
	{ CCI_REG16(0x6f12), 0xffff },
	{ CCI_REG16(0x6f12), 0x83d7 },
	{ CCI_REG16(0x6f12), 0xe703 },
	{ CCI_REG16(0x6f12), 0x97c6 },
	{ CCI_REG16(0x6f12), 0xffff },
	{ CCI_REG16(0x6f12), 0x83c6 },
	{ CCI_REG16(0x6f12), 0x0611 },
	{ CCI_REG16(0x6f12), 0x17c7 },
	{ CCI_REG16(0x6f12), 0xffff },
	{ CCI_REG16(0x6f12), 0x0357 },
	{ CCI_REG16(0x6f12), 0x470d },
	{ CCI_REG16(0x6f12), 0xbe95 },
	{ CCI_REG16(0x6f12), 0x81ea },
	{ CCI_REG16(0x6f12), 0x9706 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x6f12), 0x83c6 },
	{ CCI_REG16(0x6f12), 0x564e },
	{ CCI_REG16(0x6f12), 0x8547 },
	{ CCI_REG16(0x6f12), 0x638e },
	{ CCI_REG16(0x6f12), 0xf600 },
	{ CCI_REG16(0x6f12), 0x3307 },
	{ CCI_REG16(0x6f12), 0xe502 },
	{ CCI_REG16(0x6f12), 0xba95 },
	{ CCI_REG16(0x6f12), 0x6393 },
	{ CCI_REG16(0x6f12), 0xc500 },
	{ CCI_REG16(0x6f12), 0x8280 },
	{ CCI_REG16(0x6f12), 0xaa85 },
	{ CCI_REG16(0x6f12), 0x2d45 },
	{ CCI_REG16(0x6f12), 0x1783 },
	{ CCI_REG16(0x6f12), 0xfffb },
	{ CCI_REG16(0x6f12), 0x6700 },
	{ CCI_REG16(0x6f12), 0xa36d },
	{ CCI_REG16(0x6f12), 0x9707 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x6f12), 0x83c7 },
	{ CCI_REG16(0x6f12), 0xe74b },
	{ CCI_REG16(0x6f12), 0xb307 },
	{ CCI_REG16(0x6f12), 0xf540 },
	{ CCI_REG16(0x6f12), 0x3387 },
	{ CCI_REG16(0x6f12), 0xe702 },
	{ CCI_REG16(0x6f12), 0xba95 },
	{ CCI_REG16(0x6f12), 0xf1bf },
	{ CCI_REG16(0x6f12), 0x97e7 },
	{ CCI_REG16(0x6f12), 0xffff },
	{ CCI_REG16(0x6f12), 0x83c7 },
	{ CCI_REG16(0x6f12), 0xa747 },
	{ CCI_REG16(0x6f12), 0xf9c7 },
	{ CCI_REG16(0x6f12), 0x4111 },
	{ CCI_REG16(0x6f12), 0x22c4 },
	{ CCI_REG16(0x6f12), 0x06c6 },
	{ CCI_REG16(0x6f12), 0x26c2 },
	{ CCI_REG16(0x6f12), 0x97f7 },
	{ CCI_REG16(0x6f12), 0xffff },
	{ CCI_REG16(0x6f12), 0x83c7 },
	{ CCI_REG16(0x6f12), 0xe78b },
	{ CCI_REG16(0x6f12), 0x0947 },
	{ CCI_REG16(0x6f12), 0x17e4 },
	{ CCI_REG16(0x6f12), 0xffff },
	{ CCI_REG16(0x6f12), 0x1304 },
	{ CCI_REG16(0x6f12), 0xe445 },
	{ CCI_REG16(0x6f12), 0x6381 },
	{ CCI_REG16(0x6f12), 0xe708 },
	{ CCI_REG16(0x6f12), 0x0d47 },
	{ CCI_REG16(0x6f12), 0x6387 },
	{ CCI_REG16(0x6f12), 0xe70a },
	{ CCI_REG16(0x6f12), 0xd9c7 },
	{ CCI_REG16(0x6f12), 0xbd64 },
	{ CCI_REG16(0x6f12), 0x0546 },
	{ CCI_REG16(0x6f12), 0x8545 },
	{ CCI_REG16(0x6f12), 0x1385 },
	{ CCI_REG16(0x6f12), 0xe447 },
	{ CCI_REG16(0x6f12), 0x9780 },
	{ CCI_REG16(0x6f12), 0xfffb },
	{ CCI_REG16(0x6f12), 0xe780 },
	{ CCI_REG16(0x6f12), 0x2037 },
	{ CCI_REG16(0x6f12), 0x97f7 },
	{ CCI_REG16(0x6f12), 0xffff },
	{ CCI_REG16(0x6f12), 0x83c7 },
	{ CCI_REG16(0x6f12), 0xf788 },
	{ CCI_REG16(0x6f12), 0xd5e3 },
	{ CCI_REG16(0x6f12), 0x17f6 },
	{ CCI_REG16(0x6f12), 0xffff },
	{ CCI_REG16(0x6f12), 0x0346 },
	{ CCI_REG16(0x6f12), 0x8688 },
	{ CCI_REG16(0x6f12), 0x1336 },
	{ CCI_REG16(0x6f12), 0x1600 },
	{ CCI_REG16(0x6f12), 0x2206 },
	{ CCI_REG16(0x6f12), 0x9305 },
	{ CCI_REG16(0x6f12), 0x0010 },
	{ CCI_REG16(0x6f12), 0x1385 },
	{ CCI_REG16(0x6f12), 0xa440 },
	{ CCI_REG16(0x6f12), 0x9780 },
	{ CCI_REG16(0x6f12), 0xfffb },
	{ CCI_REG16(0x6f12), 0xe780 },
	{ CCI_REG16(0x6f12), 0xa034 },
	{ CCI_REG16(0x6f12), 0x97f0 },
	{ CCI_REG16(0x6f12), 0x00fc },
	{ CCI_REG16(0x6f12), 0xe780 },
	{ CCI_REG16(0x6f12), 0x6083 },
	{ CCI_REG16(0x6f12), 0x8547 },
	{ CCI_REG16(0x6f12), 0x6301 },
	{ CCI_REG16(0x6f12), 0xf50a },
	{ CCI_REG16(0x6f12), 0x0357 },
	{ CCI_REG16(0x6f12), 0xc445 },
	{ CCI_REG16(0x6f12), 0x8357 },
	{ CCI_REG16(0x6f12), 0xe445 },
	{ CCI_REG16(0x6f12), 0xb7f6 },
	{ CCI_REG16(0x6f12), 0x0040 },
	{ CCI_REG16(0x6f12), 0x3307 },
	{ CCI_REG16(0x6f12), 0xe040 },
	{ CCI_REG16(0x6f12), 0x4207 },
	{ CCI_REG16(0x6f12), 0xb307 },
	{ CCI_REG16(0x6f12), 0xf040 },
	{ CCI_REG16(0x6f12), 0xb240 },
	{ CCI_REG16(0x6f12), 0x2244 },
	{ CCI_REG16(0x6f12), 0x4183 },
	{ CCI_REG16(0x6f12), 0xc207 },
	{ CCI_REG16(0x6f12), 0x239a },
	{ CCI_REG16(0x6f12), 0xe64e },
	{ CCI_REG16(0x6f12), 0xc183 },
	{ CCI_REG16(0x6f12), 0x239b },
	{ CCI_REG16(0x6f12), 0xf64e },
	{ CCI_REG16(0x6f12), 0x9244 },
	{ CCI_REG16(0x6f12), 0x4101 },
	{ CCI_REG16(0x6f12), 0x8280 },
	{ CCI_REG16(0x6f12), 0xb7c7 },
	{ CCI_REG16(0x6f12), 0x0040 },
	{ CCI_REG16(0x6f12), 0x03d7 },
	{ CCI_REG16(0x6f12), 0xa720 },
	{ CCI_REG16(0x6f12), 0x97f7 },
	{ CCI_REG16(0x6f12), 0xffff },
	{ CCI_REG16(0x6f12), 0x83d7 },
	{ CCI_REG16(0x6f12), 0x4782 },
	{ CCI_REG16(0x6f12), 0xb3b7 },
	{ CCI_REG16(0x6f12), 0xe700 },
	{ CCI_REG16(0x6f12), 0xbdfb },
	{ CCI_REG16(0x6f12), 0x2244 },
	{ CCI_REG16(0x6f12), 0xb240 },
	{ CCI_REG16(0x6f12), 0x9244 },
	{ CCI_REG16(0x6f12), 0x3d65 },
	{ CCI_REG16(0x6f12), 0x0146 },
	{ CCI_REG16(0x6f12), 0x8545 },
	{ CCI_REG16(0x6f12), 0x1305 },
	{ CCI_REG16(0x6f12), 0xe547 },
	{ CCI_REG16(0x6f12), 0x4101 },
	{ CCI_REG16(0x6f12), 0x1783 },
	{ CCI_REG16(0x6f12), 0xfffb },
	{ CCI_REG16(0x6f12), 0x6700 },
	{ CCI_REG16(0x6f12), 0xe32d },
	{ CCI_REG16(0x6f12), 0x8280 },
	{ CCI_REG16(0x6f12), 0x97e7 },
	{ CCI_REG16(0x6f12), 0xffff },
	{ CCI_REG16(0x6f12), 0x83d7 },
	{ CCI_REG16(0x6f12), 0xa77f },
	{ CCI_REG16(0x6f12), 0x17c7 },
	{ CCI_REG16(0x6f12), 0xffff },
	{ CCI_REG16(0x6f12), 0x0357 },
	{ CCI_REG16(0x6f12), 0xc7fd },
	{ CCI_REG16(0x6f12), 0xb3b7 },
	{ CCI_REG16(0x6f12), 0xe700 },
	{ CCI_REG16(0x6f12), 0x89b7 },
	{ CCI_REG16(0x6f12), 0x17e6 },
	{ CCI_REG16(0x6f12), 0xffff },
	{ CCI_REG16(0x6f12), 0x0346 },
	{ CCI_REG16(0x6f12), 0x667e },
	{ CCI_REG16(0x6f12), 0x2206 },
	{ CCI_REG16(0x6f12), 0x9305 },
	{ CCI_REG16(0x6f12), 0x0010 },
	{ CCI_REG16(0x6f12), 0x1385 },
	{ CCI_REG16(0x6f12), 0xa440 },
	{ CCI_REG16(0x6f12), 0x9780 },
	{ CCI_REG16(0x6f12), 0xfffb },
	{ CCI_REG16(0x6f12), 0xe780 },
	{ CCI_REG16(0x6f12), 0xc02a },
	{ CCI_REG16(0x6f12), 0x97e0 },
	{ CCI_REG16(0x6f12), 0x00fc },
	{ CCI_REG16(0x6f12), 0xe780 },
	{ CCI_REG16(0x6f12), 0x8079 },
	{ CCI_REG16(0x6f12), 0x8547 },
	{ CCI_REG16(0x6f12), 0xe303 },
	{ CCI_REG16(0x6f12), 0xf5f6 },
	{ CCI_REG16(0x6f12), 0x17e7 },
	{ CCI_REG16(0x6f12), 0xffff },
	{ CCI_REG16(0x6f12), 0x0357 },
	{ CCI_REG16(0x6f12), 0x077c },
	{ CCI_REG16(0x6f12), 0xb7f7 },
	{ CCI_REG16(0x6f12), 0x0040 },
	{ CCI_REG16(0x6f12), 0x239a },
	{ CCI_REG16(0x6f12), 0xe74e },
	{ CCI_REG16(0x6f12), 0xb240 },
	{ CCI_REG16(0x6f12), 0x2244 },
	{ CCI_REG16(0x6f12), 0x17e7 },
	{ CCI_REG16(0x6f12), 0xffff },
	{ CCI_REG16(0x6f12), 0x0357 },
	{ CCI_REG16(0x6f12), 0xe77a },
	{ CCI_REG16(0x6f12), 0x239b },
	{ CCI_REG16(0x6f12), 0xe74e },
	{ CCI_REG16(0x6f12), 0x9244 },
	{ CCI_REG16(0x6f12), 0x4101 },
	{ CCI_REG16(0x6f12), 0x8280 },
	{ CCI_REG16(0x6f12), 0x8547 },
	{ CCI_REG16(0x6f12), 0x6303 },
	{ CCI_REG16(0x6f12), 0xf500 },
	{ CCI_REG16(0x6f12), 0x8280 },
	{ CCI_REG16(0x6f12), 0x97d7 },
	{ CCI_REG16(0x6f12), 0xffff },
	{ CCI_REG16(0x6f12), 0x83c7 },
	{ CCI_REG16(0x6f12), 0x27ba },
	{ CCI_REG16(0x6f12), 0xfddb },
	{ CCI_REG16(0x6f12), 0x4111 },
	{ CCI_REG16(0x6f12), 0x8145 },
	{ CCI_REG16(0x6f12), 0x06c6 },
	{ CCI_REG16(0x6f12), 0x97a0 },
	{ CCI_REG16(0x6f12), 0xfffb },
	{ CCI_REG16(0x6f12), 0xe780 },
	{ CCI_REG16(0x6f12), 0xa07e },
	{ CCI_REG16(0x6f12), 0x97f7 },
	{ CCI_REG16(0x6f12), 0xffff },
	{ CCI_REG16(0x6f12), 0x9387 },
	{ CCI_REG16(0x6f12), 0x6711 },
	{ CCI_REG16(0x6f12), 0x9843 },
	{ CCI_REG16(0x6f12), 0x97d5 },
	{ CCI_REG16(0x6f12), 0xffff },
	{ CCI_REG16(0x6f12), 0x9385 },
	{ CCI_REG16(0x6f12), 0xc5f0 },
	{ CCI_REG16(0x6f12), 0xdc41 },
	{ CCI_REG16(0x6f12), 0x0356 },
	{ CCI_REG16(0x6f12), 0x2734 },
	{ CCI_REG16(0x6f12), 0x8566 },
	{ CCI_REG16(0x6f12), 0xba96 },
	{ CCI_REG16(0x6f12), 0x1355 },
	{ CCI_REG16(0x6f12), 0x8600 },
	{ CCI_REG16(0x6f12), 0x2386 },
	{ CCI_REG16(0x6f12), 0xa706 },
	{ CCI_REG16(0x6f12), 0x2385 },
	{ CCI_REG16(0x6f12), 0xc706 },
	{ CCI_REG16(0x6f12), 0x0356 },
	{ CCI_REG16(0x6f12), 0x0740 },
	{ CCI_REG16(0x6f12), 0x03a8 },
	{ CCI_REG16(0x6f12), 0x0500 },
	{ CCI_REG16(0x6f12), 0x8145 },
	{ CCI_REG16(0x6f12), 0x1355 },
	{ CCI_REG16(0x6f12), 0x8600 },
	{ CCI_REG16(0x6f12), 0x238e },
	{ CCI_REG16(0x6f12), 0xa708 },
	{ CCI_REG16(0x6f12), 0x238f },
	{ CCI_REG16(0x6f12), 0xc708 },
	{ CCI_REG16(0x6f12), 0x0346 },
	{ CCI_REG16(0x6f12), 0x0711 },
	{ CCI_REG16(0x6f12), 0x0145 },
	{ CCI_REG16(0x6f12), 0x2383 },
	{ CCI_REG16(0x6f12), 0xc700 },
	{ CCI_REG16(0x6f12), 0x83c6 },
	{ CCI_REG16(0x6f12), 0x16b0 },
	{ CCI_REG16(0x6f12), 0x238b },
	{ CCI_REG16(0x6f12), 0xd70a },
	{ CCI_REG16(0x6f12), 0x8347 },
	{ CCI_REG16(0x6f12), 0xa710 },
	{ CCI_REG16(0x6f12), 0x230f },
	{ CCI_REG16(0x6f12), 0xf80a },
	{ CCI_REG16(0x6f12), 0xb240 },
	{ CCI_REG16(0x6f12), 0x4101 },
	{ CCI_REG16(0x6f12), 0x17a3 },
	{ CCI_REG16(0x6f12), 0xfffb },
	{ CCI_REG16(0x6f12), 0x6700 },
	{ CCI_REG16(0x6f12), 0x6378 },
	{ CCI_REG16(0x6f12), 0x4111 },
	{ CCI_REG16(0x6f12), 0x22c4 },
	{ CCI_REG16(0x6f12), 0x17b4 },
	{ CCI_REG16(0x6f12), 0xffff },
	{ CCI_REG16(0x6f12), 0x1304 },
	{ CCI_REG16(0x6f12), 0x64de },
	{ CCI_REG16(0x6f12), 0x26c2 },
	{ CCI_REG16(0x6f12), 0x4ac0 },
	{ CCI_REG16(0x6f12), 0x06c6 },
	{ CCI_REG16(0x6f12), 0x0329 },
	{ CCI_REG16(0x6f12), 0x8400 },
	{ CCI_REG16(0x6f12), 0x97b4 },
	{ CCI_REG16(0x6f12), 0xffff },
	{ CCI_REG16(0x6f12), 0x83d4 },
	{ CCI_REG16(0x6f12), 0x04de },
	{ CCI_REG16(0x6f12), 0x9700 },
	{ CCI_REG16(0x6f12), 0x00fc },
	{ CCI_REG16(0x6f12), 0xe780 },
	{ CCI_REG16(0x6f12), 0x80d4 },
	{ CCI_REG16(0x6f12), 0x1840 },
	{ CCI_REG16(0x6f12), 0x8567 },
	{ CCI_REG16(0x6f12), 0x9386 },
	{ CCI_REG16(0x6f12), 0x07f0 },
	{ CCI_REG16(0x6f12), 0x758f },
	{ CCI_REG16(0x6f12), 0x9387 },
	{ CCI_REG16(0x6f12), 0x07e0 },
	{ CCI_REG16(0x6f12), 0x631a },
	{ CCI_REG16(0x6f12), 0xf702 },
	{ CCI_REG16(0x6f12), 0x97f7 },
	{ CCI_REG16(0x6f12), 0xffff },
	{ CCI_REG16(0x6f12), 0x9387 },
	{ CCI_REG16(0x6f12), 0xa707 },
	{ CCI_REG16(0x6f12), 0x9843 },
	{ CCI_REG16(0x6f12), 0x97b7 },
	{ CCI_REG16(0x6f12), 0xffff },
	{ CCI_REG16(0x6f12), 0x23a4 },
	{ CCI_REG16(0x6f12), 0x07da },
	{ CCI_REG16(0x6f12), 0x8567 },
	{ CCI_REG16(0x6f12), 0x9387 },
	{ CCI_REG16(0x6f12), 0x1711 },
	{ CCI_REG16(0x6f12), 0x97b6 },
	{ CCI_REG16(0x6f12), 0xffff },
	{ CCI_REG16(0x6f12), 0x239f },
	{ CCI_REG16(0x6f12), 0xf6d8 },
	{ CCI_REG16(0x6f12), 0x231f },
	{ CCI_REG16(0x6f12), 0x070c },
	{ CCI_REG16(0x6f12), 0x2310 },
	{ CCI_REG16(0x6f12), 0x070e },
	{ CCI_REG16(0x6f12), 0x97b7 },
	{ CCI_REG16(0x6f12), 0xffff },
	{ CCI_REG16(0x6f12), 0x239c },
	{ CCI_REG16(0x6f12), 0x07d8 },
	{ CCI_REG16(0x6f12), 0x97b7 },
	{ CCI_REG16(0x6f12), 0xffff },
	{ CCI_REG16(0x6f12), 0x23a5 },
	{ CCI_REG16(0x6f12), 0x27d9 },
	{ CCI_REG16(0x6f12), 0x97b7 },
	{ CCI_REG16(0x6f12), 0xffff },
	{ CCI_REG16(0x6f12), 0x2393 },
	{ CCI_REG16(0x6f12), 0x97d8 },
	{ CCI_REG16(0x6f12), 0x83c7 },
	{ CCI_REG16(0x6f12), 0xa15d },
	{ CCI_REG16(0x6f12), 0x91c7 },
	{ CCI_REG16(0x6f12), 0x97b5 },
	{ CCI_REG16(0x6f12), 0xffff },
	{ CCI_REG16(0x6f12), 0x83d5 },
	{ CCI_REG16(0x6f12), 0x8575 },
	{ CCI_REG16(0x6f12), 0xa1e5 },
	{ CCI_REG16(0x6f12), 0x83c7 },
	{ CCI_REG16(0x6f12), 0x1178 },
	{ CCI_REG16(0x6f12), 0x9dcb },
	{ CCI_REG16(0x6f12), 0x9797 },
	{ CCI_REG16(0x6f12), 0xffff },
	{ CCI_REG16(0x6f12), 0x83c7 },
	{ CCI_REG16(0x6f12), 0xd7a5 },
	{ CCI_REG16(0x6f12), 0x95c7 },
	{ CCI_REG16(0x6f12), 0x83c7 },
	{ CCI_REG16(0x6f12), 0xa15b },
	{ CCI_REG16(0x6f12), 0x9dc3 },
	{ CCI_REG16(0x6f12), 0x97b7 },
	{ CCI_REG16(0x6f12), 0xffff },
	{ CCI_REG16(0x6f12), 0x83d7 },
	{ CCI_REG16(0x6f12), 0x8773 },
	{ CCI_REG16(0x6f12), 0x91ef },
	{ CCI_REG16(0x6f12), 0x2244 },
	{ CCI_REG16(0x6f12), 0xb240 },
	{ CCI_REG16(0x6f12), 0x9244 },
	{ CCI_REG16(0x6f12), 0x0249 },
	{ CCI_REG16(0x6f12), 0x0565 },
	{ CCI_REG16(0x6f12), 0x8545 },
	{ CCI_REG16(0x6f12), 0x1305 },
	{ CCI_REG16(0x6f12), 0x35e0 },
	{ CCI_REG16(0x6f12), 0x4101 },
	{ CCI_REG16(0x6f12), 0x1783 },
	{ CCI_REG16(0x6f12), 0xfffb },
	{ CCI_REG16(0x6f12), 0x6700 },
	{ CCI_REG16(0x6f12), 0x4344 },
	{ CCI_REG16(0x6f12), 0xb240 },
	{ CCI_REG16(0x6f12), 0x2244 },
	{ CCI_REG16(0x6f12), 0x9244 },
	{ CCI_REG16(0x6f12), 0x0249 },
	{ CCI_REG16(0x6f12), 0x4101 },
	{ CCI_REG16(0x6f12), 0x8280 },
	{ CCI_REG16(0x6f12), 0x83c7 },
	{ CCI_REG16(0x6f12), 0xf177 },
	{ CCI_REG16(0x6f12), 0x9d8d },
	{ CCI_REG16(0x6f12), 0xc205 },
	{ CCI_REG16(0x6f12), 0xc181 },
	{ CCI_REG16(0x6f12), 0x83d7 },
	{ CCI_REG16(0x6f12), 0xe15d },
	{ CCI_REG16(0x6f12), 0xe386 },
	{ CCI_REG16(0x6f12), 0xb7fa },
	{ CCI_REG16(0x6f12), 0x0565 },
	{ CCI_REG16(0x6f12), 0x1305 },
	{ CCI_REG16(0x6f12), 0x15e0 },
	{ CCI_REG16(0x6f12), 0x9780 },
	{ CCI_REG16(0x6f12), 0xfffb },
	{ CCI_REG16(0x6f12), 0xe780 },
	{ CCI_REG16(0x6f12), 0x8041 },
	{ CCI_REG16(0x6f12), 0x69bf },
	{ CCI_REG16(0x6f12), 0x5c45 },
	{ CCI_REG16(0x6f12), 0x0328 },
	{ CCI_REG16(0x6f12), 0x0501 },
	{ CCI_REG16(0x6f12), 0x85e2 },
	{ CCI_REG16(0x6f12), 0x63c7 },
	{ CCI_REG16(0x6f12), 0xb700 },
	{ CCI_REG16(0x6f12), 0x8d8f },
	{ CCI_REG16(0x6f12), 0x5cd9 },
	{ CCI_REG16(0x6f12), 0x232c },
	{ CCI_REG16(0x6f12), 0x0502 },
	{ CCI_REG16(0x6f12), 0x8280 },
	{ CCI_REG16(0x6f12), 0xb387 },
	{ CCI_REG16(0x6f12), 0xf540 },
	{ CCI_REG16(0x6f12), 0xb3e7 },
	{ CCI_REG16(0x6f12), 0x0703 },
	{ CCI_REG16(0x6f12), 0x232a },
	{ CCI_REG16(0x6f12), 0x0502 },
	{ CCI_REG16(0x6f12), 0x1cdd },
	{ CCI_REG16(0x6f12), 0x8280 },
	{ CCI_REG16(0x6f12), 0x4c49 },
	{ CCI_REG16(0x6f12), 0x1441 },
	{ CCI_REG16(0x6f12), 0x7d17 },
	{ CCI_REG16(0x6f12), 0xb305 },
	{ CCI_REG16(0x6f12), 0xb802 },
	{ CCI_REG16(0x6f12), 0x3a96 },
	{ CCI_REG16(0x6f12), 0xb387 },
	{ CCI_REG16(0x6f12), 0xf640 },
	{ CCI_REG16(0x6f12), 0x918e },
	{ CCI_REG16(0x6f12), 0xfd16 },
	{ CCI_REG16(0x6f12), 0x8d8f },
	{ CCI_REG16(0x6f12), 0x63c8 },
	{ CCI_REG16(0x6f12), 0xd700 },
	{ CCI_REG16(0x6f12), 0x958f },
	{ CCI_REG16(0x6f12), 0x7d18 },
	{ CCI_REG16(0x6f12), 0x5cd9 },
	{ CCI_REG16(0x6f12), 0x232c },
	{ CCI_REG16(0x6f12), 0x0503 },
	{ CCI_REG16(0x6f12), 0x8280 },
	{ CCI_REG16(0x6f12), 0xb307 },
	{ CCI_REG16(0x6f12), 0xf640 },
	{ CCI_REG16(0x6f12), 0xb3e7 },
	{ CCI_REG16(0x6f12), 0x0703 },
	{ CCI_REG16(0x6f12), 0x232a },
	{ CCI_REG16(0x6f12), 0x0502 },
	{ CCI_REG16(0x6f12), 0x1cdd },
	{ CCI_REG16(0x6f12), 0x8280 },
	{ CCI_REG16(0x6f12), 0x3336 },
	{ CCI_REG16(0x6f12), 0xa000 },
	{ CCI_REG16(0x6f12), 0x8945 },
	{ CCI_REG16(0x6f12), 0x2d65 },
	{ CCI_REG16(0x6f12), 0x1783 },
	{ CCI_REG16(0x6f12), 0xfffb },
	{ CCI_REG16(0x6f12), 0x6700 },
	{ CCI_REG16(0x6f12), 0x8306 },
	{ CCI_REG16(0x6f12), 0x4111 },
	{ CCI_REG16(0x6f12), 0x9707 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x6f12), 0x9387 },
	{ CCI_REG16(0x6f12), 0xc717 },
	{ CCI_REG16(0x6f12), 0x22c4 },
	{ CCI_REG16(0x6f12), 0x8043 },
	{ CCI_REG16(0x6f12), 0x26c2 },
	{ CCI_REG16(0x6f12), 0x0146 },
	{ CCI_REG16(0x6f12), 0xaa84 },
	{ CCI_REG16(0x6f12), 0xa285 },
	{ CCI_REG16(0x6f12), 0x1145 },
	{ CCI_REG16(0x6f12), 0x06c6 },
	{ CCI_REG16(0x6f12), 0x9780 },
	{ CCI_REG16(0x6f12), 0xfffb },
	{ CCI_REG16(0x6f12), 0xe780 },
	{ CCI_REG16(0x6f12), 0xa00f },
	{ CCI_REG16(0x6f12), 0x9707 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x6f12), 0x83c7 },
	{ CCI_REG16(0x6f12), 0x0716 },
	{ CCI_REG16(0x6f12), 0x95eb },
	{ CCI_REG16(0x6f12), 0x97d7 },
	{ CCI_REG16(0x6f12), 0xffff },
	{ CCI_REG16(0x6f12), 0x83d7 },
	{ CCI_REG16(0x6f12), 0xa7da },
	{ CCI_REG16(0x6f12), 0x89c7 },
	{ CCI_REG16(0x6f12), 0x9848 },
	{ CCI_REG16(0x6f12), 0x8947 },
	{ CCI_REG16(0x6f12), 0x6315 },
	{ CCI_REG16(0x6f12), 0xf702 },
	{ CCI_REG16(0x6f12), 0x2685 },
	{ CCI_REG16(0x6f12), 0x9760 },
	{ CCI_REG16(0x6f12), 0x01fc },
	{ CCI_REG16(0x6f12), 0xe780 },
	{ CCI_REG16(0x6f12), 0x0018 },
	{ CCI_REG16(0x6f12), 0xa285 },
	{ CCI_REG16(0x6f12), 0x2244 },
	{ CCI_REG16(0x6f12), 0xb240 },
	{ CCI_REG16(0x6f12), 0x9244 },
	{ CCI_REG16(0x6f12), 0x0546 },
	{ CCI_REG16(0x6f12), 0x1145 },
	{ CCI_REG16(0x6f12), 0x4101 },
	{ CCI_REG16(0x6f12), 0x1783 },
	{ CCI_REG16(0x6f12), 0xfffb },
	{ CCI_REG16(0x6f12), 0x6700 },
	{ CCI_REG16(0x6f12), 0xe30b },
	{ CCI_REG16(0x6f12), 0x0547 },
	{ CCI_REG16(0x6f12), 0xe39f },
	{ CCI_REG16(0x6f12), 0xe7fc },
	{ CCI_REG16(0x6f12), 0x0145 },
	{ CCI_REG16(0x6f12), 0x6937 },
	{ CCI_REG16(0x6f12), 0x0545 },
	{ CCI_REG16(0x6f12), 0x5937 },
	{ CCI_REG16(0x6f12), 0xc9bf },
	{ CCI_REG16(0x6f12), 0x9707 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x6f12), 0x9387 },
	{ CCI_REG16(0x6f12), 0x67c7 },
	{ CCI_REG16(0x6f12), 0x4111 },
	{ CCI_REG16(0x6f12), 0x0146 },
	{ CCI_REG16(0x6f12), 0x9705 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x6f12), 0x9385 },
	{ CCI_REG16(0x6f12), 0x8596 },
	{ CCI_REG16(0x6f12), 0x17f7 },
	{ CCI_REG16(0x6f12), 0xffff },
	{ CCI_REG16(0x6f12), 0x2324 },
	{ CCI_REG16(0x6f12), 0xf7fc },
	{ CCI_REG16(0x6f12), 0x1735 },
	{ CCI_REG16(0x6f12), 0x00fc },
	{ CCI_REG16(0x6f12), 0x1305 },
	{ CCI_REG16(0x6f12), 0xc5d2 },
	{ CCI_REG16(0x6f12), 0x9707 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x6f12), 0x9387 },
	{ CCI_REG16(0x6f12), 0xe7d8 },
	{ CCI_REG16(0x6f12), 0x17f7 },
	{ CCI_REG16(0x6f12), 0xffff },
	{ CCI_REG16(0x6f12), 0x2326 },
	{ CCI_REG16(0x6f12), 0xf7f2 },
	{ CCI_REG16(0x6f12), 0x06c6 },
	{ CCI_REG16(0x6f12), 0x9730 },
	{ CCI_REG16(0x6f12), 0x00fc },
	{ CCI_REG16(0x6f12), 0xe780 },
	{ CCI_REG16(0x6f12), 0x6095 },
	{ CCI_REG16(0x6f12), 0x9707 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x6f12), 0x23a5 },
	{ CCI_REG16(0x6f12), 0xa70c },
	{ CCI_REG16(0x6f12), 0x0146 },
	{ CCI_REG16(0x6f12), 0x9705 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x6f12), 0x9385 },
	{ CCI_REG16(0x6f12), 0x6594 },
	{ CCI_REG16(0x6f12), 0x17d5 },
	{ CCI_REG16(0x6f12), 0x00fc },
	{ CCI_REG16(0x6f12), 0x1305 },
	{ CCI_REG16(0x6f12), 0x655f },
	{ CCI_REG16(0x6f12), 0x9730 },
	{ CCI_REG16(0x6f12), 0x00fc },
	{ CCI_REG16(0x6f12), 0xe780 },
	{ CCI_REG16(0x6f12), 0x4093 },
	{ CCI_REG16(0x6f12), 0x9707 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x6f12), 0x23a2 },
	{ CCI_REG16(0x6f12), 0xa70c },
	{ CCI_REG16(0x6f12), 0x0146 },
	{ CCI_REG16(0x6f12), 0x9705 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x6f12), 0x9385 },
	{ CCI_REG16(0x6f12), 0xe59c },
	{ CCI_REG16(0x6f12), 0x17a5 },
	{ CCI_REG16(0x6f12), 0x00fc },
	{ CCI_REG16(0x6f12), 0x1305 },
	{ CCI_REG16(0x6f12), 0x85d6 },
	{ CCI_REG16(0x6f12), 0x9730 },
	{ CCI_REG16(0x6f12), 0x00fc },
	{ CCI_REG16(0x6f12), 0xe780 },
	{ CCI_REG16(0x6f12), 0x2091 },
	{ CCI_REG16(0x6f12), 0x9707 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x6f12), 0x23ad },
	{ CCI_REG16(0x6f12), 0xa708 },
	{ CCI_REG16(0x6f12), 0x0146 },
	{ CCI_REG16(0x6f12), 0x9705 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x6f12), 0x9385 },
	{ CCI_REG16(0x6f12), 0x45b9 },
	{ CCI_REG16(0x6f12), 0x17a5 },
	{ CCI_REG16(0x6f12), 0x00fc },
	{ CCI_REG16(0x6f12), 0x1305 },
	{ CCI_REG16(0x6f12), 0x2528 },
	{ CCI_REG16(0x6f12), 0x9730 },
	{ CCI_REG16(0x6f12), 0x00fc },
	{ CCI_REG16(0x6f12), 0xe780 },
	{ CCI_REG16(0x6f12), 0x008f },
	{ CCI_REG16(0x6f12), 0x9707 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x6f12), 0x23aa },
	{ CCI_REG16(0x6f12), 0xa706 },
	{ CCI_REG16(0x6f12), 0x0146 },
	{ CCI_REG16(0x6f12), 0x9705 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x6f12), 0x9385 },
	{ CCI_REG16(0x6f12), 0x45ee },
	{ CCI_REG16(0x6f12), 0x1765 },
	{ CCI_REG16(0x6f12), 0x01fc },
	{ CCI_REG16(0x6f12), 0x1305 },
	{ CCI_REG16(0x6f12), 0xc509 },
	{ CCI_REG16(0x6f12), 0x9730 },
	{ CCI_REG16(0x6f12), 0x00fc },
	{ CCI_REG16(0x6f12), 0xe780 },
	{ CCI_REG16(0x6f12), 0xe08c },
	{ CCI_REG16(0x6f12), 0x9707 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x6f12), 0x23a5 },
	{ CCI_REG16(0x6f12), 0xa704 },
	{ CCI_REG16(0x6f12), 0x9707 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x6f12), 0x9387 },
	{ CCI_REG16(0x6f12), 0x87d6 },
	{ CCI_REG16(0x6f12), 0x0146 },
	{ CCI_REG16(0x6f12), 0x9705 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x6f12), 0x9385 },
	{ CCI_REG16(0x6f12), 0xc585 },
	{ CCI_REG16(0x6f12), 0x17d5 },
	{ CCI_REG16(0x6f12), 0x00fc },
	{ CCI_REG16(0x6f12), 0x1305 },
	{ CCI_REG16(0x6f12), 0x0505 },
	{ CCI_REG16(0x6f12), 0x17f7 },
	{ CCI_REG16(0x6f12), 0xffff },
	{ CCI_REG16(0x6f12), 0x2324 },
	{ CCI_REG16(0x6f12), 0xf7ec },
	{ CCI_REG16(0x6f12), 0x9730 },
	{ CCI_REG16(0x6f12), 0x00fc },
	{ CCI_REG16(0x6f12), 0xe780 },
	{ CCI_REG16(0x6f12), 0xc089 },
	{ CCI_REG16(0x6f12), 0xb240 },
	{ CCI_REG16(0x6f12), 0x9707 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x6f12), 0x23a9 },
	{ CCI_REG16(0x6f12), 0xa700 },
	{ CCI_REG16(0x6f12), 0x4101 },
	{ CCI_REG16(0x6f12), 0x8280 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x602a), 0x705a },
	{ CCI_REG16(0x6f12), 0x0101 },
	{ CCI_REG16(0x602a), 0x63d0 },
	{ CCI_REG16(0x6f12), 0x143c },
	{ CCI_REG16(0x602a), 0x565e },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x602a), 0x564a },
	{ CCI_REG16(0x6f12), 0x0020 },
	{ CCI_REG16(0x6f12), 0x0020 },
	{ CCI_REG16(0x6f12), 0x0020 },
	{ CCI_REG16(0x6f12), 0x0020 },
	{ CCI_REG16(0x6f12), 0x0020 },
	{ CCI_REG16(0x6f12), 0x0020 },
	{ CCI_REG16(0x6f12), 0x0020 },
	{ CCI_REG16(0x6f12), 0x0020 },
	{ CCI_REG16(0x6f12), 0x0020 },
	{ CCI_REG16(0x6f12), 0x0020 },
	{ CCI_REG16(0x602a), 0x71d8 },
	{ CCI_REG16(0x6f12), 0x803a },
	{ CCI_REG16(0x602a), 0x71de },
	{ CCI_REG16(0x6f12), 0x803a },
	{ CCI_REG16(0x602a), 0x61dc },
	{ CCI_REG16(0x6f12), 0x803a },
	{ CCI_REG16(0x602a), 0x631c },
	{ CCI_REG16(0x6f12), 0x4000 },
	{ CCI_REG16(0x602a), 0x635e },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x602a), 0x637e },
	{ CCI_REG16(0x6f12), 0x0110 },
	{ CCI_REG16(0x6f12), 0x0001 },
	{ CCI_REG16(0x602a), 0x650a },
	{ CCI_REG16(0x6f12), 0xe039 },
	{ CCI_REG16(0x6f12), 0xf039 },
	{ CCI_REG16(0x6f12), 0x003a },
	{ CCI_REG16(0x6f12), 0x103a },
	{ CCI_REG16(0x6f12), 0x203a },
	{ CCI_REG16(0x6028), 0x4000 },
	{ CCI_REG8(0x0bcc), 0x00 },
	{ CCI_REG8(0x0106), 0x01 },
	{ CCI_REG16(0x6028), 0x2400 },
	{ CCI_REG16(0x602a), 0x708a },
	{ CCI_REG16(0x6f12), 0x0064 },
};

static const struct cci_reg_sequence s5kgw3_mode_4640x3472_regs[] = {
	{ CCI_REG16(0x6028), 0x4000 },
	{ CCI_REG16(0x6214), 0xff7d },
	{ CCI_REG16(0x6218), 0x0000 },
	{ CCI_REG16(0x6028), 0x2400 },
	{ CCI_REG16(0x602a), 0x64e6 },
	{ CCI_REG16(0x6f12), 0x0100 },
	{ CCI_REG16(0x6f12), 0x2823 },
	{ CCI_REG16(0x602a), 0x6a36 },
	{ CCI_REG16(0x6f12), 0xca44 },
	{ CCI_REG16(0x6f12), 0x6f00 },
	{ CCI_REG16(0x6f12), 0xa020 },
	{ CCI_REG16(0xf46a), 0xae80 },
	{ CCI_REG16(0xf44a), 0x0007 },
	{ CCI_REG16(0xf44c), 0x1209 },
	{ CCI_REG16(0x602a), 0x68ce },
	{ CCI_REG16(0x6f12), 0x560b },
	{ CCI_REG16(0x602a), 0x6536 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x6f12), 0x0100 },
	{ CCI_REG16(0x6f12), 0x0a00 },
	{ CCI_REG16(0x602a), 0x6f64 },
	{ CCI_REG16(0x6f12), 0x1a00 },
	{ CCI_REG16(0x602a), 0x68bc },
	{ CCI_REG16(0x6f12), 0x0706 },
	{ CCI_REG16(0x6f12), 0x0b03 },
	{ CCI_REG16(0x6f12), 0x0b00 },
	{ CCI_REG16(0x6f12), 0x111c },
	{ CCI_REG16(0x602a), 0x7270 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x602a), 0x7274 },
	{ CCI_REG16(0x6f12), 0x8000 },
	{ CCI_REG16(0x602a), 0x70f0 },
	{ CCI_REG16(0x6f12), 0x0300 },
	{ CCI_REG16(0x602a), 0x70f8 },
	{ CCI_REG16(0x6f12), 0x0003 },
	{ CCI_REG16(0x6f12), 0x0106 },
	{ CCI_REG16(0x602a), 0x7060 },
	{ CCI_REG16(0x6f12), 0x0100 },
	{ CCI_REG16(0x602a), 0x6926 },
	{ CCI_REG16(0x6f12), 0x0301 },
	{ CCI_REG16(0x6f12), 0xbf00 },
	{ CCI_REG16(0x602a), 0x65ba },
	{ CCI_REG16(0x6f12), 0x1900 },
	{ CCI_REG16(0x602a), 0x65ea },
	{ CCI_REG16(0x6f12), 0x0300 },
	{ CCI_REG16(0x6f12), 0x0e00 },
	{ CCI_REG16(0x6f12), 0x0800 },
	{ CCI_REG16(0x602a), 0x65f2 },
	{ CCI_REG16(0x6f12), 0x3300 },
	{ CCI_REG16(0x6f12), 0x2f00 },
	{ CCI_REG16(0x602a), 0x664a },
	{ CCI_REG16(0x6f12), 0x0b00 },
	{ CCI_REG16(0x602a), 0x664e },
	{ CCI_REG16(0x6f12), 0x5c00 },
	{ CCI_REG16(0x602a), 0x66de },
	{ CCI_REG16(0x6f12), 0x0a00 },
	{ CCI_REG16(0x602a), 0x66e2 },
	{ CCI_REG16(0x6f12), 0x0400 },
	{ CCI_REG16(0x602a), 0x68aa },
	{ CCI_REG16(0x6f12), 0x5e01 },
	{ CCI_REG16(0x1134), 0x0000 },
	{ CCI_REG16(0x1136), 0x4844 },
	{ CCI_REG16(0x602a), 0x6a26 },
	{ CCI_REG16(0x6f12), 0x5000 },
	{ CCI_REG16(0x6f12), 0x5100 },
	{ CCI_REG16(0x602a), 0x68fe },
	{ CCI_REG16(0x6f12), 0xfe07 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x602a), 0x6908 },
	{ CCI_REG16(0x6f12), 0xbc00 },
	{ CCI_REG16(0x6f12), 0xfe07 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x602a), 0x6914 },
	{ CCI_REG16(0x6f12), 0x8400 },
	{ CCI_REG16(0x602a), 0x546a },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x602a), 0x5460 },
	{ CCI_REG16(0x6f12), 0x0600 },
	{ CCI_REG16(0x602a), 0x5464 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x602a), 0x5478 },
	{ CCI_REG16(0x6f12), 0x0700 },
	{ CCI_REG16(0x6f12), 0x0700 },
	{ CCI_REG16(0x602a), 0x5480 },
	{ CCI_REG16(0x6f12), 0x0500 },
	{ CCI_REG16(0x6f12), 0x0500 },
	{ CCI_REG16(0x602a), 0x548c },
	{ CCI_REG16(0x6f12), 0x4001 },
	{ CCI_REG16(0x602a), 0x5490 },
	{ CCI_REG16(0x6f12), 0x4001 },
	{ CCI_REG16(0x602a), 0x8500 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x602a), 0x653c },
	{ CCI_REG16(0x6f12), 0x0100 },
	{ CCI_REG16(0x0136), 0x1300 },
	{ CCI_REG16(0x013e), 0x00c8 },
	{ CCI_REG16(0x0304), 0x0003 },
	{ CCI_REG16(0x0306), 0x008c },
	{ CCI_REG16(0x030c), 0x0000 },
	{ CCI_REG16(0x030e), 0x0002 },
	{ CCI_REG16(0x0310), 0x0057 },
	{ CCI_REG16(0x0312), 0x0000 },
	{ CCI_REG16(0x602a), 0x68d0 },
	{ CCI_REG16(0x6f12), 0x9600 },
	{ CCI_REG16(0x602a), 0x720e },
	{ CCI_REG16(0x6f12), 0x0103 },
	{ CCI_REG16(0x0344), 0x0000 },
	{ CCI_REG16(0x0346), 0x0000 },
	{ CCI_REG16(0x0348), 0x245f },
	{ CCI_REG16(0x034a), 0x1b3f },
	{ CCI_REG16(0x034c), 0x1220 },
	{ CCI_REG16(0x034e), 0x0d90 },
	{ CCI_REG16(0x0350), 0x0008 },
	{ CCI_REG16(0x0352), 0x0008 },
	{ CCI_REG16(0x0340), 0x0e0e },
	{ CCI_REG16(0x0342), 0x1cd0 },
	{ CCI_REG16(0x0380), 0x0002 },
	{ CCI_REG16(0x0382), 0x0002 },
	{ CCI_REG16(0x0384), 0x0002 },
	{ CCI_REG16(0x0386), 0x0002 },
	{ CCI_REG16(0x0900), 0x0122 },
	{ CCI_REG16(0x602a), 0x63ae },
	{ CCI_REG16(0x6f12), 0x1000 },
	{ CCI_REG16(0x602a), 0x7048 },
	{ CCI_REG16(0x6f12), 0x0200 },
	{ CCI_REG16(0x602a), 0x71a2 },
	{ CCI_REG16(0x6f12), 0x0200 },
	{ CCI_REG16(0x602a), 0x6548 },
	{ CCI_REG16(0x6f12), 0x0101 },
	{ CCI_REG16(0x0118), 0x0001 },
	{ CCI_REG16(0x602a), 0x54c6 },
	{ CCI_REG16(0x6f12), 0x0a00 },
	{ CCI_REG16(0x602a), 0x708c },
	{ CCI_REG16(0x6f12), 0x0100 },
	{ CCI_REG16(0x602a), 0x71d4 },
	{ CCI_REG16(0x6f12), 0x0100 },
	{ CCI_REG16(0x011a), 0x0001 },
	{ CCI_REG16(0x602a), 0x5634 },
	{ CCI_REG16(0x6f12), 0x0041 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x602a), 0x5ac4 },
	{ CCI_REG16(0x6f12), 0x1106 },
	{ CCI_REG16(0x602a), 0x70a2 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x602a), 0x55c0 },
	{ CCI_REG16(0x6f12), 0x0001 },
	{ CCI_REG16(0x602a), 0x5602 },
	{ CCI_REG16(0x6f12), 0x5e01 },
	{ CCI_REG16(0x0bc8), 0x0002 },
	{ CCI_REG16(0x602a), 0x5528 },
	{ CCI_REG16(0x6f12), 0x0001 },
	{ CCI_REG16(0x602a), 0x5520 },
	{ CCI_REG16(0x6f12), 0x0002 },
	{ CCI_REG16(0x6f12), 0x0804 },
	{ CCI_REG16(0x6f12), 0x0100 },
	{ CCI_REG16(0x602a), 0x6a7e },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x602a), 0x5596 },
	{ CCI_REG16(0x6f12), 0x7701 },
	{ CCI_REG16(0x602a), 0x6f44 },
	{ CCI_REG16(0x6f12), 0x0500 },
	{ CCI_REG16(0x6f12), 0x0500 },
	{ CCI_REG16(0x0d00), 0x0101 },
	{ CCI_REG16(0x602a), 0x61ba },
	{ CCI_REG16(0x6f12), 0x0100 },
	{ CCI_REG16(0x602a), 0x5640 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x602a), 0x5cf2 },
	{ CCI_REG16(0x6f12), 0x0a00 },
	{ CCI_REG16(0x6f12), 0x0a00 },
	{ CCI_REG16(0x6f12), 0x0a00 },
	{ CCI_REG16(0x6f12), 0x0a00 },
	{ CCI_REG16(0x6f12), 0x0a00 },
	{ CCI_REG16(0x6f12), 0x1e00 },
	{ CCI_REG16(0x6f12), 0x1e00 },
	{ CCI_REG16(0x6f12), 0x1e00 },
	{ CCI_REG16(0x6f12), 0x1e00 },
	{ CCI_REG16(0x6f12), 0x1e00 },
	{ CCI_REG16(0x602a), 0x5d10 },
	{ CCI_REG16(0x6f12), 0x1400 },
	{ CCI_REG16(0x6f12), 0x1400 },
	{ CCI_REG16(0x6f12), 0x1400 },
	{ CCI_REG16(0x6f12), 0x1400 },
	{ CCI_REG16(0x6f12), 0x1400 },
	{ CCI_REG16(0x602a), 0x5df4 },
	{ CCI_REG16(0x6f12), 0x0a00 },
	{ CCI_REG16(0x6f12), 0x0a00 },
	{ CCI_REG16(0x6f12), 0x0a00 },
	{ CCI_REG16(0x6f12), 0x0a00 },
	{ CCI_REG16(0x6f12), 0x0a00 },
	{ CCI_REG16(0x6f12), 0x1e00 },
	{ CCI_REG16(0x6f12), 0x1e00 },
	{ CCI_REG16(0x6f12), 0x1e00 },
	{ CCI_REG16(0x6f12), 0x1e00 },
	{ CCI_REG16(0x6f12), 0x1e00 },
	{ CCI_REG16(0x602a), 0x5e12 },
	{ CCI_REG16(0x6f12), 0x1400 },
	{ CCI_REG16(0x6f12), 0x1400 },
	{ CCI_REG16(0x6f12), 0x1400 },
	{ CCI_REG16(0x6f12), 0x1400 },
	{ CCI_REG16(0x6f12), 0x1400 },
	{ CCI_REG16(0x602a), 0x5ef6 },
	{ CCI_REG16(0x6f12), 0x2d00 },
	{ CCI_REG16(0x6f12), 0x2d00 },
	{ CCI_REG16(0x6f12), 0x3700 },
	{ CCI_REG16(0x6f12), 0x3700 },
	{ CCI_REG16(0x6f12), 0x3700 },
	{ CCI_REG16(0x6f12), 0x1400 },
	{ CCI_REG16(0x6f12), 0x1400 },
	{ CCI_REG16(0x6f12), 0x1400 },
	{ CCI_REG16(0x6f12), 0x1400 },
	{ CCI_REG16(0x6f12), 0x1400 },
	{ CCI_REG16(0x602a), 0x5f14 },
	{ CCI_REG16(0x6f12), 0x1400 },
	{ CCI_REG16(0x6f12), 0x1400 },
	{ CCI_REG16(0x6f12), 0x1400 },
	{ CCI_REG16(0x6f12), 0x1400 },
	{ CCI_REG16(0x6f12), 0x1400 },
	{ CCI_REG16(0x602a), 0x5ff8 },
	{ CCI_REG16(0x6f12), 0x5000 },
	{ CCI_REG16(0x6f12), 0x5000 },
	{ CCI_REG16(0x6f12), 0x6400 },
	{ CCI_REG16(0x6f12), 0x6400 },
	{ CCI_REG16(0x6f12), 0x6400 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x602a), 0x6016 },
	{ CCI_REG16(0x6f12), 0x1400 },
	{ CCI_REG16(0x6f12), 0x1400 },
	{ CCI_REG16(0x6f12), 0x1400 },
	{ CCI_REG16(0x6f12), 0x1400 },
	{ CCI_REG16(0x6f12), 0x1400 },
	{ CCI_REG16(0x602a), 0x60fa },
	{ CCI_REG16(0x6f12), 0x5000 },
	{ CCI_REG16(0x6f12), 0x5000 },
	{ CCI_REG16(0x6f12), 0x6400 },
	{ CCI_REG16(0x6f12), 0x6400 },
	{ CCI_REG16(0x6f12), 0x6400 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x602a), 0x6118 },
	{ CCI_REG16(0x6f12), 0x1400 },
	{ CCI_REG16(0x6f12), 0x1400 },
	{ CCI_REG16(0x6f12), 0x1400 },
	{ CCI_REG16(0x6f12), 0x1400 },
	{ CCI_REG16(0x6f12), 0x1400 },
	{ CCI_REG16(0x602a), 0x5cda },
	{ CCI_REG16(0x6f12), 0xd403 },
	{ CCI_REG16(0x602a), 0x5ddc },
	{ CCI_REG16(0x6f12), 0xd403 },
	{ CCI_REG16(0x602a), 0x5ede },
	{ CCI_REG16(0x6f12), 0xd403 },
	{ CCI_REG16(0x602a), 0x5fe0 },
	{ CCI_REG16(0x6f12), 0xd403 },
	{ CCI_REG16(0x602a), 0x60e2 },
	{ CCI_REG16(0x6f12), 0xd403 },
	{ CCI_REG16(0x602a), 0x5cd8 },
	{ CCI_REG16(0x6f12), 0x6400 },
	{ CCI_REG16(0x602a), 0x5dda },
	{ CCI_REG16(0x6f12), 0x6400 },
	{ CCI_REG16(0x602a), 0x5edc },
	{ CCI_REG16(0x6f12), 0x6400 },
	{ CCI_REG16(0x602a), 0x5fde },
	{ CCI_REG16(0x6f12), 0x6400 },
	{ CCI_REG16(0x602a), 0x60e0 },
	{ CCI_REG16(0x6f12), 0x6400 },
	{ CCI_REG16(0x602a), 0x5ce4 },
	{ CCI_REG16(0x6f12), 0x1a00 },
	{ CCI_REG16(0x602a), 0x5de6 },
	{ CCI_REG16(0x6f12), 0x1a00 },
	{ CCI_REG16(0x602a), 0x5ee8 },
	{ CCI_REG16(0x6f12), 0x1a00 },
	{ CCI_REG16(0x602a), 0x5fea },
	{ CCI_REG16(0x6f12), 0x1a00 },
	{ CCI_REG16(0x602a), 0x60ec },
	{ CCI_REG16(0x6f12), 0x1a00 },
	{ CCI_REG16(0x0d02), 0x0101 },
	{ CCI_REG16(0x0d04), 0x0102 },
	{ CCI_REG16(0x0114), 0x0301 },
	{ CCI_REG16(0x602a), 0x550a },
	{ CCI_REG16(0x6f12), 0x2000 },
	{ CCI_REG16(0x602a), 0x54f6 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x602a), 0x54ec },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x602a), 0x72b0 },
	{ CCI_REG16(0x6f12), 0x0100 },
	{ CCI_REG16(0x602a), 0x64f2 },
	{ CCI_REG16(0x6f12), 0x0100 },
	{ CCI_REG16(0x602a), 0x6508 },
	{ CCI_REG16(0x6f12), 0x0100 },
	{ CCI_REG16(0x602a), 0x5694 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x602a), 0x54f4 },
	{ CCI_REG16(0x6f12), 0x0100 },
	{ CCI_REG16(0x602a), 0x00fc },
	{ CCI_REG16(0x6f12), 0x4000 },
	{ CCI_REG16(0x6f12), 0x4000 },
	{ CCI_REG16(0x6f12), 0xc007 },
	{ CCI_REG16(0x6f12), 0xc007 },
	{ CCI_REG16(0x6f12), 0x0004 },
	{ CCI_REG16(0x6f12), 0x0004 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x602a), 0x5c5e },
	{ CCI_REG16(0x6f12), 0x0800 },
	{ CCI_REG16(0x602a), 0x5c78 },
	{ CCI_REG16(0x6f12), 0x0001 },
	{ CCI_REG16(0x6f12), 0x0004 },
	{ CCI_REG16(0x6f12), 0x0008 },
	{ CCI_REG16(0x6f12), 0x0010 },
	{ CCI_REG16(0x6f12), 0xffff },
	{ CCI_REG16(0x602a), 0x5cdc },
	{ CCI_REG16(0x6f12), 0x0100 },
	{ CCI_REG16(0x602a), 0x5dde },
	{ CCI_REG16(0x6f12), 0x0100 },
	{ CCI_REG16(0x602a), 0x5ee0 },
	{ CCI_REG16(0x6f12), 0x0100 },
	{ CCI_REG16(0x602a), 0x5fe2 },
	{ CCI_REG16(0x6f12), 0x0100 },
	{ CCI_REG16(0x602a), 0x60e4 },
	{ CCI_REG16(0x6f12), 0x0100 },
	{ CCI_REG16(0x602a), 0x5cde },
	{ CCI_REG16(0x6f12), 0x1700 },
	{ CCI_REG16(0x602a), 0x5de0 },
	{ CCI_REG16(0x6f12), 0x1700 },
	{ CCI_REG16(0x602a), 0x5ee2 },
	{ CCI_REG16(0x6f12), 0x1700 },
	{ CCI_REG16(0x602a), 0x5fe4 },
	{ CCI_REG16(0x6f12), 0x1700 },
	{ CCI_REG16(0x602a), 0x60e6 },
	{ CCI_REG16(0x6f12), 0x1700 },
	{ CCI_REG16(0x602a), 0x5ce0 },
	{ CCI_REG16(0x6f12), 0x2f00 },
	{ CCI_REG16(0x602a), 0x5de2 },
	{ CCI_REG16(0x6f12), 0x2f00 },
	{ CCI_REG16(0x602a), 0x5ee4 },
	{ CCI_REG16(0x6f12), 0x2f00 },
	{ CCI_REG16(0x602a), 0x5fe6 },
	{ CCI_REG16(0x6f12), 0x2f00 },
	{ CCI_REG16(0x602a), 0x60e8 },
	{ CCI_REG16(0x6f12), 0x2f00 },
	{ CCI_REG16(0x602a), 0x5c50 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x6214), 0xf9f0 },
	{ CCI_REG16(0x6218), 0xf9f0 },
};

static const struct cci_reg_sequence s5kgw3_mode_2432x1824_regs[] = {
	{ CCI_REG16(0x6028), 0x4000 },
	{ CCI_REG16(0x6214), 0xff7d },
	{ CCI_REG16(0x6218), 0x0000 },
	{ CCI_REG16(0x6028), 0x2400 },
	{ CCI_REG16(0x602a), 0x64e6 },
	{ CCI_REG16(0x6f12), 0x0100 },
	{ CCI_REG16(0x6f12), 0x2823 },
	{ CCI_REG16(0x602a), 0x6a36 },
	{ CCI_REG16(0x6f12), 0xca44 },
	{ CCI_REG16(0x6f12), 0x6f00 },
	{ CCI_REG16(0x6f12), 0xa020 },
	{ CCI_REG16(0xf46a), 0xae80 },
	{ CCI_REG16(0xf44a), 0x0007 },
	{ CCI_REG16(0xf44c), 0x1209 },
	{ CCI_REG16(0x602a), 0x68ce },
	{ CCI_REG16(0x6f12), 0x560b },
	{ CCI_REG16(0x602a), 0x6536 },
	{ CCI_REG16(0x6f12), 0x0100 },
	{ CCI_REG16(0x6f12), 0x0400 },
	{ CCI_REG16(0x6f12), 0x0500 },
	{ CCI_REG16(0x602a), 0x6f64 },
	{ CCI_REG16(0x6f12), 0x1f00 },
	{ CCI_REG16(0x602a), 0x68bc },
	{ CCI_REG16(0x6f12), 0x0704 },
	{ CCI_REG16(0x6f12), 0x0b03 },
	{ CCI_REG16(0x6f12), 0x0b00 },
	{ CCI_REG16(0x6f12), 0x111c },
	{ CCI_REG16(0x602a), 0x7270 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x602a), 0x7274 },
	{ CCI_REG16(0x6f12), 0x4001 },
	{ CCI_REG16(0x602a), 0x70f0 },
	{ CCI_REG16(0x6f12), 0x0400 },
	{ CCI_REG16(0x602a), 0x70f8 },
	{ CCI_REG16(0x6f12), 0x0208 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x602a), 0x7060 },
	{ CCI_REG16(0x6f12), 0x0100 },
	{ CCI_REG16(0x602a), 0x6926 },
	{ CCI_REG16(0x6f12), 0x0001 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x602a), 0x65ba },
	{ CCI_REG16(0x6f12), 0x1900 },
	{ CCI_REG16(0x602a), 0x65ea },
	{ CCI_REG16(0x6f12), 0x0300 },
	{ CCI_REG16(0x6f12), 0x0300 },
	{ CCI_REG16(0x6f12), 0x0600 },
	{ CCI_REG16(0x602a), 0x65f2 },
	{ CCI_REG16(0x6f12), 0x3300 },
	{ CCI_REG16(0x6f12), 0x2f00 },
	{ CCI_REG16(0x602a), 0x664a },
	{ CCI_REG16(0x6f12), 0x0b00 },
	{ CCI_REG16(0x602a), 0x664e },
	{ CCI_REG16(0x6f12), 0x5c00 },
	{ CCI_REG16(0x602a), 0x66de },
	{ CCI_REG16(0x6f12), 0x1600 },
	{ CCI_REG16(0x602a), 0x66e2 },
	{ CCI_REG16(0x6f12), 0x1000 },
	{ CCI_REG16(0x602a), 0x68aa },
	{ CCI_REG16(0x6f12), 0x5e01 },
	{ CCI_REG16(0x1134), 0x0000 },
	{ CCI_REG16(0x1136), 0x4844 },
	{ CCI_REG16(0x602a), 0x6a26 },
	{ CCI_REG16(0x6f12), 0x5000 },
	{ CCI_REG16(0x6f12), 0x5100 },
	{ CCI_REG16(0x602a), 0x68fe },
	{ CCI_REG16(0x6f12), 0xc207 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x602a), 0x6908 },
	{ CCI_REG16(0x6f12), 0xb000 },
	{ CCI_REG16(0x6f12), 0xc207 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x602a), 0x6914 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x602a), 0x546a },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x602a), 0x5460 },
	{ CCI_REG16(0x6f12), 0x0600 },
	{ CCI_REG16(0x602a), 0x5464 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x602a), 0x5478 },
	{ CCI_REG16(0x6f12), 0x0700 },
	{ CCI_REG16(0x6f12), 0x0700 },
	{ CCI_REG16(0x602a), 0x5480 },
	{ CCI_REG16(0x6f12), 0x0400 },
	{ CCI_REG16(0x6f12), 0x0400 },
	{ CCI_REG16(0x602a), 0x548c },
	{ CCI_REG16(0x6f12), 0x4001 },
	{ CCI_REG16(0x602a), 0x5490 },
	{ CCI_REG16(0x6f12), 0x4001 },
	{ CCI_REG16(0x602a), 0x8500 },
	{ CCI_REG16(0x6f12), 0x0101 },
	{ CCI_REG16(0x602a), 0x653c },
	{ CCI_REG16(0x6f12), 0x0100 },
	{ CCI_REG16(0x0136), 0x1300 },
	{ CCI_REG16(0x013e), 0x00c8 },
	{ CCI_REG16(0x0304), 0x0003 },
	{ CCI_REG16(0x0306), 0x008c },
	{ CCI_REG16(0x030c), 0x0000 },
	{ CCI_REG16(0x030e), 0x0003 },
	{ CCI_REG16(0x0310), 0x007d },
	{ CCI_REG16(0x0312), 0x0002 },
	{ CCI_REG16(0x602a), 0x68d0 },
	{ CCI_REG16(0x6f12), 0x9600 },
	{ CCI_REG16(0x602a), 0x720e },
	{ CCI_REG16(0x6f12), 0x0103 },
	{ CCI_REG16(0x0344), 0x0d60 },
	{ CCI_REG16(0x0346), 0x0a00 },
	{ CCI_REG16(0x0348), 0x16ff },
	{ CCI_REG16(0x034a), 0x113f },
	{ CCI_REG16(0x034c), 0x0980 },
	{ CCI_REG16(0x034e), 0x0720 },
	{ CCI_REG16(0x0350), 0x0010 },
	{ CCI_REG16(0x0352), 0x0010 },
	{ CCI_REG16(0x0340), 0x0814 },
	{ CCI_REG16(0x0342), 0x3200 },
	{ CCI_REG16(0x0380), 0x0001 },
	{ CCI_REG16(0x0382), 0x0001 },
	{ CCI_REG16(0x0384), 0x0001 },
	{ CCI_REG16(0x0386), 0x0001 },
	{ CCI_REG16(0x0900), 0x0111 },
	{ CCI_REG16(0x602a), 0x63ae },
	{ CCI_REG16(0x6f12), 0x1000 },
	{ CCI_REG16(0x602a), 0x7048 },
	{ CCI_REG16(0x6f12), 0x0200 },
	{ CCI_REG16(0x602a), 0x71a2 },
	{ CCI_REG16(0x6f12), 0x0200 },
	{ CCI_REG16(0x602a), 0x6548 },
	{ CCI_REG16(0x6f12), 0x0100 },
	{ CCI_REG16(0x0118), 0x0001 },
	{ CCI_REG16(0x602a), 0x54c6 },
	{ CCI_REG16(0x6f12), 0x0a00 },
	{ CCI_REG16(0x602a), 0x708c },
	{ CCI_REG16(0x6f12), 0x0100 },
	{ CCI_REG16(0x602a), 0x71d4 },
	{ CCI_REG16(0x6f12), 0x0100 },
	{ CCI_REG16(0x011a), 0x0001 },
	{ CCI_REG16(0x602a), 0x5634 },
	{ CCI_REG16(0x6f12), 0x0041 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x602a), 0x5ac4 },
	{ CCI_REG16(0x6f12), 0x1106 },
	{ CCI_REG16(0x602a), 0x70a2 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x602a), 0x55c0 },
	{ CCI_REG16(0x6f12), 0x0101 },
	{ CCI_REG16(0x602a), 0x5602 },
	{ CCI_REG16(0x6f12), 0x5e01 },
	{ CCI_REG16(0x0bc8), 0x0002 },
	{ CCI_REG16(0x602a), 0x5528 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x602a), 0x5520 },
	{ CCI_REG16(0x6f12), 0x0004 },
	{ CCI_REG16(0x6f12), 0x1004 },
	{ CCI_REG16(0x6f12), 0x0100 },
	{ CCI_REG16(0x602a), 0x6a7e },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x602a), 0x5596 },
	{ CCI_REG16(0x6f12), 0x1301 },
	{ CCI_REG16(0x602a), 0x6f44 },
	{ CCI_REG16(0x6f12), 0x0500 },
	{ CCI_REG16(0x6f12), 0x0500 },
	{ CCI_REG16(0x0d00), 0x0100 },
	{ CCI_REG16(0x602a), 0x61ba },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x602a), 0x5cf2 },
	{ CCI_REG16(0x6f12), 0x5000 },
	{ CCI_REG16(0x6f12), 0x5000 },
	{ CCI_REG16(0x6f12), 0x6400 },
	{ CCI_REG16(0x6f12), 0x6400 },
	{ CCI_REG16(0x6f12), 0x6400 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x602a), 0x5df4 },
	{ CCI_REG16(0x6f12), 0x5000 },
	{ CCI_REG16(0x6f12), 0x5000 },
	{ CCI_REG16(0x6f12), 0x6400 },
	{ CCI_REG16(0x6f12), 0x6400 },
	{ CCI_REG16(0x6f12), 0x6400 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x602a), 0x5ef6 },
	{ CCI_REG16(0x6f12), 0x5000 },
	{ CCI_REG16(0x6f12), 0x5000 },
	{ CCI_REG16(0x6f12), 0x6400 },
	{ CCI_REG16(0x6f12), 0x6400 },
	{ CCI_REG16(0x6f12), 0x6400 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x602a), 0x5ff8 },
	{ CCI_REG16(0x6f12), 0x5000 },
	{ CCI_REG16(0x6f12), 0x5000 },
	{ CCI_REG16(0x6f12), 0x6400 },
	{ CCI_REG16(0x6f12), 0x6400 },
	{ CCI_REG16(0x6f12), 0x6400 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x602a), 0x60fa },
	{ CCI_REG16(0x6f12), 0x5000 },
	{ CCI_REG16(0x6f12), 0x5000 },
	{ CCI_REG16(0x6f12), 0x6400 },
	{ CCI_REG16(0x6f12), 0x6400 },
	{ CCI_REG16(0x6f12), 0x6400 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x0d02), 0x0001 },
	{ CCI_REG16(0x0d04), 0x0002 },
	{ CCI_REG16(0x0114), 0x0300 },
	{ CCI_REG16(0x602a), 0x550a },
	{ CCI_REG16(0x6f12), 0x1000 },
	{ CCI_REG16(0x602a), 0x54f6 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x602a), 0x54ec },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x602a), 0x72b0 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x602a), 0x64f2 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x602a), 0x6508 },
	{ CCI_REG16(0x6f12), 0x0100 },
	{ CCI_REG16(0x602a), 0x5694 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x602a), 0x54f4 },
	{ CCI_REG16(0x6f12), 0x0001 },
	{ CCI_REG16(0x602a), 0x54fc },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x6f12), 0x0004 },
	{ CCI_REG16(0x6f12), 0x0004 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x6214), 0xf9f0 },
	{ CCI_REG16(0x6218), 0xf9f0 },
};

struct s5kgw3_mode {
	u32 width;
	u32 height;
	u32 hts;
	u32 vts;
	u64 pixel_rate;
	unsigned int link_freq_idx;
	struct v4l2_rect crop;
	const struct cci_reg_sequence *regs;
	unsigned int num_regs;
};

static const struct s5kgw3_mode s5kgw3_modes[] = {
	{
		.width = 4640,
		.height = 3472,
		.hts = 7376,
		.vts = 3598,
		.pixel_rate = 668160000ULL,
		.link_freq_idx = 0,
		.crop = { 0, 0, 9280, 6944 },
		.regs = s5kgw3_mode_4640x3472_regs,
		.num_regs = ARRAY_SIZE(s5kgw3_mode_4640x3472_regs),
	},
	{
		.width = 2432,
		.height = 1824,
		.hts = 12800,
		.vts = 2068,
		.pixel_rate = 160000000ULL,
		.link_freq_idx = 1,
		.crop = { 3424, 2560, 2432, 1824 },
		.regs = s5kgw3_mode_2432x1824_regs,
		.num_regs = ARRAY_SIZE(s5kgw3_mode_2432x1824_regs),
	},
};

struct s5kgw3 {
	struct device *dev;
	struct regmap *regmap;
	struct clk *xvclk;
	struct gpio_desc *reset_gpio;
	struct regulator_bulk_data supplies[ARRAY_SIZE(s5kgw3_supply_names)];

	struct v4l2_subdev sd;
	struct media_pad pad;

	const struct s5kgw3_mode *cur_mode;

	struct v4l2_ctrl_handler ctrl_handler;
	struct v4l2_ctrl *link_freq;
	struct v4l2_ctrl *pixel_rate;
	struct v4l2_ctrl *hblank;
	struct v4l2_ctrl *vblank;
	struct v4l2_ctrl *exposure;
	struct v4l2_ctrl *hflip;
	struct v4l2_ctrl *vflip;
};

static inline struct s5kgw3 *to_s5kgw3(struct v4l2_subdev *sd)
{
	return container_of(sd, struct s5kgw3, sd);
}

static u32 s5kgw3_get_format_code(struct s5kgw3 *s5kgw3)
{
	unsigned int idx = 0;

	if (s5kgw3->hflip && s5kgw3->hflip->val)
		idx |= BIT(0);
	if (s5kgw3->vflip && s5kgw3->vflip->val)
		idx |= BIT(1);

	return s5kgw3_mbus_codes[idx];
}

static int s5kgw3_power_on(struct device *dev)
{
	struct v4l2_subdev *sd = dev_get_drvdata(dev);
	struct s5kgw3 *s5kgw3 = to_s5kgw3(sd);
	int ret;

	gpiod_set_value_cansleep(s5kgw3->reset_gpio, 1);

	ret = regulator_bulk_enable(ARRAY_SIZE(s5kgw3_supply_names),
				    s5kgw3->supplies);
	if (ret)
		return ret;

	ret = clk_prepare_enable(s5kgw3->xvclk);
	if (ret) {
		regulator_bulk_disable(ARRAY_SIZE(s5kgw3_supply_names),
				       s5kgw3->supplies);
		return ret;
	}

	usleep_range(2000, 3000);
	gpiod_set_value_cansleep(s5kgw3->reset_gpio, 0);
	usleep_range(6000, 8000);

	return 0;
}

static int s5kgw3_power_off(struct device *dev)
{
	struct v4l2_subdev *sd = dev_get_drvdata(dev);
	struct s5kgw3 *s5kgw3 = to_s5kgw3(sd);

	gpiod_set_value_cansleep(s5kgw3->reset_gpio, 1);
	clk_disable_unprepare(s5kgw3->xvclk);
	regulator_bulk_disable(ARRAY_SIZE(s5kgw3_supply_names),
			       s5kgw3->supplies);

	return 0;
}

static int s5kgw3_identify(struct s5kgw3 *s5kgw3)
{
	u64 val = 0;
	int ret;

	ret = cci_read(s5kgw3->regmap, S5KGW3_CHIP_ID_REG, &val, NULL);
	if (ret) {
		dev_err(s5kgw3->dev, "failed to read chip ID: %d\n", ret);
		return ret;
	}

	if (val != S5KGW3_CHIP_ID) {
		dev_err(s5kgw3->dev, "unexpected chip ID 0x%04llx (expected 0x%04x)\n",
			val, S5KGW3_CHIP_ID);
		return -ENXIO;
	}

	dev_info(s5kgw3->dev, "detected Samsung S5KGW3 (id=0x%04llx)\n", val);
	return 0;
}

static int s5kgw3_set_ctrl(struct v4l2_ctrl *ctrl)
{
	struct s5kgw3 *s5kgw3 = container_of(ctrl->handler, struct s5kgw3,
					     ctrl_handler);
	const struct s5kgw3_mode *mode = s5kgw3->cur_mode;
	int ret = 0;

	if (ctrl->id == V4L2_CID_VBLANK) {
		s64 exp_max = mode->height + ctrl->val - S5KGW3_EXPOSURE_MARGIN;
		__v4l2_ctrl_modify_range(s5kgw3->exposure, S5KGW3_EXPOSURE_MIN,
					 exp_max, S5KGW3_EXPOSURE_STEP,
					 min_t(s64, s5kgw3->exposure->val, exp_max));
	}

	if (!pm_runtime_get_if_in_use(s5kgw3->dev))
		return 0;

	switch (ctrl->id) {
	case V4L2_CID_EXPOSURE:
		cci_write(s5kgw3->regmap, S5KGW3_REG_EXPOSURE, ctrl->val, &ret);
		break;
	case V4L2_CID_ANALOGUE_GAIN:
		cci_write(s5kgw3->regmap, S5KGW3_REG_AGAIN, ctrl->val << 5, &ret);
		break;
	case V4L2_CID_VBLANK:
		cci_write(s5kgw3->regmap, S5KGW3_REG_VTS,
			  mode->height + ctrl->val, &ret);
		break;
	case V4L2_CID_HFLIP:
	case V4L2_CID_VFLIP:
		cci_write(s5kgw3->regmap, S5KGW3_REG_ORIENTATION,
			  (s5kgw3->hflip->val ? BIT(0) : 0) |
			  (s5kgw3->vflip->val ? BIT(1) : 0), &ret);
		break;
	case V4L2_CID_TEST_PATTERN:
		cci_write(s5kgw3->regmap, S5KGW3_REG_TEST_PATTERN, ctrl->val, &ret);
		break;
	default:
		ret = -EINVAL;
		break;
	}

	pm_runtime_put(s5kgw3->dev);
	return ret;
}

static const struct v4l2_ctrl_ops s5kgw3_ctrl_ops = {
	.s_ctrl = s5kgw3_set_ctrl,
};

static int s5kgw3_enable_streams(struct v4l2_subdev *sd,
				 struct v4l2_subdev_state *state, u32 pad,
				 u64 streams_mask)
{
	struct s5kgw3 *s5kgw3 = to_s5kgw3(sd);
	const struct s5kgw3_mode *mode = s5kgw3->cur_mode;
	u16 orient = (s5kgw3->hflip->val ? BIT(0) : 0) |
		     (s5kgw3->vflip->val ? BIT(1) : 0);
	int ret;

	ret = pm_runtime_resume_and_get(s5kgw3->dev);
	if (ret < 0)
		return ret;

	ret = cci_multi_reg_write(s5kgw3->regmap, s5kgw3_init_regs, 5, NULL);
	if (ret)
		goto err_rpm;

	usleep_range(5000, 6000);

	ret = cci_multi_reg_write(s5kgw3->regmap, s5kgw3_init_regs + 5,
				  ARRAY_SIZE(s5kgw3_init_regs) - 5, NULL);
	if (ret)
		goto err_rpm;

	ret = cci_multi_reg_write(s5kgw3->regmap, mode->regs,
				  mode->num_regs, NULL);
	if (ret)
		goto err_rpm;

	ret = __v4l2_ctrl_handler_setup(&s5kgw3->ctrl_handler);
	if (ret)
		goto err_rpm;

	ret = cci_write(s5kgw3->regmap, S5KGW3_REG_MODE_SELECT,
			S5KGW3_MODE_STREAMING | orient, NULL);
	if (ret)
		goto err_rpm;

	return 0;

err_rpm:
	pm_runtime_put(s5kgw3->dev);
	return ret;
}

static int s5kgw3_disable_streams(struct v4l2_subdev *sd,
				  struct v4l2_subdev_state *state, u32 pad,
				  u64 streams_mask)
{
	struct s5kgw3 *s5kgw3 = to_s5kgw3(sd);
	int ret;

	ret = cci_write(s5kgw3->regmap, S5KGW3_REG_MODE_SELECT,
			S5KGW3_MODE_STANDBY, NULL);
	pm_runtime_put(s5kgw3->dev);

	return ret;
}

static int s5kgw3_enum_mbus_code(struct v4l2_subdev *sd,
				 struct v4l2_subdev_state *state,
				 struct v4l2_subdev_mbus_code_enum *code)
{
	struct s5kgw3 *s5kgw3 = to_s5kgw3(sd);

	if (code->index > 0)
		return -EINVAL;

	code->code = s5kgw3_get_format_code(s5kgw3);
	return 0;
}

static int s5kgw3_enum_frame_size(struct v4l2_subdev *sd,
				  struct v4l2_subdev_state *state,
				  struct v4l2_subdev_frame_size_enum *fse)
{
	struct s5kgw3 *s5kgw3 = to_s5kgw3(sd);

	if (fse->index >= ARRAY_SIZE(s5kgw3_modes) ||
	    fse->code != s5kgw3_get_format_code(s5kgw3))
		return -EINVAL;

	fse->min_width = s5kgw3_modes[fse->index].width;
	fse->max_width = s5kgw3_modes[fse->index].width;
	fse->min_height = s5kgw3_modes[fse->index].height;
	fse->max_height = s5kgw3_modes[fse->index].height;
	return 0;
}

static void s5kgw3_update_controls_for_mode(struct s5kgw3 *s5kgw3,
					    const struct s5kgw3_mode *mode)
{
	s64 hblank = mode->hts - mode->width;
	s64 vblank = mode->vts - mode->height;

	__v4l2_ctrl_s_ctrl(s5kgw3->link_freq, mode->link_freq_idx);
	__v4l2_ctrl_s_ctrl_int64(s5kgw3->pixel_rate, mode->pixel_rate);
	__v4l2_ctrl_modify_range(s5kgw3->hblank, hblank, hblank, 1, hblank);
	__v4l2_ctrl_modify_range(s5kgw3->vblank, vblank,
				 S5KGW3_VTS_MAX - mode->height, 1, vblank);
}

static int s5kgw3_set_fmt(struct v4l2_subdev *sd,
			  struct v4l2_subdev_state *state,
			  struct v4l2_subdev_format *fmt)
{
	struct s5kgw3 *s5kgw3 = to_s5kgw3(sd);
	const struct s5kgw3_mode *mode;
	struct v4l2_mbus_framefmt *format;

	mode = v4l2_find_nearest_size(s5kgw3_modes, ARRAY_SIZE(s5kgw3_modes),
				      width, height,
				      fmt->format.width, fmt->format.height);

	fmt->format.width = mode->width;
	fmt->format.height = mode->height;
	fmt->format.code = s5kgw3_get_format_code(s5kgw3);
	fmt->format.field = V4L2_FIELD_NONE;
	fmt->format.colorspace = V4L2_COLORSPACE_RAW;

	format = v4l2_subdev_state_get_format(state, fmt->pad);
	*format = fmt->format;

	if (fmt->which == V4L2_SUBDEV_FORMAT_ACTIVE) {
		s5kgw3->cur_mode = mode;
		s5kgw3_update_controls_for_mode(s5kgw3, mode);
	}

	return 0;
}

static int s5kgw3_get_selection(struct v4l2_subdev *sd,
				struct v4l2_subdev_state *state,
				struct v4l2_subdev_selection *sel)
{
	struct s5kgw3 *s5kgw3 = to_s5kgw3(sd);

	switch (sel->target) {
	case V4L2_SEL_TGT_CROP:
		sel->r = s5kgw3->cur_mode->crop;
		return 0;
	case V4L2_SEL_TGT_CROP_DEFAULT:
	case V4L2_SEL_TGT_CROP_BOUNDS:
	case V4L2_SEL_TGT_NATIVE_SIZE:
		sel->r.top = 0;
		sel->r.left = 0;
		sel->r.width = S5KGW3_NATIVE_WIDTH;
		sel->r.height = S5KGW3_NATIVE_HEIGHT;
		return 0;
	default:
		return -EINVAL;
	}
}

static int s5kgw3_init_state(struct v4l2_subdev *sd,
			     struct v4l2_subdev_state *state)
{
	struct v4l2_mbus_framefmt *format =
		v4l2_subdev_state_get_format(state, 0);

	format->width = s5kgw3_modes[0].width;
	format->height = s5kgw3_modes[0].height;
	format->code = MEDIA_BUS_FMT_SGRBG10_1X10;
	format->field = V4L2_FIELD_NONE;
	format->colorspace = V4L2_COLORSPACE_RAW;
	return 0;
}

static const struct v4l2_subdev_pad_ops s5kgw3_pad_ops = {
	.enum_mbus_code = s5kgw3_enum_mbus_code,
	.enum_frame_size = s5kgw3_enum_frame_size,
	.get_fmt = v4l2_subdev_get_fmt,
	.set_fmt = s5kgw3_set_fmt,
	.get_selection = s5kgw3_get_selection,
	.enable_streams = s5kgw3_enable_streams,
	.disable_streams = s5kgw3_disable_streams,
};

static const struct v4l2_subdev_video_ops s5kgw3_video_ops = {
	.s_stream = v4l2_subdev_s_stream_helper,
};

static const struct v4l2_subdev_ops s5kgw3_subdev_ops = {
	.video = &s5kgw3_video_ops,
	.pad = &s5kgw3_pad_ops,
};

static const struct v4l2_subdev_internal_ops s5kgw3_internal_ops = {
	.init_state = s5kgw3_init_state,
};

static int s5kgw3_init_controls(struct s5kgw3 *s5kgw3)
{
	struct v4l2_ctrl_handler *hdl = &s5kgw3->ctrl_handler;
	struct v4l2_fwnode_device_properties props;
	const struct s5kgw3_mode *mode = s5kgw3->cur_mode;
	int ret;

	v4l2_ctrl_handler_init(hdl, 10);

	s5kgw3->link_freq = v4l2_ctrl_new_int_menu(hdl, &s5kgw3_ctrl_ops,
						   V4L2_CID_LINK_FREQ,
						   ARRAY_SIZE(s5kgw3_link_freq_menu) - 1,
						   mode->link_freq_idx,
						   s5kgw3_link_freq_menu);
	if (s5kgw3->link_freq)
		s5kgw3->link_freq->flags |= V4L2_CTRL_FLAG_READ_ONLY;

	s5kgw3->pixel_rate = v4l2_ctrl_new_std(hdl, &s5kgw3_ctrl_ops,
					       V4L2_CID_PIXEL_RATE,
					       160000000ULL, 668160000ULL, 1,
					       mode->pixel_rate);

	s5kgw3->hblank = v4l2_ctrl_new_std(hdl, &s5kgw3_ctrl_ops,
					   V4L2_CID_HBLANK,
					   mode->hts - mode->width,
					   mode->hts - mode->width, 1,
					   mode->hts - mode->width);
	if (s5kgw3->hblank)
		s5kgw3->hblank->flags |= V4L2_CTRL_FLAG_READ_ONLY;

	s5kgw3->vblank = v4l2_ctrl_new_std(hdl, &s5kgw3_ctrl_ops,
					   V4L2_CID_VBLANK,
					   mode->vts - mode->height,
					   S5KGW3_VTS_MAX - mode->height,
					   1, mode->vts - mode->height);

	s5kgw3->exposure = v4l2_ctrl_new_std(hdl, &s5kgw3_ctrl_ops,
					     V4L2_CID_EXPOSURE,
					     S5KGW3_EXPOSURE_MIN,
					     mode->vts - S5KGW3_EXPOSURE_MARGIN,
					     S5KGW3_EXPOSURE_STEP,
					     mode->vts / 2);

	v4l2_ctrl_new_std(hdl, &s5kgw3_ctrl_ops, V4L2_CID_ANALOGUE_GAIN,
			  S5KGW3_AGAIN_MIN, S5KGW3_AGAIN_MAX, 1,
			  S5KGW3_AGAIN_DEFAULT);

	s5kgw3->hflip = v4l2_ctrl_new_std(hdl, &s5kgw3_ctrl_ops, V4L2_CID_HFLIP,
					  0, 1, 1, 0);
	if (s5kgw3->hflip)
		s5kgw3->hflip->flags |= V4L2_CTRL_FLAG_MODIFY_LAYOUT;

	s5kgw3->vflip = v4l2_ctrl_new_std(hdl, &s5kgw3_ctrl_ops, V4L2_CID_VFLIP,
					  0, 1, 1, 0);
	if (s5kgw3->vflip)
		s5kgw3->vflip->flags |= V4L2_CTRL_FLAG_MODIFY_LAYOUT;

	ret = v4l2_fwnode_device_parse(s5kgw3->dev, &props);
	if (!ret)
		v4l2_ctrl_new_fwnode_properties(hdl, &s5kgw3_ctrl_ops, &props);

	if (hdl->error) {
		ret = hdl->error;
		v4l2_ctrl_handler_free(hdl);
		return ret;
	}

	s5kgw3->sd.ctrl_handler = hdl;
	return 0;
}

static int s5kgw3_probe(struct i2c_client *client)
{
	struct s5kgw3 *s5kgw3;
	int i, ret;

	s5kgw3 = devm_kzalloc(&client->dev, sizeof(*s5kgw3), GFP_KERNEL);
	if (!s5kgw3)
		return -ENOMEM;

	s5kgw3->dev = &client->dev;
	s5kgw3->cur_mode = &s5kgw3_modes[0];
	s5kgw3->regmap = devm_cci_regmap_init_i2c(client, 16);
	if (IS_ERR(s5kgw3->regmap))
		return dev_err_probe(s5kgw3->dev, PTR_ERR(s5kgw3->regmap),
				     "failed to init CCI regmap\n");

	s5kgw3->xvclk = devm_clk_get(s5kgw3->dev, NULL);
	if (IS_ERR(s5kgw3->xvclk))
		return dev_err_probe(s5kgw3->dev, PTR_ERR(s5kgw3->xvclk),
				     "failed to get xvclk\n");

	s5kgw3->reset_gpio = devm_gpiod_get(s5kgw3->dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(s5kgw3->reset_gpio))
		return dev_err_probe(s5kgw3->dev, PTR_ERR(s5kgw3->reset_gpio),
				     "failed to get reset gpio\n");

	for (i = 0; i < ARRAY_SIZE(s5kgw3_supply_names); i++)
		s5kgw3->supplies[i].supply = s5kgw3_supply_names[i];

	ret = devm_regulator_bulk_get(s5kgw3->dev, ARRAY_SIZE(s5kgw3_supply_names),
				      s5kgw3->supplies);
	if (ret)
		return dev_err_probe(s5kgw3->dev, ret, "failed to get supplies\n");

	v4l2_i2c_subdev_init(&s5kgw3->sd, client, &s5kgw3_subdev_ops);
	s5kgw3->sd.internal_ops = &s5kgw3_internal_ops;

	ret = s5kgw3_power_on(s5kgw3->dev);
	if (ret)
		return ret;

	ret = s5kgw3_identify(s5kgw3);
	if (ret)
		goto err_power_off;

	ret = s5kgw3_init_controls(s5kgw3);
	if (ret)
		goto err_power_off;

	s5kgw3->sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	s5kgw3->sd.entity.function = MEDIA_ENT_F_CAM_SENSOR;
	s5kgw3->pad.flags = MEDIA_PAD_FL_SOURCE;

	ret = media_entity_pads_init(&s5kgw3->sd.entity, 1, &s5kgw3->pad);
	if (ret)
		goto err_ctrl_free;

	s5kgw3->sd.state_lock = s5kgw3->ctrl_handler.lock;
	ret = v4l2_subdev_init_finalize(&s5kgw3->sd);
	if (ret)
		goto err_entity_cleanup;

	pm_runtime_set_active(s5kgw3->dev);
	pm_runtime_enable(s5kgw3->dev);

	ret = v4l2_async_register_subdev_sensor(&s5kgw3->sd);
	if (ret)
		goto err_rpm_disable;

	pm_runtime_idle(s5kgw3->dev);
	return 0;

err_rpm_disable:
	pm_runtime_disable(s5kgw3->dev);
	pm_runtime_set_suspended(s5kgw3->dev);
	v4l2_subdev_cleanup(&s5kgw3->sd);
err_entity_cleanup:
	media_entity_cleanup(&s5kgw3->sd.entity);
err_ctrl_free:
	v4l2_ctrl_handler_free(&s5kgw3->ctrl_handler);
err_power_off:
	s5kgw3_power_off(s5kgw3->dev);
	return ret;
}

static void s5kgw3_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct s5kgw3 *s5kgw3 = to_s5kgw3(sd);

	v4l2_async_unregister_subdev(sd);
	v4l2_subdev_cleanup(sd);
	media_entity_cleanup(&sd->entity);
	v4l2_ctrl_handler_free(&s5kgw3->ctrl_handler);

	pm_runtime_disable(s5kgw3->dev);
	if (!pm_runtime_status_suspended(s5kgw3->dev))
		s5kgw3_power_off(s5kgw3->dev);
	pm_runtime_set_suspended(s5kgw3->dev);
}

static const struct dev_pm_ops s5kgw3_pm_ops = {
	SET_RUNTIME_PM_OPS(s5kgw3_power_off, s5kgw3_power_on, NULL)
};

static const struct of_device_id s5kgw3_of_match[] = {
	{ .compatible = "samsung,s5kgw3" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, s5kgw3_of_match);

static struct i2c_driver s5kgw3_i2c_driver = {
	.driver = {
		.name = "s5kgw3",
		.of_match_table = s5kgw3_of_match,
		.pm = &s5kgw3_pm_ops,
	},
	.probe = s5kgw3_probe,
	.remove = s5kgw3_remove,
};
module_i2c_driver(s5kgw3_i2c_driver);

MODULE_DESCRIPTION("Samsung S5KGW3 64MP CMOS image sensor driver");
MODULE_LICENSE("GPL");

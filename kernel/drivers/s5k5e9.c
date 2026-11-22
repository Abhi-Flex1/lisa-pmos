// SPDX-License-Identifier: GPL-2.0
/*
 * V4L2 subdev driver for Samsung S5K5E9 5MP CMOS image sensor
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

#define S5K5E9_CHIP_ID_REG		CCI_REG16(0x0000)
#define S5K5E9_CHIP_ID			0x559b

#define S5K5E9_REG_MODE_SELECT		CCI_REG8(0x0100)
#define S5K5E9_MODE_STANDBY		0x00
#define S5K5E9_MODE_STREAMING		0x01

#define S5K5E9_REG_ORIENTATION		CCI_REG8(0x0101)
#define S5K5E9_REG_GROUP_HOLD		CCI_REG8(0x0104)

#define S5K5E9_REG_EXPOSURE		CCI_REG16(0x0202)
#define S5K5E9_EXPOSURE_MIN		4
#define S5K5E9_EXPOSURE_STEP		1
#define S5K5E9_EXPOSURE_MARGIN		4

#define S5K5E9_REG_AGAIN		CCI_REG16(0x0204)
#define S5K5E9_AGAIN_MIN		1
#define S5K5E9_AGAIN_MAX		16
#define S5K5E9_AGAIN_DEFAULT		1

#define S5K5E9_REG_VTS			CCI_REG16(0x0340)
#define S5K5E9_VTS_MAX			0xffff

#define S5K5E9_REG_TEST_PATTERN		CCI_REG16(0x0600)

#define S5K5E9_NATIVE_WIDTH		2592
#define S5K5E9_NATIVE_HEIGHT		1944
#define S5K5E9_HTS			3112
#define S5K5E9_VTS			2030
#define S5K5E9_PIXEL_RATE		175200000ULL
#define S5K5E9_LINK_FREQ		438400000LL
#define S5K5E9_MCLK_FREQ		19200000UL

static const s64 s5k5e9_link_freq_menu[] = {
	S5K5E9_LINK_FREQ,
};

static const char * const s5k5e9_supply_names[] = {
	"vio",
	"vana",
	"vdig",
};

static const u32 s5k5e9_mbus_codes[] = {
	MEDIA_BUS_FMT_SGRBG10_1X10,
	MEDIA_BUS_FMT_SRGGB10_1X10,
	MEDIA_BUS_FMT_SBGGR10_1X10,
	MEDIA_BUS_FMT_SGBRG10_1X10,
};

static const struct cci_reg_sequence s5k5e9_init_regs[] = {
	{ CCI_REG8(0x0100), 0x00 },
	{ CCI_REG8(0x3b45), 0x01 },
	{ CCI_REG8(0x0b05), 0x01 },
	{ CCI_REG8(0x392f), 0x01 },
	{ CCI_REG8(0x3930), 0x00 },
	{ CCI_REG8(0x3924), 0x7f },
	{ CCI_REG8(0x3925), 0xfd },
	{ CCI_REG8(0x3c08), 0xff },
	{ CCI_REG8(0x3c09), 0xff },
	{ CCI_REG8(0x3c31), 0xff },
	{ CCI_REG8(0x3c32), 0xff },
	{ CCI_REG8(0x3290), 0x10 },
	{ CCI_REG8(0x3200), 0x01 },
	{ CCI_REG8(0x3074), 0x06 },
	{ CCI_REG8(0x3075), 0x2f },
	{ CCI_REG8(0x308a), 0x20 },
	{ CCI_REG8(0x308b), 0x08 },
	{ CCI_REG8(0x308c), 0x0b },
	{ CCI_REG8(0x3081), 0x07 },
	{ CCI_REG8(0x307b), 0x85 },
	{ CCI_REG8(0x307a), 0x0a },
	{ CCI_REG8(0x3079), 0x0a },
	{ CCI_REG8(0x306e), 0x71 },
	{ CCI_REG8(0x306f), 0x28 },
	{ CCI_REG8(0x301f), 0x20 },
	{ CCI_REG8(0x3012), 0x4e },
	{ CCI_REG8(0x306b), 0x9a },
	{ CCI_REG8(0x3091), 0x16 },
	{ CCI_REG8(0x30c4), 0x06 },
	{ CCI_REG8(0x306a), 0x79 },
	{ CCI_REG8(0x30b0), 0xff },
	{ CCI_REG8(0x306d), 0x08 },
	{ CCI_REG8(0x3084), 0x16 },
	{ CCI_REG8(0x3070), 0x0f },
	{ CCI_REG8(0x30c2), 0x05 },
	{ CCI_REG8(0x3069), 0x87 },
	{ CCI_REG8(0x3c0f), 0x00 },
	{ CCI_REG8(0x3083), 0x14 },
	{ CCI_REG8(0x3080), 0x08 },
	{ CCI_REG8(0x3c34), 0xea },
	{ CCI_REG8(0x3c35), 0x5c },
	{ CCI_REG8(0x3931), 0x02 },
};

static const struct cci_reg_sequence s5k5e9_mode_2592x1944_regs[] = {
	{ CCI_REG8(0x0100), 0x00 },
	{ CCI_REG8(0x0136), 0x13 },
	{ CCI_REG8(0x0137), 0x33 },
	{ CCI_REG8(0x0305), 0x03 },
	{ CCI_REG8(0x0306), 0x00 },
	{ CCI_REG8(0x0307), 0x59 },
	{ CCI_REG8(0x030d), 0x03 },
	{ CCI_REG8(0x030e), 0x00 },
	{ CCI_REG8(0x030f), 0x89 },
	{ CCI_REG8(0x3c1f), 0x00 },
	{ CCI_REG8(0x3c17), 0x00 },
	{ CCI_REG8(0x0112), 0x0a },
	{ CCI_REG8(0x0113), 0x0a },
	{ CCI_REG8(0x0114), 0x01 },
	{ CCI_REG8(0x0820), 0x03 },
	{ CCI_REG8(0x0821), 0x6c },
	{ CCI_REG8(0x0822), 0x00 },
	{ CCI_REG8(0x0823), 0x00 },
	{ CCI_REG8(0x3929), 0x0f },
	{ CCI_REG8(0x0344), 0x00 },
	{ CCI_REG8(0x0345), 0x08 },
	{ CCI_REG8(0x0346), 0x00 },
	{ CCI_REG8(0x0347), 0x08 },
	{ CCI_REG8(0x0348), 0x0a },
	{ CCI_REG8(0x0349), 0x27 },
	{ CCI_REG8(0x034a), 0x07 },
	{ CCI_REG8(0x034b), 0x9f },
	{ CCI_REG8(0x034c), 0x0a },
	{ CCI_REG8(0x034d), 0x20 },
	{ CCI_REG8(0x034e), 0x07 },
	{ CCI_REG8(0x034f), 0x98 },
	{ CCI_REG8(0x0900), 0x00 },
	{ CCI_REG8(0x0901), 0x00 },
	{ CCI_REG8(0x0381), 0x01 },
	{ CCI_REG8(0x0383), 0x01 },
	{ CCI_REG8(0x0385), 0x01 },
	{ CCI_REG8(0x0387), 0x01 },
	{ CCI_REG8(0x0101), 0x00 },
	{ CCI_REG8(0x0340), 0x07 },
	{ CCI_REG8(0x0341), 0xee },
	{ CCI_REG8(0x0342), 0x0c },
	{ CCI_REG8(0x0343), 0x28 },
	{ CCI_REG8(0x0200), 0x0b },
	{ CCI_REG8(0x0201), 0x9c },
	{ CCI_REG8(0x0202), 0x00 },
	{ CCI_REG8(0x0203), 0x02 },
	{ CCI_REG8(0x30b8), 0x2e },
	{ CCI_REG8(0x30ba), 0x36 },
};

struct s5k5e9 {
	struct device *dev;
	struct regmap *regmap;
	struct clk *xvclk;
	struct gpio_desc *reset_gpio;
	struct regulator_bulk_data supplies[ARRAY_SIZE(s5k5e9_supply_names)];

	struct v4l2_subdev sd;
	struct media_pad pad;

	struct v4l2_ctrl_handler ctrl_handler;
	struct v4l2_ctrl *exposure;
	struct v4l2_ctrl *vblank;
	struct v4l2_ctrl *hflip;
	struct v4l2_ctrl *vflip;
};

static inline struct s5k5e9 *to_s5k5e9(struct v4l2_subdev *sd)
{
	return container_of(sd, struct s5k5e9, sd);
}

static u32 s5k5e9_get_format_code(struct s5k5e9 *s5k5e9)
{
	unsigned int idx = 0;

	if (s5k5e9->hflip && s5k5e9->hflip->val)
		idx |= BIT(0);
	if (s5k5e9->vflip && s5k5e9->vflip->val)
		idx |= BIT(1);

	return s5k5e9_mbus_codes[idx];
}

static int s5k5e9_power_on(struct device *dev)
{
	struct v4l2_subdev *sd = dev_get_drvdata(dev);
	struct s5k5e9 *s5k5e9 = to_s5k5e9(sd);
	int ret;

	gpiod_set_value_cansleep(s5k5e9->reset_gpio, 1);

	ret = regulator_bulk_enable(ARRAY_SIZE(s5k5e9_supply_names),
				    s5k5e9->supplies);
	if (ret)
		return ret;

	ret = clk_prepare_enable(s5k5e9->xvclk);
	if (ret) {
		regulator_bulk_disable(ARRAY_SIZE(s5k5e9_supply_names),
				       s5k5e9->supplies);
		return ret;
	}

	usleep_range(2000, 3000);
	gpiod_set_value_cansleep(s5k5e9->reset_gpio, 0);
	usleep_range(5000, 6000);

	return 0;
}

static int s5k5e9_power_off(struct device *dev)
{
	struct v4l2_subdev *sd = dev_get_drvdata(dev);
	struct s5k5e9 *s5k5e9 = to_s5k5e9(sd);

	gpiod_set_value_cansleep(s5k5e9->reset_gpio, 1);
	clk_disable_unprepare(s5k5e9->xvclk);
	regulator_bulk_disable(ARRAY_SIZE(s5k5e9_supply_names),
			       s5k5e9->supplies);

	return 0;
}

static int s5k5e9_identify(struct s5k5e9 *s5k5e9)
{
	u64 val = 0;
	int ret;

	ret = cci_read(s5k5e9->regmap, S5K5E9_CHIP_ID_REG, &val, NULL);
	if (ret) {
		dev_err(s5k5e9->dev, "failed to read chip ID: %d\n", ret);
		return ret;
	}

	if (val != S5K5E9_CHIP_ID) {
		dev_err(s5k5e9->dev, "unexpected chip ID 0x%04llx (expected 0x%04x)\n",
			val, S5K5E9_CHIP_ID);
		return -ENXIO;
	}

	dev_info(s5k5e9->dev, "detected Samsung S5K5E9 (id=0x%04llx)\n", val);
	return 0;
}

static int s5k5e9_set_ctrl(struct v4l2_ctrl *ctrl)
{
	struct s5k5e9 *s5k5e9 = container_of(ctrl->handler, struct s5k5e9,
					     ctrl_handler);
	int ret = 0;

	if (ctrl->id == V4L2_CID_VBLANK) {
		s64 exp_max = S5K5E9_NATIVE_HEIGHT + ctrl->val - S5K5E9_EXPOSURE_MARGIN;
		__v4l2_ctrl_modify_range(s5k5e9->exposure, S5K5E9_EXPOSURE_MIN,
					 exp_max, S5K5E9_EXPOSURE_STEP,
					 min_t(s64, s5k5e9->exposure->val, exp_max));
	}

	if (!pm_runtime_get_if_in_use(s5k5e9->dev))
		return 0;

	switch (ctrl->id) {
	case V4L2_CID_EXPOSURE:
		cci_write(s5k5e9->regmap, S5K5E9_REG_EXPOSURE, ctrl->val, &ret);
		break;
	case V4L2_CID_ANALOGUE_GAIN:
		cci_write(s5k5e9->regmap, S5K5E9_REG_AGAIN, ctrl->val << 5, &ret);
		break;
	case V4L2_CID_VBLANK:
		cci_write(s5k5e9->regmap, S5K5E9_REG_VTS,
			  S5K5E9_NATIVE_HEIGHT + ctrl->val, &ret);
		break;
	case V4L2_CID_HFLIP:
	case V4L2_CID_VFLIP:
		cci_write(s5k5e9->regmap, S5K5E9_REG_ORIENTATION,
			  (s5k5e9->hflip->val ? BIT(0) : 0) |
			  (s5k5e9->vflip->val ? BIT(1) : 0), &ret);
		break;
	case V4L2_CID_TEST_PATTERN:
		cci_write(s5k5e9->regmap, S5K5E9_REG_TEST_PATTERN, ctrl->val, &ret);
		break;
	default:
		ret = -EINVAL;
		break;
	}

	pm_runtime_put(s5k5e9->dev);
	return ret;
}

static const struct v4l2_ctrl_ops s5k5e9_ctrl_ops = {
	.s_ctrl = s5k5e9_set_ctrl,
};

static int s5k5e9_enable_streams(struct v4l2_subdev *sd,
				 struct v4l2_subdev_state *state, u32 pad,
				 u64 streams_mask)
{
	struct s5k5e9 *s5k5e9 = to_s5k5e9(sd);
	int ret;

	ret = pm_runtime_resume_and_get(s5k5e9->dev);
	if (ret < 0)
		return ret;

	ret = cci_multi_reg_write(s5k5e9->regmap, s5k5e9_init_regs,
				  ARRAY_SIZE(s5k5e9_init_regs), NULL);
	if (ret)
		goto err_rpm;

	ret = cci_multi_reg_write(s5k5e9->regmap, s5k5e9_mode_2592x1944_regs,
				  ARRAY_SIZE(s5k5e9_mode_2592x1944_regs), NULL);
	if (ret)
		goto err_rpm;

	ret = __v4l2_ctrl_handler_setup(&s5k5e9->ctrl_handler);
	if (ret)
		goto err_rpm;

	ret = cci_write(s5k5e9->regmap, S5K5E9_REG_MODE_SELECT,
			S5K5E9_MODE_STREAMING, NULL);
	if (ret)
		goto err_rpm;

	return 0;

err_rpm:
	pm_runtime_put(s5k5e9->dev);
	return ret;
}

static int s5k5e9_disable_streams(struct v4l2_subdev *sd,
				  struct v4l2_subdev_state *state, u32 pad,
				  u64 streams_mask)
{
	struct s5k5e9 *s5k5e9 = to_s5k5e9(sd);
	int ret;

	ret = cci_write(s5k5e9->regmap, S5K5E9_REG_MODE_SELECT,
			S5K5E9_MODE_STANDBY, NULL);
	pm_runtime_put(s5k5e9->dev);

	return ret;
}

static int s5k5e9_enum_mbus_code(struct v4l2_subdev *sd,
				 struct v4l2_subdev_state *state,
				 struct v4l2_subdev_mbus_code_enum *code)
{
	struct s5k5e9 *s5k5e9 = to_s5k5e9(sd);

	if (code->index > 0)
		return -EINVAL;

	code->code = s5k5e9_get_format_code(s5k5e9);
	return 0;
}

static int s5k5e9_enum_frame_size(struct v4l2_subdev *sd,
				  struct v4l2_subdev_state *state,
				  struct v4l2_subdev_frame_size_enum *fse)
{
	struct s5k5e9 *s5k5e9 = to_s5k5e9(sd);

	if (fse->index > 0 || fse->code != s5k5e9_get_format_code(s5k5e9))
		return -EINVAL;

	fse->min_width = S5K5E9_NATIVE_WIDTH;
	fse->max_width = S5K5E9_NATIVE_WIDTH;
	fse->min_height = S5K5E9_NATIVE_HEIGHT;
	fse->max_height = S5K5E9_NATIVE_HEIGHT;
	return 0;
}

static int s5k5e9_set_fmt(struct v4l2_subdev *sd,
			  struct v4l2_subdev_state *state,
			  struct v4l2_subdev_format *fmt)
{
	struct s5k5e9 *s5k5e9 = to_s5k5e9(sd);
	struct v4l2_mbus_framefmt *format;

	fmt->format.width = S5K5E9_NATIVE_WIDTH;
	fmt->format.height = S5K5E9_NATIVE_HEIGHT;
	fmt->format.code = s5k5e9_get_format_code(s5k5e9);
	fmt->format.field = V4L2_FIELD_NONE;
	fmt->format.colorspace = V4L2_COLORSPACE_RAW;

	format = v4l2_subdev_state_get_format(state, fmt->pad);
	*format = fmt->format;

	return 0;
}

static int s5k5e9_get_selection(struct v4l2_subdev *sd,
				struct v4l2_subdev_state *state,
				struct v4l2_subdev_selection *sel)
{
	switch (sel->target) {
	case V4L2_SEL_TGT_CROP:
	case V4L2_SEL_TGT_CROP_DEFAULT:
	case V4L2_SEL_TGT_CROP_BOUNDS:
	case V4L2_SEL_TGT_NATIVE_SIZE:
		sel->r.top = 0;
		sel->r.left = 0;
		sel->r.width = S5K5E9_NATIVE_WIDTH;
		sel->r.height = S5K5E9_NATIVE_HEIGHT;
		return 0;
	default:
		return -EINVAL;
	}
}

static int s5k5e9_init_state(struct v4l2_subdev *sd,
			     struct v4l2_subdev_state *state)
{
	struct v4l2_mbus_framefmt *format =
		v4l2_subdev_state_get_format(state, 0);

	format->width = S5K5E9_NATIVE_WIDTH;
	format->height = S5K5E9_NATIVE_HEIGHT;
	format->code = MEDIA_BUS_FMT_SGRBG10_1X10;
	format->field = V4L2_FIELD_NONE;
	format->colorspace = V4L2_COLORSPACE_RAW;
	return 0;
}

static const struct v4l2_subdev_pad_ops s5k5e9_pad_ops = {
	.enum_mbus_code = s5k5e9_enum_mbus_code,
	.enum_frame_size = s5k5e9_enum_frame_size,
	.get_fmt = v4l2_subdev_get_fmt,
	.set_fmt = s5k5e9_set_fmt,
	.get_selection = s5k5e9_get_selection,
	.enable_streams = s5k5e9_enable_streams,
	.disable_streams = s5k5e9_disable_streams,
};

static const struct v4l2_subdev_video_ops s5k5e9_video_ops = {
	.s_stream = v4l2_subdev_s_stream_helper,
};

static const struct v4l2_subdev_ops s5k5e9_subdev_ops = {
	.video = &s5k5e9_video_ops,
	.pad = &s5k5e9_pad_ops,
};

static const struct v4l2_subdev_internal_ops s5k5e9_internal_ops = {
	.init_state = s5k5e9_init_state,
};

static int s5k5e9_init_controls(struct s5k5e9 *s5k5e9)
{
	struct v4l2_ctrl_handler *hdl = &s5k5e9->ctrl_handler;
	struct v4l2_fwnode_device_properties props;
	struct v4l2_ctrl *ctrl;
	int ret;

	v4l2_ctrl_handler_init(hdl, 10);

	ctrl = v4l2_ctrl_new_int_menu(hdl, &s5k5e9_ctrl_ops, V4L2_CID_LINK_FREQ,
				      0, 0, s5k5e9_link_freq_menu);
	if (ctrl)
		ctrl->flags |= V4L2_CTRL_FLAG_READ_ONLY;

	v4l2_ctrl_new_std(hdl, &s5k5e9_ctrl_ops, V4L2_CID_PIXEL_RATE,
			  S5K5E9_PIXEL_RATE, S5K5E9_PIXEL_RATE, 1,
			  S5K5E9_PIXEL_RATE);

	ctrl = v4l2_ctrl_new_std(hdl, &s5k5e9_ctrl_ops, V4L2_CID_HBLANK,
				 S5K5E9_HTS - S5K5E9_NATIVE_WIDTH,
				 S5K5E9_HTS - S5K5E9_NATIVE_WIDTH, 1,
				 S5K5E9_HTS - S5K5E9_NATIVE_WIDTH);
	if (ctrl)
		ctrl->flags |= V4L2_CTRL_FLAG_READ_ONLY;

	s5k5e9->vblank = v4l2_ctrl_new_std(hdl, &s5k5e9_ctrl_ops, V4L2_CID_VBLANK,
					   S5K5E9_VTS - S5K5E9_NATIVE_HEIGHT,
					   S5K5E9_VTS_MAX - S5K5E9_NATIVE_HEIGHT,
					   1, S5K5E9_VTS - S5K5E9_NATIVE_HEIGHT);

	s5k5e9->exposure = v4l2_ctrl_new_std(hdl, &s5k5e9_ctrl_ops, V4L2_CID_EXPOSURE,
					     S5K5E9_EXPOSURE_MIN,
					     S5K5E9_VTS - S5K5E9_EXPOSURE_MARGIN,
					     S5K5E9_EXPOSURE_STEP,
					     S5K5E9_VTS / 2);

	v4l2_ctrl_new_std(hdl, &s5k5e9_ctrl_ops, V4L2_CID_ANALOGUE_GAIN,
			  S5K5E9_AGAIN_MIN, S5K5E9_AGAIN_MAX, 1,
			  S5K5E9_AGAIN_DEFAULT);

	s5k5e9->hflip = v4l2_ctrl_new_std(hdl, &s5k5e9_ctrl_ops, V4L2_CID_HFLIP,
					  0, 1, 1, 0);
	if (s5k5e9->hflip)
		s5k5e9->hflip->flags |= V4L2_CTRL_FLAG_MODIFY_LAYOUT;

	s5k5e9->vflip = v4l2_ctrl_new_std(hdl, &s5k5e9_ctrl_ops, V4L2_CID_VFLIP,
					  0, 1, 1, 0);
	if (s5k5e9->vflip)
		s5k5e9->vflip->flags |= V4L2_CTRL_FLAG_MODIFY_LAYOUT;

	ret = v4l2_fwnode_device_parse(s5k5e9->dev, &props);
	if (!ret)
		v4l2_ctrl_new_fwnode_properties(hdl, &s5k5e9_ctrl_ops, &props);

	if (hdl->error) {
		ret = hdl->error;
		v4l2_ctrl_handler_free(hdl);
		return ret;
	}

	s5k5e9->sd.ctrl_handler = hdl;
	return 0;
}

static int s5k5e9_probe(struct i2c_client *client)
{
	struct s5k5e9 *s5k5e9;
	int i, ret;

	s5k5e9 = devm_kzalloc(&client->dev, sizeof(*s5k5e9), GFP_KERNEL);
	if (!s5k5e9)
		return -ENOMEM;

	s5k5e9->dev = &client->dev;
	s5k5e9->regmap = devm_cci_regmap_init_i2c(client, 16);
	if (IS_ERR(s5k5e9->regmap))
		return dev_err_probe(s5k5e9->dev, PTR_ERR(s5k5e9->regmap),
				     "failed to init CCI regmap\n");

	s5k5e9->xvclk = devm_clk_get(s5k5e9->dev, NULL);
	if (IS_ERR(s5k5e9->xvclk))
		return dev_err_probe(s5k5e9->dev, PTR_ERR(s5k5e9->xvclk),
				     "failed to get xvclk\n");

	s5k5e9->reset_gpio = devm_gpiod_get(s5k5e9->dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(s5k5e9->reset_gpio))
		return dev_err_probe(s5k5e9->dev, PTR_ERR(s5k5e9->reset_gpio),
				     "failed to get reset gpio\n");

	for (i = 0; i < ARRAY_SIZE(s5k5e9_supply_names); i++)
		s5k5e9->supplies[i].supply = s5k5e9_supply_names[i];

	ret = devm_regulator_bulk_get(s5k5e9->dev, ARRAY_SIZE(s5k5e9_supply_names),
				      s5k5e9->supplies);
	if (ret)
		return dev_err_probe(s5k5e9->dev, ret, "failed to get supplies\n");

	v4l2_i2c_subdev_init(&s5k5e9->sd, client, &s5k5e9_subdev_ops);
	s5k5e9->sd.internal_ops = &s5k5e9_internal_ops;

	ret = s5k5e9_power_on(s5k5e9->dev);
	if (ret)
		return ret;

	ret = s5k5e9_identify(s5k5e9);
	if (ret)
		goto err_power_off;

	ret = s5k5e9_init_controls(s5k5e9);
	if (ret)
		goto err_power_off;

	s5k5e9->sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	s5k5e9->sd.entity.function = MEDIA_ENT_F_CAM_SENSOR;
	s5k5e9->pad.flags = MEDIA_PAD_FL_SOURCE;

	ret = media_entity_pads_init(&s5k5e9->sd.entity, 1, &s5k5e9->pad);
	if (ret)
		goto err_ctrl_free;

	s5k5e9->sd.state_lock = s5k5e9->ctrl_handler.lock;
	ret = v4l2_subdev_init_finalize(&s5k5e9->sd);
	if (ret)
		goto err_entity_cleanup;

	pm_runtime_set_active(s5k5e9->dev);
	pm_runtime_enable(s5k5e9->dev);

	ret = v4l2_async_register_subdev_sensor(&s5k5e9->sd);
	if (ret)
		goto err_rpm_disable;

	pm_runtime_idle(s5k5e9->dev);
	return 0;

err_rpm_disable:
	pm_runtime_disable(s5k5e9->dev);
	pm_runtime_set_suspended(s5k5e9->dev);
	v4l2_subdev_cleanup(&s5k5e9->sd);
err_entity_cleanup:
	media_entity_cleanup(&s5k5e9->sd.entity);
err_ctrl_free:
	v4l2_ctrl_handler_free(&s5k5e9->ctrl_handler);
err_power_off:
	s5k5e9_power_off(s5k5e9->dev);
	return ret;
}

static void s5k5e9_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct s5k5e9 *s5k5e9 = to_s5k5e9(sd);

	v4l2_async_unregister_subdev(sd);
	v4l2_subdev_cleanup(sd);
	media_entity_cleanup(&sd->entity);
	v4l2_ctrl_handler_free(&s5k5e9->ctrl_handler);

	pm_runtime_disable(s5k5e9->dev);
	if (!pm_runtime_status_suspended(s5k5e9->dev))
		s5k5e9_power_off(s5k5e9->dev);
	pm_runtime_set_suspended(s5k5e9->dev);
}

static const struct dev_pm_ops s5k5e9_pm_ops = {
	SET_RUNTIME_PM_OPS(s5k5e9_power_off, s5k5e9_power_on, NULL)
};

static const struct of_device_id s5k5e9_of_match[] = {
	{ .compatible = "samsung,s5k5e9" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, s5k5e9_of_match);

static struct i2c_driver s5k5e9_i2c_driver = {
	.driver = {
		.name = "s5k5e9",
		.of_match_table = s5k5e9_of_match,
		.pm = &s5k5e9_pm_ops,
	},
	.probe = s5k5e9_probe,
	.remove = s5k5e9_remove,
};
module_i2c_driver(s5k5e9_i2c_driver);

MODULE_DESCRIPTION("Samsung S5K5E9 5MP CMOS image sensor driver");
MODULE_LICENSE("GPL");

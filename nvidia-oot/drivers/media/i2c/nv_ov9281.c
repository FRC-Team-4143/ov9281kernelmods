// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (c) 2021-2024, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
/*
 * Copyright (c) 2020, RidgeRun. All rights reserved.
 *
 * Contact us: support@ridgerun.com
 *
 * nv_ov9281.c - ov9281 sensor driver
 */

#include <nvidia/conftest.h>

#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/gpio.h>
#include <linux/module.h>
#include <linux/seq_file.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>

#include <media/tegra_v4l2_camera.h>
#include <media/tegracam_core.h>

#define OV9281_MIN_GAIN                         (0)
#define OV9281_MAX_GAIN                         (0xFE)
#define OV9281_ANALOG_GAIN_C0                   (1024)
#define OV9281_SHIFT_8_BITS                     (8)
#define OV9281_MIN_COARSE_EXPOSURE              (0x10)
#define OV9281_MAX_COARSE_EXPOSURE              (0x3750)
#define OV9281_MASK_LSB_2_BITS                  0x0003
#define OV9281_MASK_LSB_8_BITS                  0x00ff

#define OV9281_MODEL_ID_ADDR_MSB                0x300A
#define OV9281_MODEL_ID_ADDR_LSB                0x300B
#define OV9281_ANALOG_GAIN_SHIFT                0x3507
#define OV9281_ANALOG_GAIN_ADDR_MSB             0x3508
#define OV9281_ANALOG_GAIN_ADDR_LSB             0x3509
#define OV9281_COARSE_INTEG_TIME_ADDR_MSB       0x3500
#define OV9281_COARSE_INTEG_TIME_ADDR_MID       0x3501
#define OV9281_COARSE_INTEG_TIME_ADDR_LSB       0x3502

#include "../platform/tegra/camera/camera_gpio.h"
#include "ov9281_mode_tbls.h"

#define OV9281_SENSOR_INTERNAL_CLK_FREQ   800000000

static const struct of_device_id ov9281_of_match[] = {
	{.compatible = "ovti,ov9281",},
	{},
};

MODULE_DEVICE_TABLE(of, ov9281_of_match);

static const u32 ctrl_cid_list[] = {
	TEGRA_CAMERA_CID_GAIN,
	TEGRA_CAMERA_CID_EXPOSURE,
	TEGRA_CAMERA_CID_SENSOR_MODE_ID,
};

 static int sensor_mode = 5;
 //Sensor Work Mode: 0=1280x800x10bit_stream 1=1280x800x8bit_stream 2=1280x800x10bit_ext_trig 3=1280x800x8bit_ext_trig 4=1280x720_10_stream 5=1280x720_8_stream  6=1280x720_10_ext_trig  7=1280x720_8_ext_trig 8=640x400_10_stream  9=640x400_8_stream 10=640x400_10_ext_trig 11=640x400_8_ext_trig 12=320x200_8_stream 13=320x200_8_ext_trig

 struct inno_rom_table {
         char magic[12];
         char manuf[32];
         u16 manuf_id;
         char sen_manuf[8];
         char sen_type[16];
         u16 mod_id;
         u16 mod_rev;
         char regs[56];
         u16 nr_modes;
         u16 bytes_per_mode;
         char mode1[16];
         char mode2[16];
 };



struct ov9281 {
	struct i2c_client *i2c_client;
	struct v4l2_subdev *subdev;
	struct camera_common_data *s_data;
	struct tegracam_device *tc_dev;
	struct i2c_client *rom;
	struct inno_rom_table rom_table;
};

static const struct regmap_config sensor_regmap_config = {
	.reg_bits = 16,
	.val_bits = 8,
	.cache_type = REGCACHE_RBTREE,
	.use_single_read = true,
	.use_single_write = true,
};

static inline void ov9281_get_coarse_integ_time_regs(ov9281_reg *regs,
						     u32 coarse_time)
{
	regs->addr = OV9281_COARSE_INTEG_TIME_ADDR_MSB;
	regs->val = (coarse_time >> 16) & 0xff;
	(regs + 1)->addr = OV9281_COARSE_INTEG_TIME_ADDR_MID;
	(regs + 1)->val = (coarse_time >> 8) & 0xff;
	(regs + 2)->addr = OV9281_COARSE_INTEG_TIME_ADDR_LSB;
	(regs + 2)->val = (coarse_time) & 0xff;
}

static inline void ov9281_get_gain_reg(ov9281_reg *reg, u16 gain)
{
	reg->addr = OV9281_ANALOG_GAIN_ADDR_MSB;
	reg->val = (gain >> OV9281_SHIFT_8_BITS) & OV9281_MASK_LSB_2_BITS;

	(reg + 1)->addr = OV9281_ANALOG_GAIN_ADDR_LSB;
	(reg + 1)->val = (gain) & OV9281_MASK_LSB_8_BITS;
}

static inline int ov9281_read_reg(struct camera_common_data *s_data,
				  u16 addr, u8 *val)
{
	int err = 0;
	u32 reg_val = 0;

	err = regmap_read(s_data->regmap, addr, &reg_val);
	*val = reg_val & 0xff;

	return err;
}

static inline int ov9281_write_reg(struct camera_common_data *s_data,
				   u16 addr, u8 val)
{
	int err = 0;

	err = regmap_write(s_data->regmap, addr, val);
	if (err)
		dev_err(s_data->dev, "%s: i2c write failed, 0x%x = %x",
			__func__, addr, val);

	return err;
}

static int ov9281_write_table(struct ov9281 *priv, const ov9281_reg table[])
{
	return regmap_util_write_table_8(priv->s_data->regmap, table, NULL, 0,
					 OV9281_TABLE_WAIT_MS,
					 OV9281_TABLE_END);
}

static int rom_write(struct i2c_client *client, const u8 addr, const u8 data)
{
        struct i2c_adapter *adap = client->adapter;
        struct i2c_msg msg;
        u8 tx[2];
        int ret;

        msg.addr = client->addr;
        msg.buf = tx;
        msg.len = 2;
        msg.flags = 0;
        tx[0] = addr;
        tx[1] = data;
        ret = i2c_transfer(adap, &msg, 1);
        mdelay(2);

        return ret == 1 ? 0 : -EIO;
}

static int rom_read(struct i2c_client *client, const u8 addr)
{
        u8 buf[1]={ addr };
        int ret;
        struct i2c_msg msgs[] = {
                {
                        .addr  = client->addr,
                        .flags = 0,
                        .len   = 1,
                        .buf   = buf,
                }, {
                        .addr  = client->addr,
                        .flags = I2C_M_RD,
                        .len   = 1,
                        .buf   = buf,
                },
        };

        ret = i2c_transfer(client->adapter, msgs, ARRAY_SIZE(msgs));
        if (ret < 0) {
                dev_warn(&client->dev, "Reading register %x from %x failed\n",
                         addr, client->addr);
                return ret;
        }

        return buf[0];
}


static int ov9281_set_gain(struct tegracam_device *tc_dev, s64 val)
{
	struct camera_common_data *s_data = tc_dev->s_data;
	struct device *dev = s_data->dev;
	const struct sensor_mode_properties *mode =
	    &s_data->sensor_props.sensor_modes[s_data->mode_prop_idx];
	int err = 0, i = 0;
	ov9281_reg gain_reg[2];
	s16 gain;

	dev_dbg(dev, "%s: Setting gain control to: %lld\n", __func__, val);

	if (val < mode->control_properties.min_gain_val)
		val = mode->control_properties.min_gain_val;
	else if (val > mode->control_properties.max_gain_val)
		val = mode->control_properties.max_gain_val;

	//if (val == 0)
	//	return -EINVAL;
	gain = val;
	    //(s16) (OV9281_ANALOG_GAIN_C0 -
	//	   (mode->control_properties.gain_factor *
	//	    OV9281_ANALOG_GAIN_C0 / val));

	if (gain < OV9281_MIN_GAIN)
		gain = OV9281_MIN_GAIN;
	else if (gain > OV9281_MAX_GAIN)
		gain = OV9281_MAX_GAIN;

	dev_dbg(dev, "%s: val: %lld (/%d) [times], gain: %u\n",
		__func__, val, mode->control_properties.gain_factor, gain);

	ov9281_get_gain_reg(gain_reg, (u16) gain);

	for (i = 0; i < ARRAY_SIZE(gain_reg); i++) {
		err = ov9281_write_reg(s_data, gain_reg[i].addr,
				       gain_reg[i].val);
		if (err) {
			dev_err(dev, "%s: gain control error\n", __func__);
			break;
		}
	}

	return err;
}

static int ov9281_set_exposure(struct tegracam_device *tc_dev, s64 val)
{
	struct camera_common_data *s_data = tc_dev->s_data;
	//struct ov9281 *priv = (struct ov9281 *)tc_dev->priv;
	struct device *dev = tc_dev->dev;
	const struct sensor_mode_properties *mode =
	    &s_data->sensor_props.sensor_modes[s_data->mode_prop_idx];

	int err = 0;
	ov9281_reg ct_regs[3];

	const s32 max_coarse_time = OV9281_MAX_COARSE_EXPOSURE;
	s32 fine_integ_time_factor;
	u32 coarse_time;
	int i;

	if (mode->signal_properties.pixel_clock.val == 0 ||
		 mode->control_properties.exposure_factor == 0 ||
		 mode->image_properties.line_length == 0)
		return -EINVAL;

	fine_integ_time_factor = mode->control_properties.exposure_factor ;

	dev_dbg(dev, "%s: Setting exposure control to: %lld factor %d\n", __func__, val, fine_integ_time_factor);

	coarse_time = val;
	 //(val - fine_integ_time_factor)
	  //   * OV9281_SENSOR_INTERNAL_CLK_FREQ
	   //  / mode->control_properties.exposure_factor
	    // / mode->image_properties.line_length;

	if (coarse_time < OV9281_MIN_COARSE_EXPOSURE)
		coarse_time = OV9281_MIN_COARSE_EXPOSURE;
	else if (coarse_time > max_coarse_time) {
		coarse_time = max_coarse_time;
		dev_dbg(dev,
			"%s: exposure limited by frame_length: %d [lines]\n",
			__func__, max_coarse_time);
	}

	dev_dbg(dev, "%s: val: %lld [us], coarse_time: %d [lines]\n",
		__func__, val, coarse_time);

	ov9281_get_coarse_integ_time_regs(ct_regs, coarse_time);

	for (i = 0; i < 2; i++) {
		err = ov9281_write_reg(s_data, ct_regs[i].addr, ct_regs[i].val);
		if (err) {
			dev_dbg(dev,
				"%s: coarse_time control error\n", __func__);
			return err;
		}
	}

	return err;
}

static int imx477_set_group_hold(struct tegracam_device *tc_dev, bool val)
{
        //struct camera_common_data *s_data = tc_dev->s_data;
        struct device *dev = tc_dev->dev;
        //int err;

        dev_dbg(dev, "%s: Setting group hold control to: %u\n", __func__, val);

        //err = imx477_write_reg(s_data, IMX477_GROUP_HOLD_ADDR, val);
        //if (err) {
        //        dev_err(dev, "%s: Group hold control error\n", __func__);
        //        return err;
        // }

        return 0;
}


static struct tegracam_ctrl_ops ov9281_ctrl_ops = {
	.numctrls = ARRAY_SIZE(ctrl_cid_list),
	.ctrl_cid_list = ctrl_cid_list,
	.set_gain = ov9281_set_gain,
	.set_exposure = ov9281_set_exposure,
        .set_group_hold = imx477_set_group_hold,
};

static int ov9281_power_on(struct camera_common_data *s_data)
{
	int err = 0;
	struct camera_common_power_rail *pw = s_data->power;
	struct camera_common_pdata *pdata = s_data->pdata;
	struct device *dev = s_data->dev;

	dev_dbg(dev, "%s: power on\n", __func__);
	if (pdata && pdata->power_on) {
		err = pdata->power_on(pw);
		if (err)
			dev_err(dev, "%s failed.\n", __func__);
		else
			pw->state = SWITCH_ON;
		return err;
	}

	if (pw->reset_gpio) {
		if (gpiod_cansleep(gpio_to_desc(pw->reset_gpio)))
			gpio_set_value_cansleep(pw->reset_gpio, 0);
		else
			gpio_set_value(pw->reset_gpio, 0);
	}

	if (unlikely(!(pw->avdd || pw->iovdd || pw->dvdd)))
		goto skip_power_seqn;

	usleep_range(10, 20);

	if (pw->avdd) {
		err = regulator_enable(pw->avdd);
		if (err)
			goto ov9281_avdd_fail;
	}

	if (pw->iovdd) {
		err = regulator_enable(pw->iovdd);
		if (err)
			goto ov9281_iovdd_fail;
	}

	if (pw->dvdd) {
		err = regulator_enable(pw->dvdd);
		if (err)
			goto ov9281_dvdd_fail;
	}

	usleep_range(10, 20);

skip_power_seqn:
	if (pw->reset_gpio) {
		if (gpiod_cansleep(gpio_to_desc(pw->reset_gpio)))
			gpio_set_value_cansleep(pw->reset_gpio, 1);
		else
			gpio_set_value(pw->reset_gpio, 1);
	}

	/* Need to wait for t4 + t5 + t9 + t10 time as per the data sheet */
	/* t4 - 200us, t5 - 21.2ms, t9 - 1.2ms t10 - 270 ms */
	usleep_range(300000, 300100);

	pw->state = SWITCH_ON;

	return 0;

ov9281_dvdd_fail:
	regulator_disable(pw->iovdd);

ov9281_iovdd_fail:
	regulator_disable(pw->avdd);

ov9281_avdd_fail:
	dev_err(dev, "%s failed.\n", __func__);

	return -ENODEV;
}

static int ov9281_power_off(struct camera_common_data *s_data)
{
	int err = 0;
	struct camera_common_power_rail *pw = s_data->power;
	struct camera_common_pdata *pdata = s_data->pdata;
	struct device *dev = s_data->dev;

	dev_dbg(dev, "%s: power off\n", __func__);

	if (pdata && pdata->power_off) {
		err = pdata->power_off(pw);
		if (err) {
			dev_err(dev, "%s failed.\n", __func__);
			return err;
		}
	} else {
		if (pw->reset_gpio) {
			if (gpiod_cansleep(gpio_to_desc(pw->reset_gpio)))
				gpio_set_value_cansleep(pw->reset_gpio, 0);
			else
				gpio_set_value(pw->reset_gpio, 0);
		}

		usleep_range(10, 10);

		if (pw->dvdd)
			regulator_disable(pw->dvdd);
		if (pw->iovdd)
			regulator_disable(pw->iovdd);
		if (pw->avdd)
			regulator_disable(pw->avdd);
	}

	pw->state = SWITCH_OFF;

	return 0;
}

static int ov9281_power_put(struct tegracam_device *tc_dev)
{
	struct camera_common_data *s_data = tc_dev->s_data;
	struct camera_common_power_rail *pw = s_data->power;

	if (unlikely(!pw))
		return -EFAULT;

	if (likely(pw->dvdd))
		devm_regulator_put(pw->dvdd);

	if (likely(pw->avdd))
		devm_regulator_put(pw->avdd);

	if (likely(pw->iovdd))
		devm_regulator_put(pw->iovdd);

	pw->dvdd = NULL;
	pw->avdd = NULL;
	pw->iovdd = NULL;

	if (likely(pw->reset_gpio))
		gpio_free(pw->reset_gpio);

	return 0;
}

static int ov9281_power_get(struct tegracam_device *tc_dev)
{
	struct device *dev = tc_dev->dev;
	struct camera_common_data *s_data = tc_dev->s_data;
	struct camera_common_power_rail *pw = s_data->power;
	struct camera_common_pdata *pdata = s_data->pdata;
	struct clk *parent;
	int err = 0;

	if (!pdata) {
		dev_err(dev, "pdata missing\n");
		return -EFAULT;
	}

	/* Sensor MCLK (aka. INCK) */
	if (pdata->mclk_name) {
		pw->mclk = devm_clk_get(dev, pdata->mclk_name);
		if (IS_ERR(pw->mclk)) {
			dev_err(dev, "unable to get clock %s\n",
				pdata->mclk_name);
			return PTR_ERR(pw->mclk);
		}

		if (pdata->parentclk_name) {
			parent = devm_clk_get(dev, pdata->parentclk_name);
			if (IS_ERR(parent)) {
				dev_err(dev, "unable to get parent clock %s",
					pdata->parentclk_name);
			} else
				clk_set_parent(pw->mclk, parent);
		}
	}

	/* analog 2.8v */
	if (pdata->regulators.avdd)
		err |= camera_common_regulator_get(dev,
						   &pw->avdd,
						   pdata->regulators.avdd);
	/* IO 1.8v */
	if (pdata->regulators.iovdd)
		err |= camera_common_regulator_get(dev,
						   &pw->iovdd,
						   pdata->regulators.iovdd);
	/* dig 1.2v */
	if (pdata->regulators.dvdd)
		err |= camera_common_regulator_get(dev,
						   &pw->dvdd,
						   pdata->regulators.dvdd);
	if (err) {
		dev_err(dev, "%s: unable to get regulator(s)\n", __func__);
		goto done;
	}

	/* Reset or ENABLE GPIO */
	pw->reset_gpio = pdata->reset_gpio;
	err = gpio_request(pw->reset_gpio, "cam_reset_gpio");
	if (err < 0) {
		dev_err(dev, "%s: unable to request reset_gpio (%d)\n",
			__func__, err);
		goto done;
	}

done:
	pw->state = SWITCH_OFF;

	return err;
}

static struct camera_common_pdata *ov9281_parse_dt(struct tegracam_device
						   *tc_dev)
{
	struct device *dev = tc_dev->dev;
	struct device_node *np = dev->of_node;
	struct camera_common_pdata *board_priv_pdata;
	const struct of_device_id *match;
	struct camera_common_pdata *ret = NULL;
	int err = 0;
	int gpio;

	if (!np)
		return NULL;

	match = of_match_device(ov9281_of_match, dev);
	if (!match) {
		dev_err(dev, "Failed to find matching dt id\n");
		return NULL;
	}

	board_priv_pdata = devm_kzalloc(dev,
					sizeof(*board_priv_pdata), GFP_KERNEL);
	if (!board_priv_pdata)
		return NULL;

	gpio = of_get_named_gpio(np, "reset-gpios", 0);
	if (gpio < 0) {
		if (gpio == -EPROBE_DEFER)
			ret = ERR_PTR(-EPROBE_DEFER);
		dev_err(dev, "reset-gpios not found\n");
		goto error;
	}
	board_priv_pdata->reset_gpio = (unsigned int)gpio;

	err = of_property_read_string(np, "mclk", &board_priv_pdata->mclk_name);
	if (err)
		dev_dbg(dev,
			"mclk name not present, assume sensor driven externally\n");

	err = of_property_read_string(np, "avdd-reg",
				      &board_priv_pdata->regulators.avdd);
	err |= of_property_read_string(np, "iovdd-reg",
				       &board_priv_pdata->regulators.iovdd);
	err |= of_property_read_string(np, "dvdd-reg",
				       &board_priv_pdata->regulators.dvdd);
	if (err)
		dev_dbg(dev,
		"avdd, iovdd and/or dvdd reglrs. not present, assume sensor powered independently\n");

	board_priv_pdata->has_eeprom = of_property_read_bool(np, "has-eeprom");

	return board_priv_pdata;

error:
	devm_kfree(dev, board_priv_pdata);

	return ret;
}

static int ov9281_set_mode(struct tegracam_device *tc_dev)
{
	struct ov9281 *priv = (struct ov9281 *)tegracam_get_privdata(tc_dev);
	struct camera_common_data *s_data = tc_dev->s_data;
	unsigned int mode_index = 0;
	int err = 0;
	struct device_node *mode;

	dev_dbg(tc_dev->dev, "%s:\n", __func__);
	mode = of_get_child_by_name(tc_dev->dev->of_node, "mode0");

	err = ov9281_write_table(priv, mode_table[OV9281_MODE_COMMON]);
	if (err)
		return err;

	mode_index = s_data->mode;
	err = ov9281_write_table(priv, mode_table[mode_index]);

	if (err)
		return err;

	return 0;
}

static int ov9281_start_streaming(struct tegracam_device *tc_dev)
{
	struct ov9281 *priv = (struct ov9281 *)tegracam_get_privdata(tc_dev);

	dev_dbg(tc_dev->dev, "%s:\n", __func__);
	return ov9281_write_table(priv, mode_table[OV9281_START_STREAM]);
}

static int ov9281_stop_streaming(struct tegracam_device *tc_dev)
{
	int err;
	struct ov9281 *priv = (struct ov9281 *)tegracam_get_privdata(tc_dev);

	dev_dbg(tc_dev->dev, "%s:\n", __func__);
	err = ov9281_write_table(priv, mode_table[OV9281_STOP_STREAM]);

	return err;
}

static struct camera_common_sensor_ops ov9281_common_ops = {
	.numfrmfmts = ARRAY_SIZE(ov9281_frmfmt),
	.frmfmt_table = ov9281_frmfmt,
	.power_on = ov9281_power_on,
	.power_off = ov9281_power_off,
	.write_reg = ov9281_write_reg,
	.read_reg = ov9281_read_reg,
	.parse_dt = ov9281_parse_dt,
	.power_get = ov9281_power_get,
	.power_put = ov9281_power_put,
	.set_mode = ov9281_set_mode,
	.start_streaming = ov9281_start_streaming,
	.stop_streaming = ov9281_stop_streaming,
};

static int ov9281_board_setup(struct ov9281 *priv)
{
	struct camera_common_data *s_data = priv->s_data;
	struct device *dev = s_data->dev;
	u8 reg_val[2];
	int err = 0;

	/* Skip mclk enable as this camera has an internal oscillator */

	err = ov9281_power_on(s_data);
	if (err) {
		dev_err(dev, "error during power on sensor (%d)\n", err);
		goto done;
	}

	/* Probe sensor model id registers */
	err = ov9281_read_reg(s_data, OV9281_MODEL_ID_ADDR_MSB, &reg_val[0]);
	if (err) {
		dev_err(dev, "%s: error during i2c read probe (%d)\n",
			__func__, err);
		goto err_reg_probe;
	}
	err = ov9281_read_reg(s_data, OV9281_MODEL_ID_ADDR_LSB, &reg_val[1]);
	if (err) {
		dev_err(dev, "%s: error during i2c read probe (%d)\n",
			__func__, err);
		goto err_reg_probe;
	}

	if (!((reg_val[0] == 0x92) && reg_val[1] == 0x81))
		dev_err(dev, "%s: invalid sensor model id: %x%x\n",
			__func__, reg_val[0], reg_val[1]);

err_reg_probe:
	ov9281_power_off(s_data);

done:
	return err;
}

static int ov9281_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	dev_dbg(&client->dev, "%s:\n", __func__);

	return 0;
}

static const struct v4l2_subdev_internal_ops ov9281_subdev_internal_ops = {
	.open = ov9281_open,
};

#if defined(NV_I2C_DRIVER_STRUCT_PROBE_WITHOUT_I2C_DEVICE_ID_ARG) /* Linux 6.3 */
static int ov9281_probe(struct i2c_client *client)
#else
static int ov9281_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
#endif
{
	struct device *dev = &client->dev;
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct tegracam_device *tc_dev;
	struct ov9281 *priv;
	int err;

	dev_dbg(dev, "probing v4l2 sensor at addr 0x%0x\n", client->addr);

	if (!IS_ENABLED(CONFIG_OF) || !client->dev.of_node)
		return -EINVAL;

	priv = devm_kzalloc(dev, sizeof(struct ov9281), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	tc_dev = devm_kzalloc(dev, sizeof(struct tegracam_device), GFP_KERNEL);
	if (!tc_dev)
		return -ENOMEM;

	priv->i2c_client = tc_dev->client = client;

	priv->rom = i2c_new_dummy_device(adapter,0x10);  /*_device*/
        if ( priv->rom )
        {
                static int i=1;
                int addr,reg,data;
                dev_info(&client->dev, "InnoMaker Camera controller found!\n");
#if 1
                for (addr=0; addr<sizeof(priv->rom_table); addr++)
                {
                  reg = rom_read(priv->rom, addr);
                  *((char *)(&(priv->rom_table))+addr)=(char)reg;
                  dev_dbg(&client->dev, "addr=0x%04x reg=0x%02x\n",addr,reg);
                }

                dev_info(&client->dev, "[ MAGIC  ] [ %s ]\n",
                                priv->rom_table.magic);

                dev_info(&client->dev, "[ MANUF. ] [ %s ] [ MID=0x%04x ]\n",
                                priv->rom_table.manuf,
                                priv->rom_table.manuf_id);

                dev_info(&client->dev, "[ SENSOR ] [ %s %s ]\n",
                                priv->rom_table.sen_manuf,
                                priv->rom_table.sen_type);

                dev_info(&client->dev, "[ MODULE ] [ ID=0x%04x ] [ REV=0x%04x ]\n",
                                priv->rom_table.mod_id,
                                priv->rom_table.mod_rev);

                dev_info(&client->dev, "[ MODES  ] [ NR=0x%04x ] [ BPM=0x%04x ]\n",
                                priv->rom_table.nr_modes,
                                priv->rom_table.bytes_per_mode);
#endif
                addr = 200; // reset
                data =   2; // powerdown sensor
                reg = rom_write(priv->rom, addr, data);

                addr = 202; // mode
                data = sensor_mode; // default 8-bit streaming
                reg = rom_write(priv->rom, addr, data);

                //addr = 200; // reset
                //data =   0; // powerup sensor
                //reg = reg_write(priv->rom, addr, data);

                while(1)
               {
                        mdelay(100); // wait 100ms

                        addr = 201; // status
                        reg = rom_read(priv->rom, addr);

                        if(reg & 0x80)
                                break;

                        if(reg & 0x01)
                                dev_err(&client->dev, "!!! ERROR !!! setting  Sensor MODE=%d STATUS=0x%02x i=%d\n",sensor_mode,reg,i);

                        if(i++ >  4)
                                break;
                }

                dev_info(&client->dev, " Sensor MODE=%d PowerOn STATUS=0x%02x i=%d\n",sensor_mode,reg,i);

        }
        else
        {

                dev_err(&client->dev, "NOTE !!!  External Camera controller  not found !!!\n");
                dev_info(&client->dev, "Sensor MODE=%d \n",sensor_mode);
                return -EIO;
        }

	tc_dev->dev = dev;
	strncpy(tc_dev->name, "ov9281", sizeof(tc_dev->name));
	tc_dev->dev_regmap_config = &sensor_regmap_config;
	tc_dev->sensor_ops = &ov9281_common_ops;
	tc_dev->v4l2sd_internal_ops = &ov9281_subdev_internal_ops;
	tc_dev->tcctrl_ops = &ov9281_ctrl_ops;

	err = tegracam_device_register(tc_dev);
	if (err) {
		dev_err(dev, "tegra camera driver registration failed\n");
		return err;
	}
	priv->tc_dev = tc_dev;
	priv->s_data = tc_dev->s_data;
	priv->subdev = &tc_dev->s_data->subdev;
	tegracam_set_privdata(tc_dev, (void *)priv);

	err = ov9281_board_setup(priv);
	if (err) {
		dev_err(dev, "board setup failed\n");
		return err;
	}

	err = tegracam_v4l2subdev_register(tc_dev, true);
	if (err) {
		tegracam_device_unregister(tc_dev);
		dev_err(dev, "tegra camera subdev registration failed\n");
		return err;
	}

	dev_dbg(dev, "detected ov9281 sensor\n");

	return 0;
}

#if defined(NV_I2C_DRIVER_STRUCT_REMOVE_RETURN_TYPE_INT) /* Linux 6.1 */
static int ov9281_remove(struct i2c_client *client)
#else
static void ov9281_remove(struct i2c_client *client)
#endif
{
	struct camera_common_data *s_data = to_camera_common_data(&client->dev);
	struct ov9281 *priv;

	if (!s_data) {
		dev_err(&client->dev, "camera common data is NULL\n");
#if defined(NV_I2C_DRIVER_STRUCT_REMOVE_RETURN_TYPE_INT) /* Linux 6.1 */
		return -EINVAL;
#else
		return;
#endif
	}
	priv = (struct ov9281 *)s_data->priv;

	tegracam_v4l2subdev_unregister(priv->tc_dev);
	tegracam_device_unregister(priv->tc_dev);
#if defined(NV_I2C_DRIVER_STRUCT_REMOVE_RETURN_TYPE_INT) /* Linux 6.1 */
	return 0;
#endif
}

static const struct i2c_device_id ov9281_id[] = {
	{"ov9281", 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, ov9281_id);

static struct i2c_driver ov9281_i2c_driver = {
	.driver = {
		   .name = "ov9281",
		   .owner = THIS_MODULE,
		   .of_match_table = of_match_ptr(ov9281_of_match),
		   },
	.probe = ov9281_probe,
	.remove = ov9281_remove,
	.id_table = ov9281_id,
};

module_i2c_driver(ov9281_i2c_driver);

MODULE_DESCRIPTION("Media Controller driver for Innovision OV9281");
MODULE_AUTHOR("Ryan Shoff");
MODULE_LICENSE("GPL v2");

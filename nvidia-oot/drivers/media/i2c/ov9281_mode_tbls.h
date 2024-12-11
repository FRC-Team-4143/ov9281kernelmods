/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020-2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 *
 */

#ifndef __OV9281_I2C_TABLES__
#define __OV9281_I2C_TABLES__

#include <media/camera_common.h>

#define OV9281_TABLE_WAIT_MS 0
#define OV9281_TABLE_END 1
#define OV9281_WAIT_MS 1
#define OV9281_STANDBY_REG 0x0100

#define ov9281_reg struct reg_8

static const ov9281_reg ov9281_start[] = {
	{OV9281_STANDBY_REG, 0x1},
	{OV9281_TABLE_END, 0x00}
};

static const ov9281_reg ov9281_stop[] = {
	{OV9281_STANDBY_REG, 0x0},
	{OV9281_TABLE_END, 0x00}
};

static const ov9281_reg ov9281_mode_common[] = {
	/* software reset */
	{OV9281_TABLE_WAIT_MS, OV9281_WAIT_MS*10},
	{OV9281_TABLE_END, 0x0000}

};

static const ov9281_reg ov9281_mode_1280x800[] = {
       {OV9281_TABLE_END, 0x00 }
};

static const ov9281_reg ov9281_mode_1280x720[] = {
       {OV9281_TABLE_END, 0x00 }
};

static const ov9281_reg ov9281_mode_640x400[] = {
       {OV9281_TABLE_END, 0x00 }
};


enum {
	OV9281_MODE_1280x720_8,
	OV9281_MODE_COMMON,
	OV9281_START_STREAM,
	OV9281_STOP_STREAM,
};

static const ov9281_reg *mode_table[] = {
	[OV9281_MODE_1280x720_8] = ov9281_mode_1280x720,
	[OV9281_MODE_COMMON] = ov9281_mode_common,
	[OV9281_START_STREAM] = ov9281_start,
	[OV9281_STOP_STREAM] = ov9281_stop,
};

static const int ov9281_144_fr[] = {
	144,
};

static const int ov9281_120_fr[] = {
	120,
};

static const int ov9281_60_fr[] = {
	60,
};

static const struct camera_common_frmfmt ov9281_frmfmt[] = {
	{{1280, 720}, ov9281_144_fr, 1, 0, OV9281_MODE_1280x720_8},
};
#endif /* __OV9281_I2C_TABLES__ */

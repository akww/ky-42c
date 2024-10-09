/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */
#ifndef _CAM_CAL_DATA_H
#define _CAM_CAL_DATA_H

#ifdef CONFIG_COMPAT
/* 64 bit */
#include <linux/fs.h>
#include <linux/compat.h>
#endif

struct stCAM_CAL_INFO_STRUCT {
	u32 u4Offset;
	u32 u4Length;
	u32 sensorID;
	/*
	 * MAIN = 0x01,
	 * SUB  = 0x02,
	 * MAIN_2 = 0x04,
	 * SUB_2 = 0x08,
	 * MAIN_3 = 0x10,
	 */
	u32 deviceID;
	u8 *pu1Params;
};

#ifdef CONFIG_COMPAT

struct COMPAT_stCAM_CAL_INFO_STRUCT {
	u32 u4Offset;
	u32 u4Length;
	u32 sensorID;
	u32 deviceID;
	compat_uptr_t pu1Params;
};
#endif

#define OV08D10_EEPROM_MAX_SIZE 0x788

enum OP_FLAGS_OV08D10 {
	OV08D10_SENSOR_I2C_DEFAULT = 0,
	OV08D10_SENSOR_I2C_READ,
	OV08D10_SENSOR_I2C_WRITE,
	OV08D10_SENSOR_EEPROM_LOAD,
};

struct FAC_CTX_OV08D10 {
	enum OP_FLAGS_OV08D10 flag;
	int size;
	unsigned int addr;
	unsigned short data;
};

struct FAC_DATA_OV_EEPROM {
	int size;
	unsigned short data[OV08D10_EEPROM_MAX_SIZE];
};

#endif/*_CAM_CAL_DATA_H*/

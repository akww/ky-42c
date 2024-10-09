/*
 * This software is contributed or developed by KYOCERA Corporation.
 * (C) 2022 KYOCERA Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _OEM_CHG_DNAND_H
#define _OEM_CHG_DNAND_H

enum oem_chg_dnand_property {
	OEM_CHG_DNAND_PROP_CYCLE_COUNT = 0,
	OEM_CHG_DNAND_PROP_CYCLE_INCREASE,
	OEM_CHG_DNAND_PROP_ONLINE_TIME,
	OEM_CHG_DNAND_PROP_BATTERY_CARE_MODE,
	OEM_CHG_DNAND_PROP_BATTERY_CARE_NOTIFICATION,
	OEM_CHG_DNAND_PROP_AUTO_ON_ENABLE,
	MAX_OEM_CHG_DNAND_PROP
};

struct oem_chg_dnand_chip {
	struct device *dev;
};

extern int oem_chg_dnand_set_property(enum oem_chg_dnand_property ocdp, int *val);
extern int oem_chg_dnand_get_property(enum oem_chg_dnand_property ocdp, int *val);
extern int oem_chg_dnand_sync_property(void);

#endif

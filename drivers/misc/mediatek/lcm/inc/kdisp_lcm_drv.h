/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */
/*
 * This software is contributed or developed by KYOCERA Corporation.
 * (C) 2022 KYOCERA Corporation
*/

#ifndef __KDISP_LCM_DRV_H__
#define __KDISP_LCM_DRV_H__

struct KDISP_LCM_DRIVER {
	void (*pre_off)(void);
	void (*post_off)(void);
	void (*pre_on)(void);
	void (*post_on)(void);
	unsigned int (*check_refresh_disable)(void);
	void (*panel_detect_notify)(int status);
};

#endif /* __LCM_DRV_H__ */

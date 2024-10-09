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

#ifndef _KDISP_LCM_H_
#define _KDISP_LCM_H_
#include "lcm_drv.h"

int kdisp_lcm_pre_off(struct disp_lcm_handle *plcm);
int kdisp_lcm_post_off(struct disp_lcm_handle *plcm);
int kdisp_lcm_pre_on(struct disp_lcm_handle *plcm);
int kdisp_lcm_post_on(struct disp_lcm_handle *plcm);
int kdisp_lcm_check_refresh_disable(struct disp_lcm_handle *plcm);
int kdisp_lcm_panel_detect_notify(struct disp_lcm_handle *plcm, int isConnected);

#endif

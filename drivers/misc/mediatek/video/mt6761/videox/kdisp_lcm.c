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

#include <linux/slab.h>

#include <linux/types.h>
#include "disp_drv_log.h"
#include "lcm_drv.h"
#include "lcm_define.h"
#include "disp_drv_platform.h"
#include "ddp_manager.h"
#include "disp_lcm.h"

#include "disp_dts_gpio.h"
#include <linux/delay.h>

extern int _is_lcm_inited(struct disp_lcm_handle *plcm);

int kdisp_lcm_pre_off(struct disp_lcm_handle *plcm)
{
	struct LCM_DRIVER *lcm_drv = NULL;

	DISPFUNC();
	if (_is_lcm_inited(plcm)) {
		lcm_drv = plcm->drv;

		if (lcm_drv->kdisp_lcm_drv.pre_off) {
			lcm_drv->kdisp_lcm_drv.pre_off();
		} else {
			DISPERR("[KCDISP] lcm_drv->pre_off is null\n");
			return -1;
		}

		return 0;
	}
	DISPERR("lcm_drv is null\n");
	return -1;
}

int kdisp_lcm_post_off(struct disp_lcm_handle *plcm)
{
	struct LCM_DRIVER *lcm_drv = NULL;

	DISPFUNC();
	if (_is_lcm_inited(plcm)) {
		lcm_drv = plcm->drv;

		if (lcm_drv->kdisp_lcm_drv.post_off) {
			lcm_drv->kdisp_lcm_drv.post_off();
		} else {
			DISPERR("[KCDISP] lcm_drv->post_off is null\n");
			return -1;
		}

		return 0;
	}
	DISPERR("lcm_drv is null\n");
	return -1;
}

int kdisp_lcm_pre_on(struct disp_lcm_handle *plcm)
{
	struct LCM_DRIVER *lcm_drv = NULL;

	DISPFUNC();
	if (_is_lcm_inited(plcm)) {
		lcm_drv = plcm->drv;

		if (lcm_drv->kdisp_lcm_drv.pre_on) {
			lcm_drv->kdisp_lcm_drv.pre_on();
		} else {
			DISPERR("[KCDISP] lcm_drv->pre_on is null\n");
			return -1;
		}

		return 0;
	}
	DISPERR("lcm_drv is null\n");
	return -1;
}

int kdisp_lcm_post_on(struct disp_lcm_handle *plcm)
{
	struct LCM_DRIVER *lcm_drv = NULL;

	DISPFUNC();
	if (_is_lcm_inited(plcm)) {
		lcm_drv = plcm->drv;

		if (lcm_drv->kdisp_lcm_drv.post_on) {
			lcm_drv->kdisp_lcm_drv.post_on();
		} else {
			DISPERR("[KCDISP] lcm_drv->post_on is null\n");
			return -1;
		}

		return 0;
	}
	DISPERR("lcm_drv is null\n");
	return -1;
}

int kdisp_lcm_check_refresh_disable(struct disp_lcm_handle *plcm)
{
	unsigned int ret = 0;
	struct LCM_DRIVER *lcm_drv = NULL;

	DISPFUNC();
	if (_is_lcm_inited(plcm)) {
		lcm_drv = plcm->drv;
		if (lcm_drv->kdisp_lcm_drv.check_refresh_disable) {

			ret = lcm_drv->kdisp_lcm_drv.check_refresh_disable();
		} else {
			DISPERR("[KCDISP] lcm_drv->check_refresh_disable is null\n");
			return 0;
		}

		return ret;
	}

	DISPERR("lcm_drv is null\n");
	return 0;
}

int kdisp_lcm_panel_detect_notify(struct disp_lcm_handle *plcm, int isConnected)
{
	struct LCM_DRIVER *lcm_drv = NULL;

	DISPFUNC();
	if (_is_lcm_inited(plcm)) {
		lcm_drv = plcm->drv;
		if (lcm_drv->kdisp_lcm_drv.panel_detect_notify) {
			lcm_drv->kdisp_lcm_drv.panel_detect_notify(isConnected);
		} else {
			DISPERR("[KCDISP] lcm_drv->panel_active_notify is null\n");
			return -1;
		}

		return 0;
	}

	DISPERR("lcm_drv is null\n");
	return -1;
}

/*
 * This software is contributed or developed by KYOCERA Corporation.
 * (C) 2020 KYOCERA Corporation
 * (C) 2022 KYOCERA Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
*/

#include <linux/fb.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include "disp_ext_sub_panel.h"

#define DISP_EXP_SUB_PANEL_LD7032  "ld7032"
#define DISP_EXP_SUB_PANEL_ST7571  "st7571"

struct disp_ext_sub_funcs disp_ext_sub_func;

int disp_ext_sub_panel_reset_cntrl(int onoff);

int disp_ext_sub_get_panel_detect(struct device_node * np, struct disp_ext_sub_pdata *pdata) {
    const char *name;
    int ret = 0;
    ret = of_property_read_string(np, "kc,disp-ext-sub-panel-name", &name);
    if (ret) {
        pr_err("%s end - fail get panel num\n", __func__);
        return -EINVAL;
    }
    memset(&disp_ext_sub_func, 0, sizeof(struct disp_ext_sub_funcs));
    if (strcmp(name, DISP_EXP_SUB_PANEL_LD7032) == 0) {
        pr_notice("%s: Detect ld7032. \n", __func__);
        ld7032_disp_ext_sub_func_register(&disp_ext_sub_func);
    } else if (strcmp(name, DISP_EXP_SUB_PANEL_ST7571) == 0) {
        pr_notice("%s: Detect st7571. \n", __func__);
        st7571_disp_ext_sub_func_register(&disp_ext_sub_func);
    } else {
        pr_err("%s: Panel not support. \n", __func__);
    	ret = -1;
    }
    return ret;
}

int disp_ext_sub_panel_set_status(disp_ext_sub_state_type next_status, struct disp_ext_sub_pdata *pdata) {
    if (disp_ext_sub_func.pdisp_ext_sub_panel_set_status != NULL) {
        pr_debug("%s \n", __func__);
        return disp_ext_sub_func.pdisp_ext_sub_panel_set_status(next_status, pdata);
    } else {
        pr_notice("%s: Not support. \n", __func__);
        return 0;
    }
}

int disp_ext_sub_panel_update(struct fb_var_screeninfo *var, struct fb_info *info, uint8_t* apps_img_p) {
    if (disp_ext_sub_func.pdisp_ext_sub_panel_update != NULL) {
        pr_debug("%s \n", __func__);
        return disp_ext_sub_func.pdisp_ext_sub_panel_update(var, info, apps_img_p);
    } else {
        pr_notice("%s: Not support. \n", __func__);
        return 0;
    }
}

int disp_ext_sub_set_cmd(void * payload_p, unsigned char payload_len, struct disp_ext_sub_pdata *pdata) {
    if (disp_ext_sub_func.pdisp_ext_sub_set_cmd != NULL) {
        pr_debug("%s \n", __func__);
        return disp_ext_sub_func.pdisp_ext_sub_set_cmd(payload_p, payload_len, pdata);
    } else {
        pr_notice("%s: Not support. \n", __func__);
        return 0;
    }
}

int disp_ext_sub_set_cmd2(void * payload_p, unsigned char payload_len, struct disp_ext_sub_pdata *pdata) {
    if (disp_ext_sub_func.pdisp_ext_sub_set_cmd2 != NULL) {
        pr_debug("%s \n", __func__);
        return disp_ext_sub_func.pdisp_ext_sub_set_cmd2(payload_p, payload_len, pdata);
    } else {
        pr_notice("%s: Not support. \n", __func__);
        return 0;
    }
}

int disp_ext_sub_panel_bus_init(struct disp_ext_sub_pdata *pdata) {
    if (disp_ext_sub_func.pdisp_ext_sub_panel_bus_init != NULL) {
        pr_debug("%s \n", __func__);
        return disp_ext_sub_func.pdisp_ext_sub_panel_bus_init(pdata);
    } else {
        pr_notice("%s: Not support. \n", __func__);
        return 0;
    }
}

int disp_ext_sub_panel_signal_init(struct disp_ext_sub_pdata *pdata) {
    if (disp_ext_sub_func.pdisp_ext_sub_panel_signal_init != NULL) {
        pr_debug("%s \n", __func__);
        return disp_ext_sub_func.pdisp_ext_sub_panel_signal_init(pdata);
    } else {
        pr_notice("%s: Not support. \n", __func__);
        return 0;
    }
}

int disp_ext_sub_panel_pinctrl_init(struct platform_device *pdev, struct disp_ext_sub_pdata *pdata) {
    if (disp_ext_sub_func.pdisp_ext_sub_panel_pinctrl_init != NULL) {
        pr_debug("%s \n", __func__);
        return disp_ext_sub_func.pdisp_ext_sub_panel_pinctrl_init(pdev, pdata);
    } else {
        pr_notice("%s: Not support. \n", __func__);
        return 0;
    }
}

int disp_ext_sub_get_panel_dt(struct device_node * np, struct disp_ext_sub_pdata *pdata) {
    if (disp_ext_sub_func.pdisp_ext_sub_get_panel_dt != NULL) {
        pr_debug("%s \n", __func__);
        return disp_ext_sub_func.pdisp_ext_sub_get_panel_dt(np, pdata);
    } else {
        pr_notice("%s: Not support. \n", __func__);
        return 0;
    }
}

int disp_ext_sub_get_signal_dt(struct device_node * np, struct disp_ext_sub_pdata *pdata) {
    if (disp_ext_sub_func.pdisp_ext_sub_get_signal_dt != NULL) {
        pr_debug("%s \n", __func__);
        return disp_ext_sub_func.pdisp_ext_sub_get_signal_dt(np, pdata);
    } else {
        pr_notice("%s: Not support. \n", __func__);
        return 0;
    }
}

int disp_ext_sub_get_seq_dt(struct device_node * np, struct disp_ext_sub_pdata *pdata) {
    if (disp_ext_sub_func.pdisp_ext_sub_get_seq_dt != NULL) {
        pr_debug("%s \n", __func__);
        return disp_ext_sub_func.pdisp_ext_sub_get_seq_dt(np, pdata);
    } else {
        pr_notice("%s: Not support. \n", __func__);
        return 0;
    }
}

int disp_ext_sub_get_spi_dt(struct device_node * np, struct device *dev, struct disp_ext_spi_data *sdata) {
    if (disp_ext_sub_func.pdisp_ext_sub_get_spi_dt != NULL) {
        pr_debug("%s \n", __func__);
        return disp_ext_sub_func.pdisp_ext_sub_get_spi_dt(np, dev, sdata);
    } else {
        pr_notice("%s: Not support. \n", __func__);
        return 0;
    }
}

int disp_ext_sub_subdispinfo_init(struct disp_ext_sub_pdata *pdata) {
    if (disp_ext_sub_func.pdisp_ext_sub_subdispinfo_init != NULL) {
        pr_debug("%s \n", __func__);
        return disp_ext_sub_func.pdisp_ext_sub_subdispinfo_init(pdata);
    } else {
        pr_notice("%s: Not support. \n", __func__);
        return 0;
    }
}

int disp_ext_sub_get_vreg_dt(struct device *dev, struct disp_ext_sub_pdata *pdata) {
    if (disp_ext_sub_func.pdisp_ext_sub_get_vreg_dt != NULL) {
        pr_debug("%s \n", __func__);
        return disp_ext_sub_func.pdisp_ext_sub_get_vreg_dt(dev, pdata);
    } else {
        pr_notice("%s: Not support. \n", __func__);
        return 0;
    }
}

int disp_ext_sub_set_subdispinfo(struct disp_ext_sub_pdata *pdata, struct fb_var_subdispinfo *psubdispinfo) {
    if (disp_ext_sub_func.pdisp_ext_sub_set_subdispinfo != NULL) {
        pr_debug("%s \n", __func__);
        return disp_ext_sub_func.pdisp_ext_sub_set_subdispinfo(pdata, psubdispinfo);
    } else {
        pr_notice("%s: Not support. \n", __func__);
        return 0;
    }
}

int disp_ext_sub_get_subdispinfo(struct disp_ext_sub_pdata *pdata) {
    if (disp_ext_sub_func.pdisp_ext_sub_get_subdispinfo != NULL) {
        pr_debug("%s \n", __func__);
        return disp_ext_sub_func.pdisp_ext_sub_get_subdispinfo(pdata);
    } else {
        pr_notice("%s: Not support. \n", __func__);
        return 0;
    }
}

int disp_ext_sub_set_data(void * payload_p, unsigned char payload_len, struct disp_ext_sub_pdata *pdata) {
    if (disp_ext_sub_func.pdisp_ext_sub_set_data != NULL) {
        pr_debug("%s \n", __func__);
        return disp_ext_sub_func.pdisp_ext_sub_set_data(payload_p, payload_len, pdata);
    } else {
        pr_notice("%s: Not support. \n", __func__);
        return 0;
    }
}

int disp_ext_sub_get_battery_temp(void) {
    if (disp_ext_sub_func.pdisp_ext_sub_get_battery_temp != NULL) {
        pr_debug("%s \n", __func__);
        return disp_ext_sub_func.pdisp_ext_sub_get_battery_temp();
    } else {
        pr_notice("%s: Not support. \n", __func__);
        return 0;
    }
}

int disp_ext_sub_panel_reset_cntrl(int onoff) {
    if (disp_ext_sub_func.pdisp_ext_sub_panel_reset_cntrl != NULL) {
        pr_debug("%s \n", __func__);
        return disp_ext_sub_func.pdisp_ext_sub_panel_reset_cntrl(onoff);
    } else {
        pr_notice("%s: Not support. \n", __func__);
        return 0;
    }
}

int disp_ext_sub_set_user_contrast(struct disp_ext_sub_pdata *pdata, int value, bool force_set) {
    if (disp_ext_sub_func.pdisp_ext_sub_set_user_contrast != NULL) {
        pr_debug("%s \n", __func__);
        return disp_ext_sub_func.pdisp_ext_sub_set_user_contrast(pdata, value, force_set);
    } else {
        pr_notice("%s: Not support. \n", __func__);
        return 0;
    }
}


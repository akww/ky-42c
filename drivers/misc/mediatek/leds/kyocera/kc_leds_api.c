/*
 * This software is contributed or developed by KYOCERA Corporation.
 * (C) 2022 KYOCERA Corporation
 */
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <linux/of.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/string.h>
#include <linux/ctype.h>
#include <linux/leds.h>
#include <linux/kc_leds_drv.h>

struct kc_leds_funcs kc_leds_func;

void kc_leds_funcs_register(struct kc_leds_funcs funcs)
{
	kc_leds_func = funcs;
}

void kc_leds_set_maxbrightness(int max_level, int enable)
{
    if(kc_leds_func.pset_maxbrightness != NULL){
        pr_debug("%s \n", __func__);
        kc_leds_func.pset_maxbrightness(max_level, enable);
    } else {
        pr_notice("%s: Not support. \n", __func__);
    }
}
EXPORT_SYMBOL(kc_leds_set_maxbrightness);

int32_t kc_wled_bled_connect_set(e_light_main_wled_disp disp_status, e_light_lcd_panel panel_class)
{
	int32_t ret = 0;

	if(kc_leds_func.plight_led_disp_set_panel != NULL){
        pr_debug("%s \n", __func__);
        ret = kc_leds_func.plight_led_disp_set_panel(disp_status, panel_class);
    } else {
        pr_notice("%s: Not support. \n", __func__);
    }
	return ret;
}
EXPORT_SYMBOL(kc_wled_bled_connect_set);

int32_t kc_wled_bled_en_set(e_light_main_wled_disp disp_status)
{
	int32_t ret = 0;

	if(kc_leds_func.plight_led_disp_power_set != NULL){
        pr_debug("%s \n", __func__);
        ret = kc_leds_func.plight_led_disp_power_set(disp_status);
    } else {
        pr_notice("%s: Not support. \n", __func__);
    }
	return ret;
}
EXPORT_SYMBOL(kc_wled_bled_en_set);

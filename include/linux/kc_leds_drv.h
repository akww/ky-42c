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
/*
 * This software is contributed or developed by KYOCERA Corporation.
 * (C) 2022 KYOCERA Corporation
 */

#ifndef _KC_LEDS_DRV_LINUX_H
#define _KC_LEDS_DRV_LINUX_H

#include <linux/leds.h>

typedef enum {
	LIGHT_MAIN_WLED_DIS     = 0x0000,
	LIGHT_MAIN_WLED_LCD_EN  = 0x0001,
	LIGHT_MAIN_WLED_LED_EN  = 0x0002,
	LIGHT_MAIN_WLED_LCD_PWREN  = 0x0004,
	LIGHT_MAIN_WLED_LCD_DIS = 0x0010,
	LIGHT_MAIN_WLED_LED_DIS = 0x0020,
	LIGHT_MAIN_WLED_LCD_PWRDIS = 0x0040,
	LIGHT_MAIN_WLED_EN      = (LIGHT_MAIN_WLED_LCD_EN|LIGHT_MAIN_WLED_LED_EN),
} e_light_main_wled_disp;

typedef enum {
	LIGHT_SUB_WLED_LCD_PWREN  = 0x0004,
	LIGHT_SUB_WLED_LCD_PWRDIS = 0x0040,
} e_light_sub_wled_disp;

typedef enum {
	LIGHT_LCD_PANEL0 = 0x0000,
	LIGHT_LCD_PANEL1 = 0x0001,
	LIGHT_LCD_PANEL2 = 0x0002,
	LIGHT_LCD_PANEL3 = 0x0003,
} e_light_lcd_panel;

enum led_select {
	LED_SEL_INVALID = 0,
	LED_SEL_KEYBL = 1,
	LED_SEL_GREEN = 2,
	LED_SEL_RED = 3,
	LED_SEL_AMBER = 4,
	LED_SEL_SUB_LCD = 5,
};
struct kc_leds_funcs {
	void (*pset_maxbrightness)(int max_level, int enable);
	int32_t (*plight_led_disp_set_panel)(e_light_main_wled_disp disp_status, e_light_lcd_panel panel_class);
	int32_t (*plight_led_disp_power_set)(e_light_main_wled_disp disp_status);
};

#define	LED_NAME_KEYBL			"button-backlight"
#define	LED_NAME_SUB_LCD		"subbacklightinfo"
#define	LED_NAME_RED			"red"
#define	LED_NAME_GREEN			"green"
#define	LED_NAME_AMBER			"amber"

//#define LED_COLOR_OFF		0
#define LED_COLOR_BLUE		1
#define LED_COLOR_GREEN		2
#define LED_COLOR_RED		4
#define LED_COLOR_AMBER		(LED_COLOR_GREEN | LED_COLOR_RED)

#define LED_BLINK_ON		1
#define LED_BLINK_OFF		0

#define LED_PRIORITY_BT_INCOMING	0
#define LED_PRIORITY_INCOMING		1
#define LED_PRIORITY_DATAROAMGUARD	2
#define LED_PRIORITY_NOTIFICATION	3
#define LED_PRIORITY_BATTERY		4
#define LED_PRIORITY_NUMBER			5	/* number of priorities */

#define LED_CTRL_MAX_SEQUENCE		10

struct led_control_ex_data {
	uint32_t	priority;
	uint32_t	color;
	uint32_t	mode;
	uint32_t	pattern[LED_CTRL_MAX_SEQUENCE];
};

extern void qpnp_set_leddata_ex(struct led_control_ex_data *);

extern void light_led_threecolor_led_set(int32_t color, int32_t blink);

extern void kc_leds_set_maxbrightness(int max_level, int enable);
extern int32_t kc_wled_bled_en_set(e_light_main_wled_disp disp_status);
extern int32_t kc_wled_bled_connect_set(e_light_main_wled_disp disp_status, e_light_lcd_panel panel_class);
extern void kc_rgb_led_info_get(int num , struct led_classdev *data);
extern void kc_wled_bled_en_set_bool(bool flag);

extern void mt6370_pmu_bled_en_set(bool flag);

extern void kc_leds_funcs_register(struct kc_leds_funcs funcs);
#endif

/*
 * This software is contributed or developed by KYOCERA Corporation.
 * (C) 2019 KYOCERA Corporation
 * (C) 2020 KYOCERA Corporation
 * (C) 2021 KYOCERA Corporation
 * (C) 2022 KYOCERA Corporation
 */ /*
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

#ifndef _KC_LEDS_DRV_H
#define _KC_LEDS_DRV_H

#include <linux/leds.h>
#include <mtk_leds_hal.h>

#include <linux/uaccess.h>
#include <linux/miscdevice.h>


/****************************************************************************
 * LED DRV functions
 ***************************************************************************/
struct kc_rgb_info{
	char* 	name;
	int		point;
	 struct led_classdev *dev_class;
};

struct kc_led_info {
	struct led_classdev cdev;
	struct mutex		request_lock;
	//struct work_struct 	work;
	struct miscdevice	mdev;
	uint32_t    		mode;
	uint32_t    		on_time;
	uint32_t    		off_time;
	uint32_t    		off_color;
	bool				blue_support;
	bool				red_first_control_flag;

	unsigned long		red_on_time;
	unsigned long		red_off_time;
	uint32_t			red_brightness;
	bool				red_hcur_en;

	unsigned long		green_on_time;
	unsigned long		green_off_time;
	uint32_t			green_brightness;

	unsigned long		blue_on_time;
	unsigned long		blue_off_time;
	uint32_t			blue_brightness;
	
	bool				mix_color_flag;
	
	unsigned long		RG_red_on_time;	
	unsigned long		RG_red_off_time;
	uint32_t			RG_red_brightness;
	unsigned long		RG_green_on_time;
	unsigned long		RG_green_off_time;
	uint32_t			RG_green_brightness;
	
	unsigned long		RB_red_on_time;
	unsigned long		RB_red_off_time;
	uint32_t			RB_red_brightness;	
	unsigned long		RB_blue_on_time;
	unsigned long		RB_blue_off_time;
	uint32_t			RB_blue_brightness;
	
	unsigned long		GB_green_on_time;
	unsigned long		GB_green_off_time;
	uint32_t			GB_green_brightness;
	unsigned long		GB_blue_on_time;
	unsigned long		GB_blue_off_time;
	uint32_t			GB_blue_brightness;
	
	unsigned long		RGB_red_on_time;
	unsigned long		RGB_red_off_time;
	uint32_t			RGB_red_brightness;
	unsigned long		RGB_green_on_time;
	unsigned long		RGB_green_off_time;
	uint32_t			RGB_green_brightness;
	unsigned long		RGB_blue_on_time;
	unsigned long		RGB_blue_off_time;
	uint32_t			RGB_blue_brightness;
	
};

#define LEDLIGHT_PARAM_NUM (5)
typedef struct _t_ledlight_ioctl {
uint32_t data[LEDLIGHT_PARAM_NUM];
} T_LEDLIGHT_IOCTL;

#define LEDLIGHT			'L'
#define LEDLIGHT_SET_BLINK		_IOW(LEDLIGHT, 0, T_LEDLIGHT_IOCTL)
#define LEDLIGHT_SET_PARAM      _IOW(LEDLIGHT, 1, T_LEDLIGHT_IOCTL)

#define TRICOLOR_RGB_COLOR_RED			0x00FF0000
#define TRICOLOR_RGB_COLOR_GREEN		0x0000FF00
#define TRICOLOR_RGB_COLOR_BLUE			0x000000FF

#define TRICOLOR_RGB_MAX_BRIGHT_VAL      (0xFFFFFFFFu)
#define TRICOLOR_RGB_GET_R(color)        (((color) & 0x00FF0000u) >> 16)
#define TRICOLOR_RGB_GET_G(color)        (((color) & 0x0000FF00u) >> 8 )
#define TRICOLOR_RGB_GET_B(color)        (((color) & 0x000000FFu) >> 0 )
#define TRICOLOR_RGB_MASK                (0x00FFFFFFu)
#define TRICOLOR_RGB_OFF                 (0x00000000u)

#define LIGHT_MAIN_WLED_ALLOW true
#define LIGHT_MAIN_WLED_DISALLOW false


extern void kc_rgb_led_info_get(int num , struct led_classdev *data);

extern void kc_wled_bled_en_set_bool(bool flag);

extern void get_timer(unsigned long *on,unsigned long *off);

extern void kc_leds_bl_mutex_lock(void);
extern void kc_leds_bl_mutex_unlock(void);
extern void kc_leds_set_bl_max_level(int max_level);
extern void kc_leds_store_bl_req_level(int level);
extern int kc_leds_show_bl_req_level(void);

#endif
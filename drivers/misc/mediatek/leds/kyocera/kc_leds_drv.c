/*
 * This software is contributed or developed by KYOCERA Corporation.
 * (C) 2019 KYOCERA Corporation
 * (C) 2020 KYOCERA Corporation
 * (C) 2021 KYOCERA Corporation
 * (C) 2022 KYOCERA Corporation
 *//*
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

#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/string.h>
#include <linux/ctype.h>
#include <linux/leds.h>
#include <linux/workqueue.h>
#include <linux/pm_wakeup.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <mtk_leds_hal.h>
#include <mtk_leds_drv.h>
#ifdef CONFIG_MTK_PWM
#include <mt-plat/mtk_pwm.h>
#endif
#ifdef CONFIG_MTK_AAL_SUPPORT
#include <ddp_aal.h>
#endif

#ifdef CONFIG_BACKLIGHT_SUPPORT_LP8557
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <asm-generic/gpio.h>
#endif

#include "../pmic/mt6370/inc/mt6370_pmu.h"
#include "../pmic/mt6370/inc/mt6370_pmu_bled.h"

//#include <asm/uaccess.h>
//#include <linux/miscdevice.h>
#include "kc_leds_drv.h"

#include <linux/kc_leds_drv.h>

#include <linux/fs.h>

#define TRICOLOR_DEBUG			1
#if TRICOLOR_DEBUG
#define TRICOLOR_DEBUG_LOG( msg, ... ) \
pr_debug("[TRICOLOR][%s][D](%d): " msg "\n", __func__, __LINE__, ## __VA_ARGS__)
#else
#define TRICOLOR_DEBUG_LOG( msg, ... ) \
pr_err("[TRICOLOR][%s][D](%d): " msg "\n", __func__, __LINE__, ## __VA_ARGS__)
#endif

#define TRICOLOR_NOTICE_LOG( msg, ... ) \
pr_notice("[TRICOLOR][%s][N](%d): " msg "\n", __func__, __LINE__, ## __VA_ARGS__)

#define TRICOLOR_ERR_LOG( msg, ... ) \
pr_err("[TRICOLOR][%s][E](%d): " msg "\n", __func__, __LINE__, ## __VA_ARGS__)

#define TRICOLOR_WARN_LOG( msg, ... ) \
pr_warn("[TRICOLOR][%s][W](%d): " msg "\n", __func__, __LINE__, ## __VA_ARGS__)


extern void mt6370_pmu_led_bright_set(struct led_classdev *led_cdev,
	enum led_brightness bright);

extern int mt6370_pmu_led_blink_set(struct led_classdev *led_cdev,
	unsigned long *delay_on, unsigned long *delay_off);
extern unsigned int kcGetMaxbrightness(void);
extern int setMaxbrightness(int max_level, int enable);
extern inline int mt6370_pmu_led_update_bits(struct led_classdev *led_cdev,
	uint8_t reg_addr, uint8_t reg_mask, uint8_t reg_data);
extern unsigned int mt_get_bl_brightness(void);

static struct kc_rgb_info *kc_rgb_data = NULL;
static struct kc_led_info *kc_leds_data;

struct mt6370_pmu_kc_data {
	struct mt6370_pmu_chip *chip;
};



void kc_rgb_led_info_get(int num , struct led_classdev *data)
{
	int i;

	TRICOLOR_DEBUG_LOG("%s num=%d\n",__func__, num);

	if(kc_rgb_data == NULL)
	{
		kc_rgb_data = kmalloc(3 * sizeof(struct kc_rgb_info), GFP_KERNEL);
		kc_rgb_data[0].name = "red";
		kc_rgb_data[0].point = -1;

		kc_rgb_data[1].name = "green";
		kc_rgb_data[1].point = -1;

		kc_rgb_data[2].name = "blue";
		kc_rgb_data[2].point = -1;
	}

	for(i = 0 ; i < 3 ; i++)
	{
		TRICOLOR_DEBUG_LOG("%s name=%s data->name=%s\n",
				__func__, kc_rgb_data[i].name, data->name);

		if(strcmp(kc_rgb_data[i].name , data->name) == 0)
		{
			kc_rgb_data[i].dev_class = data;
			kc_rgb_data[i].point = num;
		}
	}
}

/****************************************************************************
 * DEBUG MACROS
 ***************************************************************************/
 /*
static int debug_enable_led = 1;
#define LEDS_DRV_DEBUG(format, args...) do { \
	if (debug_enable_led) {	\
		pr_debug(format, ##args);\
	} \
} while (0)

*/


/*
void kc_leds_rgb_set_work(struct work_struct *work)
{
	return;
}
*/

static int kc_mt6370_pmu_read(uint8_t reg_addr)
{
	struct mt6370_pmu_kc_data *kc_pmu_data =
				dev_get_drvdata(kc_rgb_data[0].dev_class->dev->parent);

	return mt6370_pmu_reg_read(kc_pmu_data->chip, reg_addr);
}

static int kc_mt6370_pmu_write(uint8_t reg_addr , uint8_t mask , uint8_t value )
{
	struct mt6370_pmu_kc_data *kc_pmu_data =
				dev_get_drvdata(kc_rgb_data[0].dev_class->dev->parent);

	return mt6370_pmu_reg_update_bits(kc_pmu_data->chip, reg_addr  ,mask , value);
}




static int _kc_leds_rgb_set(int brightness)
{
	int red,green,blue;
	
	unsigned long red_on_time;
	unsigned long red_off_time;

	unsigned long green_on_time;
	unsigned long green_off_time;
	
	unsigned long blue_on_time;
	unsigned long blue_off_time;
	
	TRICOLOR_DEBUG_LOG("[KCLIGHT]%s + brightness=%x kc_rgb_data=%p\n",
				__func__, brightness, kc_rgb_data);

	if(kc_rgb_data == NULL || brightness < 0)
	{
		TRICOLOR_DEBUG_LOG("%s RGB INFO ERROR #1\n",__func__);
		return -1;
	}

	if(kc_rgb_data[0].point == -1)
	{
		TRICOLOR_DEBUG_LOG("%s RGB INFO ERROR #2\n",__func__);
		return -1;
	}

	red 	= (brightness & 0x00FF0000) >> 16;
	green 	= (brightness & 0x0000FF00) >> 8;
	blue 	= (brightness & 0x00000FF);
	
	if(kc_leds_data->blue_support == false && green == 0 && blue > 0)
	{
		TRICOLOR_DEBUG_LOG("%s blue_support not support\n",__func__);
		green =  blue;
		blue = 0;
	}
	
	if(kc_leds_data->on_time != 0 && kc_leds_data->off_time != 0)
	{
		red_on_time 	= kc_leds_data->on_time;
		red_off_time 	= kc_leds_data->off_time;
		green_on_time 	= kc_leds_data->on_time;
		green_off_time = kc_leds_data->off_time;
		blue_on_time 	= kc_leds_data->on_time;
		blue_off_time = kc_leds_data->off_time;
		
		if(red > 0)
			red = kc_leds_data->red_brightness;
		
		if(green >0)
			green = kc_leds_data->green_brightness;
		
		if(blue > 0)
			blue = kc_leds_data->blue_brightness;
	}
	else
	{
		red_on_time 	= kc_leds_data->red_on_time;
		red_off_time 	= kc_leds_data->red_off_time;
		green_on_time 	= kc_leds_data->green_on_time;
		green_off_time = kc_leds_data->green_off_time;
		blue_on_time 	= kc_leds_data->blue_on_time;
		blue_off_time = kc_leds_data->blue_off_time;
		
		if(red > 0)
			red = kc_leds_data->red_brightness;
	
		if(green >0)
			green = kc_leds_data->green_brightness;
		
		if(blue > 0)
			blue = kc_leds_data->blue_brightness;
		
		if(kc_leds_data -> mix_color_flag == true)
		{
			pr_err("[LEDS]%s color mix \n",__func__);
			if(red > 0 && green > 0 && blue > 0)
			{
				/* RGB*/
				red_on_time 	= kc_leds_data->RGB_red_on_time;
				red_off_time 	= kc_leds_data->RGB_red_off_time;
				green_on_time 	= kc_leds_data->RGB_green_on_time;
				green_off_time 	= kc_leds_data->RGB_green_off_time;
				blue_on_time 	= kc_leds_data->RGB_blue_on_time;
				blue_off_time 	= kc_leds_data->RGB_blue_off_time;
				
				red 	= kc_leds_data->RGB_red_brightness;
				green 	= kc_leds_data->RGB_green_brightness;
				blue 	= kc_leds_data->RGB_blue_brightness;
			}
			
			else if(red > 0 && green > 0 && blue == 0)
			{
				red_on_time 	= kc_leds_data->RG_red_on_time;
				red_off_time 	= kc_leds_data->RG_red_off_time;
				green_on_time 	= kc_leds_data->RG_green_on_time;
				green_off_time 	= kc_leds_data->RG_green_off_time;
				
				red 	= kc_leds_data->RG_red_brightness;
				green 	= kc_leds_data->RG_green_brightness;
			}
			else if(red > 0 && green == 0 && blue > 0)
			{
				red_on_time 	= kc_leds_data->RB_red_on_time;
				red_off_time 	= kc_leds_data->RB_red_off_time;
				blue_on_time 	= kc_leds_data->RB_blue_on_time;
				blue_off_time 	= kc_leds_data->RB_blue_off_time;
				
				red 	= kc_leds_data->RB_red_brightness;
				blue 	= kc_leds_data->RB_blue_brightness;
			}
			else if(red == 0 && green > 0 && blue > 0)
			{
				green_on_time 	= kc_leds_data->GB_green_on_time;
				green_off_time 	= kc_leds_data->GB_green_off_time;
				blue_on_time 	= kc_leds_data->GB_blue_on_time;
				blue_off_time 	= kc_leds_data->GB_blue_off_time;
				
				green	= kc_leds_data->GB_green_brightness;
				blue 	= kc_leds_data->GB_blue_brightness;
			}
		}
		else
			pr_err("[LEDS]%s non color mix \n",__func__);
		
	}
	
	if(blue > 0)
		blue = kc_leds_data->blue_brightness;


	TRICOLOR_DEBUG_LOG("%s color = %x red = %x green = %x blue = %x\n",__func__, brightness , red , green , blue );

	if(kc_leds_data->blue_support == false && green == 0 && blue > 0)
	{
		TRICOLOR_DEBUG_LOG("%s blue_support not support\n",__func__);
		green =  kc_leds_data->green_brightness;
		blue = 0;
	}

	TRICOLOR_DEBUG_LOG("%s #2 color = %x red = %x green = %x blue = %x\n",__func__, brightness , red , green , blue );
	

	kc_rgb_data[0].dev_class->brightness = red;
	kc_rgb_data[1].dev_class->brightness = green;
	
	mt6370_pmu_led_blink_set(kc_rgb_data[0].dev_class,&red_on_time,&red_off_time);	
	mt6370_pmu_led_blink_set(kc_rgb_data[1].dev_class,&green_on_time,&green_off_time);


	pr_err("[LEDS]%s red led\n",__func__);
	mt6370_pmu_led_bright_set(kc_rgb_data[0].dev_class , kc_rgb_data[0].dev_class->brightness );
	pr_err("[LEDS]%s green led\n",__func__);
	mt6370_pmu_led_bright_set(kc_rgb_data[1].dev_class , kc_rgb_data[1].dev_class->brightness );

	pr_err("[LEDS]%s mt6370_pmu_led_bright_set end\n",__func__);

	if(kc_leds_data->red_first_control_flag == true)
	{
		kc_mt6370_pmu_write(0x92,0x80,0x80);

		if(kc_leds_data->red_hcur_en)
			kc_mt6370_pmu_write(0x93,0x80,0x80);

		kc_leds_data->red_first_control_flag = false;
	}

	if(kc_leds_data->blue_support == true && kc_rgb_data[2].point != -1)
	{
		kc_rgb_data[2].dev_class->brightness = blue;
		
		mt6370_pmu_led_blink_set(kc_rgb_data[2].dev_class,&blue_on_time,&blue_off_time);
		
		mt6370_pmu_led_bright_set(kc_rgb_data[2].dev_class , kc_rgb_data[2].dev_class->brightness );
	}
	return 0;
}


static void kc_leds_rgb_set(struct led_classdev *led_cdev,
			   enum led_brightness level)
{
	TRICOLOR_DEBUG_LOG("%s level = %x\n",__func__, level );

	_kc_leds_rgb_set(level);
}



static ssize_t led_rgb_store(struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t count)
{
	u32 color_info;
	int ret;

	pr_err("[LEDS]%s\n",__func__);

	ret = kstrtou32(buf, 16, &color_info);

	if (ret)
		return ret;

	_kc_leds_rgb_set(color_info);

	return count;
}

static int sysfs_value_set_ulong(const char *buf , size_t count , unsigned long *value)
{
	u32 color_info;
	int ret;

	ret = kstrtou32(buf, 10, &color_info);

	if (ret)
		return ret;

	*value = (unsigned long)color_info;

	return count;
}

static int sysfs_value_set_u32(const char *buf , size_t count , u32 *value)
{
	u32 color_info;
	int ret;

	ret = kstrtou32(buf, 10, &color_info);

	if (ret)
		return ret;

	*value = color_info;

	return count;
}

static ssize_t red_on_store(struct device *dev,	struct device_attribute *attr,const char *buf, size_t count)
{
	return sysfs_value_set_ulong(buf , count , &kc_leds_data->red_on_time);
}
static ssize_t red_off_store(struct device *dev,	struct device_attribute *attr,const char *buf, size_t count)
{
	return sysfs_value_set_ulong(buf , count , &kc_leds_data->red_off_time);
}

static ssize_t green_on_store(struct device *dev,	struct device_attribute *attr,const char *buf, size_t count)
{
	return sysfs_value_set_ulong(buf , count , &kc_leds_data->green_on_time);
}
static ssize_t green_off_store(struct device *dev,	struct device_attribute *attr,const char *buf, size_t count)
{
	return sysfs_value_set_ulong(buf , count , &kc_leds_data->green_off_time);
}
static ssize_t blue_on_store(struct device *dev,	struct device_attribute *attr,const char *buf, size_t count)
{
	return sysfs_value_set_ulong(buf , count , &kc_leds_data->blue_on_time);
}
static ssize_t blue_off_store(struct device *dev,	struct device_attribute *attr,const char *buf, size_t count)
{
	return sysfs_value_set_ulong(buf , count , &kc_leds_data->blue_off_time);
}


static ssize_t red_brightness_store(struct device *dev,	struct device_attribute *attr,const char *buf, size_t count)
{
	return sysfs_value_set_u32(buf , count , &kc_leds_data->red_brightness);
}

static ssize_t green_brightness_store(struct device *dev,	struct device_attribute *attr,const char *buf, size_t count)
{
	return sysfs_value_set_u32(buf , count , &kc_leds_data->green_brightness);
}
static ssize_t blue_brightness_store(struct device *dev,	struct device_attribute *attr,const char *buf, size_t count)
{
	return sysfs_value_set_u32(buf , count , &kc_leds_data->blue_brightness);
}

static ssize_t red_on_show(struct device *dev, struct device_attribute *dev_attr, char * buf)
{
    return sprintf(buf, "%ld\n", kc_leds_data->red_on_time);
}
static ssize_t red_off_show(struct device *dev, struct device_attribute *dev_attr, char * buf)
{
    return sprintf(buf, "%ld\n", kc_leds_data->red_off_time);
}
static ssize_t green_on_show(struct device *dev, struct device_attribute *dev_attr, char * buf)
{
    return sprintf(buf, "%ld\n", kc_leds_data->green_on_time);
}
static ssize_t green_off_show(struct device *dev, struct device_attribute *dev_attr, char * buf)
{
    return sprintf(buf, "%ld\n", kc_leds_data->green_off_time);
}
static ssize_t blue_on_show(struct device *dev, struct device_attribute *dev_attr, char * buf)
{
    return sprintf(buf, "%ld\n", kc_leds_data->blue_on_time);
}
static ssize_t blue_off_show(struct device *dev, struct device_attribute *dev_attr, char * buf)
{
    return sprintf(buf, "%ld\n", kc_leds_data->blue_off_time);
}


static ssize_t red_brightness_show(struct device *dev, struct device_attribute *dev_attr, char * buf)
{
    return sprintf(buf, "%d\n", kc_leds_data->red_brightness);
}

static ssize_t green_brightness_show(struct device *dev, struct device_attribute *dev_attr, char * buf)
{
    return sprintf(buf, "%d\n", kc_leds_data->green_brightness);
}

static ssize_t blue_brightness_show(struct device *dev, struct device_attribute *dev_attr, char * buf)
{
    return sprintf(buf, "%d\n", kc_leds_data->blue_brightness);
}

uint8_t sysfs_reg_read_addr = 0x00;

static ssize_t reg_write_store(struct device *dev,	struct device_attribute *attr,const char *buf, size_t count)
{
	uint8_t addr , data;
	u32 value;
	int ret;

	ret = kstrtou32(buf, 16, &value);

	if (ret)
		return ret;

	pr_err("[LEDS]%s value = 0x%04x\n",__func__,value);

	addr =  ((value >> 8) & 0xFF);
	data = (value & 0xFF);

	kc_mt6370_pmu_write(addr,0xFF,data);

	return count;
}

static ssize_t reg_read_store(struct device *dev,	struct device_attribute *attr,const char *buf, size_t count)
{
	u32 addr;
	int ret;

	ret = kstrtou32(buf, 16, &addr);

	if (ret)
		return ret;

	sysfs_reg_read_addr = (uint8_t)addr;

	return count;
}

static ssize_t reg_read_show(struct device *dev, struct device_attribute *dev_attr, char * buf)
{
	int read_value = kc_mt6370_pmu_read(sysfs_reg_read_addr);
	return sprintf(buf, "Addr:0x%02x = 0x%02x\n",sysfs_reg_read_addr , read_value);
}

uint8_t sysfs_reg_dump_start = 0x80;
uint8_t sysfs_reg_dump_end = 0xA9;

static ssize_t reg_dump_store(struct device *dev,	struct device_attribute *attr,const char *buf, size_t count)
{
	u32 value;
	int ret;

	ret = kstrtou32(buf, 16, &value);

	if (ret)
		return ret;

	sysfs_reg_dump_start =  ((value >> 8) & 0xFF);
	sysfs_reg_dump_end = (value & 0xFF);

	return count;
}

static ssize_t reg_dump_show(struct device *dev, struct device_attribute *dev_attr, char * buf)
{
	int i = 0;
	int count = 0;
	int value;
	char temp_buf[100];

	count += sprintf(buf, "reg_dump 0x%02x - 0x%02x\n",sysfs_reg_dump_start,sysfs_reg_dump_end);


	for(i = sysfs_reg_dump_start ; i <= sysfs_reg_dump_end ; i++ )
	{
		value  = kc_mt6370_pmu_read(i);

		count += sprintf(temp_buf, "addr 0x%02x = 0x%02x\n", i , value);
		strcat( buf , temp_buf );
	}

	return count;
}

static DEVICE_ATTR(rgb				, 0664, NULL			, led_rgb_store);
static DEVICE_ATTR(red_on_time		, 0664, red_on_show		, red_on_store);
static DEVICE_ATTR(red_off_time		, 0664, red_off_show	, red_off_store);
static DEVICE_ATTR(green_on_time	, 0664, green_on_show	, green_on_store);
static DEVICE_ATTR(green_off_time	, 0664, green_off_show	, green_off_store);
static DEVICE_ATTR(blue_on_time		, 0664, blue_on_show	, blue_on_store);
static DEVICE_ATTR(blue_off_time	, 0664, blue_off_show	, blue_off_store);


static DEVICE_ATTR(red_brightness	, 0664, red_brightness_show		, red_brightness_store);
static DEVICE_ATTR(green_brightness	, 0664, green_brightness_show	, green_brightness_store);
static DEVICE_ATTR(blue_brightness	, 0664, blue_brightness_show	, blue_brightness_store);

static DEVICE_ATTR(reg_write, 0664, NULL	, reg_write_store);
static DEVICE_ATTR(reg_read	, 0664, reg_read_show	, reg_read_store);
static DEVICE_ATTR(reg_dump	, 0664, reg_dump_show	, reg_dump_store);

static struct attribute *led_rgb_attrs[] = {
	&dev_attr_rgb.attr,
	&dev_attr_red_on_time.attr,
	&dev_attr_red_off_time.attr,
	&dev_attr_green_on_time.attr,
	&dev_attr_green_off_time.attr,
	&dev_attr_blue_on_time.attr,
	&dev_attr_blue_off_time.attr,

	&dev_attr_red_brightness.attr,
	&dev_attr_green_brightness.attr,
	&dev_attr_blue_brightness.attr,


	&dev_attr_reg_write.attr,
	&dev_attr_reg_read.attr,
	&dev_attr_reg_dump.attr,
	NULL
};

static int pwm_set(const char *buf,int mode)
{
	u32 time;
	int ret;

	unsigned long on_timer;
	unsigned long off_timer;

	ret = kstrtou32(buf, 10, &time);

	if (ret)
		return ret;

	if(mode == 0)
		kc_leds_data->on_time = time;
	else
		kc_leds_data->off_time = time;


	on_timer = kc_leds_data->on_time;
	off_timer = kc_leds_data->off_time;

	if(on_timer == 0 || off_timer ==  0)
	{
		on_timer = 500;
		off_timer = 0;
	}

	mt6370_pmu_led_blink_set(kc_rgb_data[0].dev_class,&on_timer,&off_timer);
	mt6370_pmu_led_blink_set(kc_rgb_data[1].dev_class,&on_timer,&off_timer);

	if(kc_leds_data->blue_support == true && kc_rgb_data[2].point != -1)
		mt6370_pmu_led_blink_set(kc_rgb_data[2].dev_class,&on_timer,&off_timer);

	return 0;
}

static ssize_t pwm_lo_store(struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t count)
{
	pwm_set(buf,0);

	return count;
}

static ssize_t pwm_hi_store(struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t count)
{
	pwm_set(buf,1);

	return count;
}

static DEVICE_ATTR(pwm_lo, 0664, NULL, pwm_lo_store);
static DEVICE_ATTR(pwm_hi, 0664, NULL, pwm_hi_store);

static struct attribute *pwm_attrs[] = {
	&dev_attr_pwm_lo.attr,
	&dev_attr_pwm_hi.attr,
	NULL
};



static const struct attribute_group led_rgb_attr_group = {
	.attrs = led_rgb_attrs,
};

static const struct attribute_group pwm_attr_group = {
	.attrs = pwm_attrs,
};



static long kclights_leds_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	int32_t ret = -1;
	T_LEDLIGHT_IOCTL		st_ioctl;

    TRICOLOR_DEBUG_LOG("%s(): [IN]",__func__);

    switch (cmd) {
    case LEDLIGHT_SET_PARAM:
        TRICOLOR_DEBUG_LOG("%s(): LEDLIGHT_SET_PARAM",__func__);

        ret = copy_from_user(&st_ioctl,
                    argp,
                    sizeof(T_LEDLIGHT_IOCTL));
        if (ret) {
            TRICOLOR_DEBUG_LOG("%s(): Error leds_ioctl(cmd = LEDLIGHT_SET_PARAM)", __func__);
            return -EFAULT;
        }

        if ((st_ioctl.data[1]!=0 && st_ioctl.data[2]==0) ||
            (st_ioctl.data[1]==0 && st_ioctl.data[2]!=0)) {
            TRICOLOR_DEBUG_LOG("%s(): Error on/off time parameters invalid", __func__);
            return -EFAULT;
        }

		mutex_lock(&kc_leds_data->request_lock);
        TRICOLOR_DEBUG_LOG("%s(): mutex_lock", __func__);

		kc_leds_data->mode      = st_ioctl.data[0];
		kc_leds_data->on_time   = st_ioctl.data[1];
		kc_leds_data->off_time  = st_ioctl.data[2];
		kc_leds_data->off_color = st_ioctl.data[3];

		TRICOLOR_DEBUG_LOG("prep mode=[%d] on_time=[%d] off_time=[%d] off_color=[0x%08x]",
			kc_leds_data->mode, kc_leds_data->on_time, kc_leds_data->off_time, kc_leds_data->off_color);

        TRICOLOR_DEBUG_LOG("%s(): mutex_unlock", __func__);
		mutex_unlock(&kc_leds_data->request_lock);
        break;
    default:
        TRICOLOR_DEBUG_LOG("%s(): default", __func__);
        return -ENOTTY;
    }

    TRICOLOR_DEBUG_LOG("%s(): [OUT]",__func__);

    return 0;
}



static struct file_operations kclights_leds_fops = {
    .owner        = THIS_MODULE,
	.open           = simple_open,
    .unlocked_ioctl = kclights_leds_ioctl,
    .compat_ioctl   = kclights_leds_ioctl,
};

////////////////////////////////////////////////////////////////////////////////

static int kc_leds_probe(struct platform_device *pdev)
{
	int rc;
	u32 read_value;

	kc_leds_data = kzalloc(sizeof(struct kc_led_info), GFP_KERNEL);

	if(!kc_leds_data)
		return -ENOMEM;

	mutex_init(&kc_leds_data->request_lock);

	dev_set_drvdata(&pdev->dev, kc_leds_data);

	kc_leds_data->cdev.name = "ledinfo";

	kc_leds_data->cdev.brightness_set = kc_leds_rgb_set;
	kc_leds_data->cdev.max_brightness = 0x00FFFFFF;

	kc_leds_data->on_time = 0;
	kc_leds_data->off_time = 0;
	kc_leds_data->red_first_control_flag = true;

	kc_leds_data->mdev.minor = MISC_DYNAMIC_MINOR;
	kc_leds_data->mdev.name = "leds-ledlight";
	kc_leds_data->mdev.fops = &kclights_leds_fops;
	rc =  misc_register(&kc_leds_data->mdev);

	/*Red LED*/
	if(of_property_read_u32(pdev->dev.of_node, "red_on_time", &read_value) != 0)
		kc_leds_data->red_on_time = 1;
	else
		kc_leds_data->red_on_time = read_value;
	if(of_property_read_u32(pdev->dev.of_node, "red_off_time", &read_value) != 0)
		kc_leds_data->red_off_time = 1;
	else
		kc_leds_data->red_off_time = read_value;

	if(of_property_read_u32(pdev->dev.of_node, "red_brightness", &kc_leds_data->red_brightness) != 0)
		kc_leds_data->red_brightness = 1;

	if(kc_leds_data->red_brightness  == 0)
		kc_leds_data->red_brightness = 0x10;

	kc_leds_data->red_hcur_en = of_property_read_bool(pdev->dev.of_node, "red_hcur_en");

	/*Green LED*/
	if(of_property_read_u32(pdev->dev.of_node, "green_on_time", &read_value) != 0)
		kc_leds_data->green_on_time = 1;
	else
		kc_leds_data->green_on_time = read_value;

	if(of_property_read_u32(pdev->dev.of_node, "green_off_time", &read_value) != 0)
		kc_leds_data->green_off_time = 1;
	else
		kc_leds_data->green_off_time = read_value;

	if(of_property_read_u32(pdev->dev.of_node, "green_brightness", &kc_leds_data->green_brightness) != 0)
		kc_leds_data->green_brightness = 1;

	if(kc_leds_data->green_brightness  == 0)
		kc_leds_data->green_brightness = 0x10;

	/*Blue LED*/

	if(of_property_read_u32(pdev->dev.of_node, "blue_on_time", &read_value) != 0)
		kc_leds_data->blue_on_time = 1;
	else
		kc_leds_data->blue_on_time = read_value;

	if(of_property_read_u32(pdev->dev.of_node, "blue_off_time", &read_value) != 0)
		kc_leds_data->blue_off_time = 1;
	else
		kc_leds_data->blue_off_time = read_value;

	if(of_property_read_u32(pdev->dev.of_node, "blue_brightness", &kc_leds_data->blue_brightness) != 0)
		kc_leds_data->blue_brightness = 1;

	if(kc_leds_data->blue_brightness  == 0)
		kc_leds_data->blue_brightness = 0x10;

	kc_leds_data->blue_support = of_property_read_bool(pdev->dev.of_node, "blue_support");

	if(kc_leds_data->blue_support == false)
		TRICOLOR_DEBUG_LOG("%s BLUE NOT SUPPORT\n",__func__);
	
	kc_leds_data->mix_color_flag = of_property_read_bool(pdev->dev.of_node, "mix_color");

	if(kc_leds_data->mix_color_flag == true)
		TRICOLOR_DEBUG_LOG("%s COLOR MIX SUPPORT\n",__func__);

	
	/* RED + GREEN*/
	if(of_property_read_u32(pdev->dev.of_node, "RG_red_on_time", &read_value) != 0)
		kc_leds_data->RG_red_on_time = 1;
	else
		kc_leds_data->RG_red_on_time = read_value;
	
	if(of_property_read_u32(pdev->dev.of_node, "RG_red_off_time", &read_value) != 0)
		kc_leds_data->RG_red_off_time = 1;
	else
		kc_leds_data->RG_red_off_time = read_value;

	if(of_property_read_u32(pdev->dev.of_node, "RG_red_brightness", &kc_leds_data->RG_red_brightness) != 0)
		kc_leds_data->RG_red_brightness = 1;
	
	if(kc_leds_data->RG_red_brightness  == 0)
		kc_leds_data->RG_red_brightness = 0x10;
	
	if(of_property_read_u32(pdev->dev.of_node, "RG_green_on_time", &read_value) != 0)
		kc_leds_data->RG_green_on_time = 1;
	else
		kc_leds_data->RG_green_on_time = read_value;
	
	if(of_property_read_u32(pdev->dev.of_node, "RG_green_off_time", &read_value) != 0)
		kc_leds_data->RG_green_off_time = 1;
	else
		kc_leds_data->RG_green_off_time = read_value;
	
	if(of_property_read_u32(pdev->dev.of_node, "RG_green_brightness", &kc_leds_data->RG_green_brightness) != 0)
		kc_leds_data->RG_green_brightness = 1;
	
	if(kc_leds_data->RG_green_brightness  == 0)
		kc_leds_data->RG_green_brightness = 0x10;
	
	/* RED + BLUE*/
	if(of_property_read_u32(pdev->dev.of_node, "RB_red_on_time", &read_value) != 0)
		kc_leds_data->RB_red_on_time = 1;
	else
		kc_leds_data->RB_red_on_time = read_value;
	
	if(of_property_read_u32(pdev->dev.of_node, "RB_red_off_time", &read_value) != 0)
		kc_leds_data->RB_red_off_time = 1;
	else
		kc_leds_data->RB_red_off_time = read_value;

	if(of_property_read_u32(pdev->dev.of_node, "RB_red_brightness", &kc_leds_data->RB_red_brightness) != 0)
		kc_leds_data->RB_red_brightness = 1;
	
	if(kc_leds_data->RB_red_brightness  == 0)
		kc_leds_data->RB_red_brightness = 0x10;	
	
	if(of_property_read_u32(pdev->dev.of_node, "RB_blue_on_time", &read_value) != 0)
		kc_leds_data->RB_blue_on_time = 1;
	else
		kc_leds_data->RB_blue_on_time = read_value;
	
	if(of_property_read_u32(pdev->dev.of_node, "RB_blue_off_time", &read_value) != 0)
		kc_leds_data->RB_blue_off_time = 1;
	else
		kc_leds_data->RB_blue_off_time = read_value;
	
	if(of_property_read_u32(pdev->dev.of_node, "blue_brightness", &kc_leds_data->RB_blue_brightness) != 0)
		kc_leds_data->RB_blue_brightness = 1;
	
	if(kc_leds_data->RB_blue_brightness  == 0)
		kc_leds_data->RB_blue_brightness = 0x10;
	
	
	/* GREEN + BLUE*/
	if(of_property_read_u32(pdev->dev.of_node, "GB_green_on_time", &read_value) != 0)
		kc_leds_data->GB_green_on_time = 1;
	else
		kc_leds_data->GB_green_on_time = read_value;
	
	if(of_property_read_u32(pdev->dev.of_node, "GB_green_off_time", &read_value) != 0)
		kc_leds_data->GB_green_off_time = 1;
	else
		kc_leds_data->GB_green_off_time = read_value;
	
	if(of_property_read_u32(pdev->dev.of_node, "GB_green_brightness", &kc_leds_data->GB_green_brightness) != 0)
		kc_leds_data->GB_green_brightness = 1;
	
	if(kc_leds_data->GB_green_brightness  == 0)
		kc_leds_data->GB_green_brightness = 0x10;
		
	if(of_property_read_u32(pdev->dev.of_node, "GB_blue_on_time", &read_value) != 0)
		kc_leds_data->GB_blue_on_time = 1;
	else
		kc_leds_data->GB_blue_on_time = read_value;
	
	if(of_property_read_u32(pdev->dev.of_node, "GB_blue_off_time", &read_value) != 0)
		kc_leds_data->GB_blue_off_time = 1;
	else
		kc_leds_data->GB_blue_off_time = read_value;
	
	if(of_property_read_u32(pdev->dev.of_node, "GB_blue_brightness", &kc_leds_data->GB_blue_brightness) != 0)
		kc_leds_data->GB_blue_brightness = 1;
	
	if(kc_leds_data->GB_blue_brightness  == 0)
		kc_leds_data->GB_blue_brightness = 0x10;
	
	/* RED + GREEN + BLUE*/
	if(of_property_read_u32(pdev->dev.of_node, "RGB_red_on_time", &read_value) != 0)
		kc_leds_data->RGB_red_on_time = 1;
	else
		kc_leds_data->RGB_red_on_time = read_value;
	
	if(of_property_read_u32(pdev->dev.of_node, "RGB_red_off_time", &read_value) != 0)
		kc_leds_data->RGB_red_off_time = 1;
	else
		kc_leds_data->RGB_red_off_time = read_value;

	if(of_property_read_u32(pdev->dev.of_node, "RGB_red_brightness", &kc_leds_data->RGB_red_brightness) != 0)
		kc_leds_data->RGB_red_brightness = 1;
	
	if(kc_leds_data->RGB_red_brightness  == 0)
		kc_leds_data->RGB_red_brightness = 0x10;
	
	if(of_property_read_u32(pdev->dev.of_node, "RGB_green_on_time", &read_value) != 0)
		kc_leds_data->RGB_green_on_time = 1;
	else
		kc_leds_data->RGB_green_on_time = read_value;
	
	if(of_property_read_u32(pdev->dev.of_node, "RGB_green_off_time", &read_value) != 0)
		kc_leds_data->RGB_green_off_time = 1;
	else
		kc_leds_data->RGB_green_off_time = read_value;
	
	if(of_property_read_u32(pdev->dev.of_node, "RGB_green_brightness", &kc_leds_data->RGB_green_brightness) != 0)
		kc_leds_data->RGB_green_brightness = 1;
	
	if(kc_leds_data->RGB_green_brightness  == 0)
		kc_leds_data->RGB_green_brightness = 0x10;
	
	if(of_property_read_u32(pdev->dev.of_node, "RGB_blue_on_time", &read_value) != 0)
		kc_leds_data->RGB_blue_on_time = 1;
	else
		kc_leds_data->RGB_blue_on_time = read_value;
	
	if(of_property_read_u32(pdev->dev.of_node, "RGB_blue_off_time", &read_value) != 0)
		kc_leds_data->RGB_blue_off_time = 1;
	else
		kc_leds_data->RGB_blue_off_time = read_value;
	
	if(of_property_read_u32(pdev->dev.of_node, "RGB_blue_brightness", &kc_leds_data->RGB_blue_brightness) != 0)
		kc_leds_data->RGB_blue_brightness = 1;
	
	if(kc_leds_data->RGB_blue_brightness  == 0)
		kc_leds_data->RGB_blue_brightness = 0x10;	
	
	
	
	rc = led_classdev_register(&pdev->dev, &kc_leds_data->cdev);

	if(rc > 0)
		goto probe_error;

	rc = 0;
	rc += sysfs_create_group(&kc_leds_data->cdev.dev->kobj,&led_rgb_attr_group);
	rc += sysfs_create_group(&kc_leds_data->cdev.dev->kobj,&pwm_attr_group);
//	rc += sysfs_create_group(&kc_leds_data->cdev.dev->kobj,&backlight_attr_group);

	if(rc > 0)
		goto probe_error;

	return 0;

probe_error:
	led_classdev_unregister(&kc_leds_data->cdev);
	return rc;

}


static int kc_leds_remove(struct platform_device *pdev)
{
	if(kc_rgb_data != NULL)
		kfree(kc_rgb_data);

	
	led_classdev_unregister(&kc_leds_data->cdev);
	
	sysfs_remove_group(&kc_leds_data->cdev.dev->kobj,	&led_rgb_attr_group);
	sysfs_remove_group(&kc_leds_data->cdev.dev->kobj,	&pwm_attr_group);
//	sysfs_remove_group(&kc_leds_data->cdev.dev->kobj,	&backlight_attr_group);
	
	return 0;
}

/*

static void kc_leds_shutdown(struct platform_device *pdev)
{
	return;
}

*/

static const struct of_device_id kc_led_of_match[] = {
	{ .compatible = "kc,kc_leds", },
	{}
};

MODULE_DEVICE_TABLE(of, kc_led_of_match);

static struct platform_driver kc_leds_driver = {
	.driver = {
		   	.name = "kc_leds",
		   	.owner = THIS_MODULE,
			.of_match_table = kc_led_of_match,
		   },
	.probe = kc_leds_probe,
	.remove = kc_leds_remove,
//	.shutdown = kc_leds_shutdown,
};

static int __init kc_leds_init(void)
{
	int ret;

	TRICOLOR_DEBUG_LOG("%s\n",__func__);

	ret = platform_driver_register(&kc_leds_driver);

	return ret;
}

static void __exit kc_leds_exit(void)
{
	platform_driver_unregister(&kc_leds_driver);
}


late_initcall(kc_leds_init);
module_exit(kc_leds_exit);

MODULE_AUTHOR("Kyocera Inc.");
MODULE_DESCRIPTION("LED driver for MediaTek MT65xx chip");
MODULE_LICENSE("GPL");
MODULE_ALIAS("kc-leds-mt65xx");

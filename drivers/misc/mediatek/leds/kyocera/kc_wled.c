/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */
/*
 * This software is contributed or developed by KYOCERA Corporation.
 * (C) 2022 KYOCERA Corporation
 */

#define pr_fmt(fmt)	"WLED %s: " fmt, __func__

#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/leds.h>
#include <linux/kc_leds_drv.h>
#include <uapi/linux/kclight_uapi.h>
//#include <mach/kc_board.h>
#include "kc_wled.h"
//#ifdef CONFIG_DISCARD_BL_CONTROL
#include <linux/mutex.h>
//#endif

#define DRV_NAME "kc_wled"
//#define BACKLIGHT_INFO "backlightinfo"

#define BACKLIGHT_INFO "lcd-backlight"

struct kc_wled_data {
	struct led_classdev	st_cdev;
	struct work_struct	work;
	struct delayed_work	monitor_dwork;
	int					lcd_bl_drv_en_gpio;
};

typedef void (*kc_wled_func_type)(int level, enum kc_wled_table tbl, int gpio);

static kc_wled_func_type kc_wled_func = cat4004b_work;

static enum kc_wled_table kc_wled_tbl = WLED_TBL_NORM;

//#ifdef CONFIG_DISCARD_BL_CONTROL
static struct kc_wled_data *the_data;
static DEFINE_MUTEX(kc_wled_lock);
static atomic_t lcd_status = ATOMIC_INIT(1);
//#endif

int32_t light_led_disp_set(e_light_main_wled_disp disp_status);
//static int32_t light_led_disp_set(e_light_main_wled_disp disp_status);
//static bool light_led_disp_enabled(void);

extern void cat4004b_init(void);
extern unsigned long wled_stat_aboot;

static void cat4004b_set_kc_maxbrightness(int max_level, int enable);
static int32_t cat4004b_light_led_disp_set_panel(e_light_main_wled_disp disp_status, e_light_lcd_panel panel_class);
static int32_t cat4004b_light_led_disp_power_set(e_light_main_wled_disp disp_status);

static void cat4004b_set_kc_maxbrightness(int max_level, int enable)
{
//	struct kc_wled_data *data;
	enum led_brightness value;

	pr_notice("[KCLIGHT] %s: max_level=%d enable=%d", __func__, max_level, enable);

//	if (!kclight_data) {
//		KCLIGHT_E_LOG("kclight_data null");
//		return;
//	}

	mutex_lock(&kc_wled_lock);

	if (enable == 1) {
		the_data->st_cdev.kc_max_brightness = max_level;
	} else {
		the_data->st_cdev.kc_max_brightness = WLED_BRIGHTNESS_MAX;
	}

	mutex_unlock(&kc_wled_lock);

	value = the_data->st_cdev.usr_brightness_req;
	pr_info("[KCLIGHT] %s: brightness value:%d max:%d\n", __func__, value, the_data->st_cdev.kc_max_brightness);

	if (value > the_data->st_cdev.kc_max_brightness) {
		pr_err("[KCLIGHT] invalid value:%d max:%d\n", value, the_data->st_cdev.kc_max_brightness);
		value = the_data->st_cdev.kc_max_brightness;
	}

	the_data->st_cdev.brightness = value;

	(*kc_wled_func)(
		the_data->st_cdev.brightness,
		kc_wled_tbl,
		the_data->lcd_bl_drv_en_gpio);

	return;
}

struct kc_leds_funcs cat4004b_kc_leds_funcs = {
	.pset_maxbrightness = cat4004b_set_kc_maxbrightness,
	.plight_led_disp_set_panel = cat4004b_light_led_disp_set_panel,
	.plight_led_disp_power_set = cat4004b_light_led_disp_power_set
};

static ssize_t wled_kc_max_brightness_store(struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t count)
{
	unsigned long state;
	ssize_t ret;

	pr_info("[KCLIGHT] %s: kc_max_brightness=%s\n", __func__, buf);

//	sscanf(buf, "%d %d %d %d", &led_type, &onoff, &on_time, &off_time);
	ret = kstrtoul(buf, 10, &state);
	if (ret) {
		pr_err("[KCLIGHT] %s: kc_max_brightness err\n", __func__);
		return count;
	}

	pr_info("[KCLIGHT] %s: kc_max_brightness=%ld\n", __func__, state);
	cat4004b_set_kc_maxbrightness(state, 1);

	return count;
}

static ssize_t wled_kc_max_brightness_show(struct device *dev, struct device_attribute *dev_attr, char * buf)
{
	return sprintf(buf, "%d\n", the_data->st_cdev.kc_max_brightness);
}

static DEVICE_ATTR(kc_max_brightness, 0664, wled_kc_max_brightness_show, wled_kc_max_brightness_store);

static struct attribute *wled_attrs[] = {
	&dev_attr_kc_max_brightness.attr,
	NULL
};

static const struct attribute_group wled_attr_group = {
	.attrs = wled_attrs,
};

static void kc_wled_brightness_set(struct led_classdev *pst_cdev, enum led_brightness value)
{
	struct kc_wled_data *data = container_of(pst_cdev, struct kc_wled_data, st_cdev);

	pr_info("[KCLIGHT] %s name:%s value:0x%08x\n", __func__, pst_cdev->name, value);
	pr_info("[KCLIGHT] %s set lcd_status:%d\n", __func__, atomic_read(&lcd_status));
	
//	if(!light_led_disp_enabled()){
//		pr_err("discard light_led_disp_enabled()");
//		return;
//	}

	data->st_cdev.usr_brightness_req = value;
	
	if (value > data->st_cdev.kc_max_brightness) {
		pr_err("invalid value:%d max:%d\n", value, data->st_cdev.kc_max_brightness);
		value = data->st_cdev.kc_max_brightness;
	}

	data->st_cdev.brightness = value;
    the_data->st_cdev.brightness  = value;
	schedule_work(&data->work);

	pr_info("[KCLIGHT] name:%s value:0x%08x\n", pst_cdev->name, value);
	return;
}

static enum led_brightness kc_wled_brightness_get(struct led_classdev *pst_cdev)
{
	struct kc_wled_data *data = container_of(pst_cdev, struct kc_wled_data, st_cdev);
	int32_t lret = data->st_cdev.brightness;

	pr_err("[Disp]%s ret:0x%02x\n", __func__, lret);
	pr_err("ret:0x%02x\n", lret);
	return lret;
}

static void kc_wled_work_handler(struct work_struct *work)
{
	struct kc_wled_data *data;

	pr_err("[Disp]%s \n", __func__);
	
	mutex_lock(&kc_wled_lock);

	data = container_of(work, struct kc_wled_data, work);

//#ifdef CONFIG_DISCARD_BL_CONTROL
	if ((data->st_cdev.brightness != WLED_BRIGHTNESS_OFF) &&
		(!atomic_read(&lcd_status)))
		pr_info("lcd is off, discard backlight brightness:%d\n", data->st_cdev.brightness);
	else
//#endif
	(*kc_wled_func)(
		data->st_cdev.brightness,
		kc_wled_tbl,
		data->lcd_bl_drv_en_gpio);

	mutex_unlock(&kc_wled_lock);

	return;
}

static int kc_wled_probe(struct platform_device *pdev)
{
	int rc = 0;
	struct kc_wled_data *data;

	pr_info("[Disp]%s \n", __func__);
	if (!pdev->dev.of_node) {
		pr_err("No platform supplied from device tree.\n");
		rc = -EINVAL;
		goto exit;
	}

	data = devm_kzalloc(&pdev->dev, sizeof(struct kc_wled_data), GFP_KERNEL);
	if (!data) {
		pr_err("kzalloc fail\n");
		rc = -ENOMEM;
		goto exit;
	}

//#ifdef CONFIG_DISCARD_BL_CONTROL
	the_data = data;
//#endif

	INIT_WORK(&data->work, kc_wled_work_handler);

	pr_info("[Disp]%s INIT_WORK_end\n", __func__);
	
	platform_set_drvdata(pdev, data);
	data->st_cdev.max_brightness = WLED_BRIGHTNESS_MAX;
	data->st_cdev.brightness = wled_stat_aboot;
	data->st_cdev.usr_brightness_req = wled_stat_aboot;
	data->st_cdev.kc_max_brightness = WLED_BRIGHTNESS_MAX;
	data->st_cdev.brightness_set = kc_wled_brightness_set;
	data->st_cdev.brightness_get = kc_wled_brightness_get;
	data->st_cdev.name = BACKLIGHT_INFO;

	rc = led_classdev_register(&pdev->dev, &data->st_cdev);
	if (rc) {
		pr_err("unable to register led %s\n", data->st_cdev.name);
		goto error;
	}

	data->lcd_bl_drv_en_gpio = of_get_named_gpio(pdev->dev.of_node, "kc,lcd-bl-drv-en-gpio", 0);
	if (!gpio_is_valid(data->lcd_bl_drv_en_gpio)) {
		pr_err("of_get_named_gpio failed.\n");
		rc = -EINVAL;
		goto error;
	}

	rc = gpio_request(data->lcd_bl_drv_en_gpio, DRV_NAME);
	if (rc) {
		pr_err("gpio_request failed.\n");
		goto error;
	}

	rc = sysfs_create_group(&data->st_cdev.dev->kobj, &wled_attr_group);
	if (rc) {
		pr_err("unable to register sysfs wled.\n");
		goto error;
	}

	pr_info("initialization is completed!! GPIO(%d)=%d\n",
		data->lcd_bl_drv_en_gpio, gpio_get_value(data->lcd_bl_drv_en_gpio));
	
	cat4004b_init();

	kc_leds_funcs_register(cat4004b_kc_leds_funcs);

	return 0;

error:
	led_classdev_unregister(&data->st_cdev);
	cancel_work_sync(&data->work);
	devm_kfree(&pdev->dev, data);

exit:
	pr_err("failed.\n");

	return rc;
}

static int kc_wled_remove(struct platform_device *pdev)
{
	struct kc_wled_data *data = platform_get_drvdata(pdev);

	pr_err("[Disp]%s \n", __func__);
	led_classdev_unregister(&data->st_cdev);
	cancel_work_sync(&data->work);
	devm_kfree(&pdev->dev, data);

	return 0;
}

static const struct of_device_id kc_wled_of_match[] = {
	{ .compatible = DRV_NAME, },
};

static struct platform_driver kc_wled_driver = {
	.driver = {
		.name = DRV_NAME,
		.owner = THIS_MODULE,
		.of_match_table = kc_wled_of_match,
	},
	.probe = kc_wled_probe,
	.remove = kc_wled_remove,
};

int __init kc_wled_init(void)
{
	
	pr_err("[Disp]%s \n", __func__);
	return platform_driver_register(&kc_wled_driver);
}

static void __exit kc_wled_exit(void)
{
	
	pr_err("[Disp]%s \n", __func__);
	
	platform_driver_unregister(&kc_wled_driver);
}

static int32_t cat4004b_light_led_disp_set_panel(e_light_main_wled_disp disp_status, e_light_lcd_panel panel_class)
{
	pr_err("[IN] panel_class=0x%x", panel_class);
	switch( panel_class ){
	case LIGHT_LCD_PANEL0:
		pr_err("panel class = LIGHT_LCD_PANEL0");
		break;
	default:
		pr_err("unknown panel class");
		break;
	}

	return light_led_disp_set(disp_status);
}

static atomic_t g_display_detect = ATOMIC_INIT(0);

int32_t light_led_disp_set(e_light_main_wled_disp disp_status)
{
	pr_err("DISABLE_DISP_DETECT");
	if(disp_status == LIGHT_MAIN_WLED_LCD_EN)
	{
		atomic_set(&g_display_detect, 1);
		pr_err("%s set backlight accept\n",__func__);
	}
	return 0;
}
EXPORT_SYMBOL(light_led_disp_set);

//#ifdef CONFIG_DISCARD_BL_CONTROL
static int32_t cat4004b_light_led_disp_power_set(e_light_main_wled_disp disp_status)
{
	int32_t ret = 0;

	pr_err("[Disp]%s disp_status%d\n", __func__,disp_status);
	mutex_lock(&kc_wled_lock);
	
	if (disp_status == LIGHT_MAIN_WLED_LCD_PWREN || disp_status == LIGHT_MAIN_WLED_LCD_EN) {
		pr_err("[Disp]%s atomic_set = 1\n", __func__);
//		atomic_set(&g_display_detect, 1);
		if (the_data->st_cdev.brightness != WLED_BRIGHTNESS_OFF) {
			(*kc_wled_func)(
				the_data->st_cdev.brightness,
				kc_wled_tbl,
				the_data->lcd_bl_drv_en_gpio);
		}
	} else {
		pr_err("[Disp]%s atomic_set = -1\n", __func__);
//		atomic_set(&g_display_detect, -1);
//		if (the_data->st_cdev.brightness != WLED_BRIGHTNESS_OFF) {
			(*kc_wled_func)(
				WLED_BRIGHTNESS_OFF,
				kc_wled_tbl,
				the_data->lcd_bl_drv_en_gpio);
//		}
	}
	pr_err("[Disp]%s set lcd_status:%d brightness:%d\n", __func__, atomic_read(&lcd_status), the_data->st_cdev.brightness);
	//pr_err("set lcd_status:%d brightness:%d\n", atomic_read(&lcd_status), the_data->st_cdev.brightness);
	mutex_unlock(&kc_wled_lock);
	return ret;
}
//#endif

void kc_wled_off(void)
{
	
	pr_err("[Disp]%s \n", __func__);
	mutex_lock(&kc_wled_lock);
	(*kc_wled_func)(
		WLED_BRIGHTNESS_OFF,
		WLED_TBL_NORM,
		the_data->lcd_bl_drv_en_gpio);
	mutex_unlock(&kc_wled_lock);
}
EXPORT_SYMBOL(kc_wled_off);

module_init(kc_wled_init);
module_exit(kc_wled_exit);

MODULE_AUTHOR("KYOCERA Corporation");
MODULE_DESCRIPTION("KC WLED Driver");
MODULE_LICENSE("GPL");

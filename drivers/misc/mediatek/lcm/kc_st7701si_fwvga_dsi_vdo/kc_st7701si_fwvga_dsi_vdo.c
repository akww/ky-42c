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

#define LOG_TAG "LCM"

#include <linux/string.h>
#include <linux/kernel.h>

#include "lcm_drv.h"

#include <linux/string.h>
#include <linux/wait.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/pinctrl/consumer.h>
#include <linux/of_gpio.h>
#include "disp_dts_gpio.h"
#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/regulator/consumer.h>
#include <linux/clk.h>
#endif
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/miscdevice.h>
#include <linux/reboot.h>

#include <linux/kc_leds_drv.h>

#define LCM_LOGI(fmt, args...)  pr_notice("[KERNEL/"LOG_TAG"]"fmt, ##args)
#define LCM_LOGD(fmt, args...)  pr_debug("[KERNEL/"LOG_TAG"]"fmt, ##args)
static struct LCM_UTIL_FUNCS lcm_util;

#define MDELAY(n)		(lcm_util.mdelay(n))
#define UDELAY(n)		(lcm_util.udelay(n))

#define dsi_set_cmdq_V3(para_tbl,size,force_update) \
	lcm_util.dsi_set_cmdq_V3(para_tbl,size,force_update)
#define dsi_set_cmdq_V22(cmdq, cmd, count, ppara, force_update) \
	lcm_util.dsi_set_cmdq_V22(cmdq, cmd, count, ppara, force_update)
#define dsi_set_cmdq_V2(cmd, count, ppara, force_update) \
	lcm_util.dsi_set_cmdq_V2(cmd, count, ppara, force_update)
#define dsi_set_cmdq(pdata, queue_size, force_update) \
		lcm_util.dsi_set_cmdq(pdata, queue_size, force_update)
#define wrtie_cmd(cmd) lcm_util.dsi_write_cmd(cmd)
#define write_regs(addr, pdata, byte_nums) \
		lcm_util.dsi_write_regs(addr, pdata, byte_nums)
#define read_reg(cmd) \
	  lcm_util.dsi_dcs_read_lcm_reg(cmd)
#define read_reg_v2(cmd, buffer, buffer_size) \
		lcm_util.dsi_dcs_read_lcm_reg_v2(cmd, buffer, buffer_size)

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/i2c.h>
#include <linux/irq.h>
#include <linux/jiffies.h>
#include <linux/delay.h> 
#include <linux/uaccess.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/platform_device.h>

#define LCM_DSI_CMD_MODE	0
#define FRAME_WIDTH		(480)
#define FRAME_HEIGHT	(854)
#define LCM_DENSITY		(240)

#define LCM_PHYSICAL_WIDTH	(42000)
#define LCM_PHYSICAL_HEIGHT	(74000)

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

static int kdisp_debug = 0;
#define KCDISP_LOG_SEQ(fmt, ...)						\
	do {												\
		if (kdisp_debug)								\
			pr_err(fmt, ##__VA_ARGS__);					\
	} while (0)

static struct LCM_setting_table_V3 lcm_suspend_setting[] = {
    {0x05, 0x28, 0, {}},
    {REGFLAG_ESCAPE_ID, REGFLAG_DELAY_MS_V3, 20, {}},
    {0x05, 0x10, 0, {}},
    {REGFLAG_ESCAPE_ID, REGFLAG_DELAY_MS_V3, 120, {}},
};

static struct LCM_setting_table_V3 init_setting[] = {
	{0x29, 0xFF, 5,  {0x77, 0x01, 0x00, 0x00, 0x13} },
	{0x13, 0xEF, 1,  {0x08} },
	{0x29, 0xFF, 5,  {0x77, 0x01, 0x00, 0x00, 0x10} },
	{0x29, 0xC0, 2,  {0xE9, 0x03} },
	{0x29, 0xC1, 2,  {0x10, 0x0C} },
	{0x29, 0xC2, 2,  {0x07, 0x0A} },
	{0x13, 0xCC, 1,  {0x10} },
	{0x29, 0xB0, 16, {0x00, 0x07, 0x8E, 0x0C, 0x0F, 0x05, 0x01, 0x07, 0x07, 0x20, 0x05, 0x53, 0x11, 0x6A, 0x31, 0x1F} },
	{0x29, 0xB1, 16, {0x00, 0x08, 0x8F, 0x0D, 0x10, 0x06, 0x01, 0x08, 0x07, 0x21, 0x05, 0x53, 0x11, 0x6A, 0x32, 0x1F} },
	{0x29, 0xFF, 5,  {0x77, 0x01, 0x00, 0x00, 0x11} },
	{0x13, 0xB0, 1,  {0x75} },
	{0x13, 0xB1, 1,  {0x65} },
	{0x13, 0xB2, 1,  {0x81} },
	{0x13, 0xB3, 1,  {0x80} },
	{0x13, 0xB5, 1,  {0x4E} },
	{0x13, 0xB7, 1,  {0x85} },
	{0x13, 0xB8, 1,  {0x33} },
	{0x13, 0xC1, 1,  {0x78} },
	{0x13, 0xC2, 1,  {0x78} },
	{0x13, 0xD0, 1,  {0x88} },
	{0x29, 0xE0, 3,  {0x00, 0x00, 0x02} },
	{0x29, 0xE1, 11, {0x06, 0xA0, 0x08, 0xA0, 0x05, 0xA0, 0x07, 0xA0, 0x0E, 0x44, 0x44} },
	{0x29, 0xE2, 13, {0x30, 0x30, 0x44, 0x44, 0x6E, 0xA0, 0x00, 0x00, 0x6E, 0xA0, 0x00, 0x00, 0xEE} },
	{0x29, 0xE3, 4,  {0x00, 0x00, 0x33, 0x33} },
	{0x29, 0xE4, 2,  {0x44, 0x44} },
	{0x29, 0xE5, 16, {0x0D, 0x69, 0x0A, 0xA0, 0x0F, 0x6B, 0x0A, 0xA0, 0x09, 0x65, 0x0A, 0xA0, 0x0B, 0x67, 0x0A, 0xA0} },
	{0x29, 0xE6, 4,  {0x00, 0x00, 0x33, 0x33} },
	{0x29, 0xE7, 2,  {0x44, 0x44} },
	{0x29, 0xE8, 16, {0x0C, 0x68, 0x0A, 0xA0, 0x0E, 0x6A, 0x0A, 0xA0, 0x08, 0x64, 0x0A, 0xA0, 0x0A, 0x66, 0x0A, 0xA0} },
	{0x29, 0xE9, 2,  {0x36, 0x00} },
	{0x29, 0xEB, 7,  {0x00, 0x01, 0xE4, 0xE4, 0x44, 0x88, 0x40} },
	{0x29, 0xED, 16, {0xFF, 0x45, 0x67, 0xFA, 0x01, 0x2B, 0xCF, 0xFF, 0xFF, 0xFC, 0xB2, 0x10, 0xAF, 0x76, 0x54, 0xFF} },
	{0x29, 0xEF, 6,  {0x10, 0x0D, 0x04, 0x08, 0x3F, 0x1F} },
	{0x05, 0x11, 0, {} },
	{REGFLAG_ESCAPE_ID, REGFLAG_DELAY_MS_V3, 120, {}},
	{0x05, 0x29, 0, {} },
	{0x15, 0x35, 1, {0x00} },
};

#define KCDISP_DETECT_TIMEOUT			500
#define PANEL_NO_CHECK					0
#define PANEL_NOT_FOUND					-1
#define PANEL_FOUND						1
#define PANEL_DETECT_CHECK_RETRY_NUM	5

struct kcdisp_data {
	struct class			*class;
	struct device			*device;
	dev_t					dev_num;
	struct					cdev cdev;
	struct miscdevice		miscdev;
	int						test_mode;
	int						sre_mode;
	int						refresh_disable;
	int						panel_detect_status;	// 0:no check/ 1:detect/ -1:not detect
	int						panel_detection_1st;
	struct notifier_block		reboot_notifier;
};

static struct kcdisp_data kcdisp_data;
static int kc_lcm_get_panel_detect(void);

extern void kdisp_mtkfb_reboot_shutdown(void);

///////////////////////////////////////////////////////////////

static void kdisp_drv_init(void)
{
	;
}

#define CLASS_NAME "kcdisp"
#define DRIVER_NAME "kcdisp"
static ssize_t kcdisp_test_mode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", kdisp_debug);
}

static ssize_t kcdisp_test_mode_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int rc = 0;

	rc = kstrtoint(buf, 10, &kdisp_debug);
	if (rc) {
		pr_err("kstrtoint failed. rc=%d\n", rc);
		return rc;
	}

	pr_notice("[KCLCM]test_mode=%d\n", kdisp_debug);

	return count;
}

static ssize_t kcdisp_sre_mode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct kcdisp_data *kcdisp = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", kcdisp->sre_mode);
}

static ssize_t kcdisp_sre_mode_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct kcdisp_data *kcdisp = dev_get_drvdata(dev);
	int rc = 0;

	rc = kstrtoint(buf, 10, &kcdisp->sre_mode);
	if (rc) {
		pr_err("kstrtoint failed. rc=%d\n", rc);
		return rc;
	}

	pr_notice("[KCLCM]sre_mode=%d\n", kcdisp->sre_mode);

	return count;
}

static ssize_t kcdisp_disp_connect_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct kcdisp_data *kcdisp = dev_get_drvdata(dev);
	int panel_detect_status;

	if (kcdisp->panel_detect_status == PANEL_FOUND) {
		panel_detect_status = 0;
	} else {
		panel_detect_status = 0x3A;
	}

	return snprintf(buf, PAGE_SIZE, "0x%x\n", panel_detect_status);
}

static ssize_t kcdisp_disp_connect_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct kcdisp_data *kcdisp = dev_get_drvdata(dev);
	int rc = 0;

	rc = kstrtoint(buf, 10, &kcdisp->panel_detect_status);
	if (rc) {
		pr_err("kstrtoint failed. rc=%d\n", rc);
		return rc;
	}

	pr_notice("[KCLCM]panel_detect_status=%d\n", kcdisp->panel_detect_status);

	return count;
}

static ssize_t kcdisp_refresh_disable_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct kcdisp_data *kcdisp = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", kcdisp->refresh_disable);
}

static ssize_t kcdisp_refresh_disable_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct kcdisp_data *kcdisp = dev_get_drvdata(dev);
	int rc = 0;

	if (kc_lcm_get_panel_detect() == PANEL_NOT_FOUND) {
		pr_err("[KCDISP]%s skip for panel not detect\n",__func__);
		return 0;
	}

	rc = kstrtoint(buf, 10, &kcdisp->refresh_disable);
	if (rc) {
		pr_err("kstrtoint failed. rc=%d\n", rc);
		return rc;
	}

	pr_notice("[KCLCM]refresh_disable=%d\n", kcdisp->refresh_disable);

	return count;
}

static DEVICE_ATTR(test_mode, S_IRUGO | S_IWUSR,
	kcdisp_test_mode_show, kcdisp_test_mode_store);
static DEVICE_ATTR(sre_mode, S_IRUGO | S_IWUSR,
	kcdisp_sre_mode_show, kcdisp_sre_mode_store);
static DEVICE_ATTR(disp_connect, S_IRUGO | S_IWUSR,
	kcdisp_disp_connect_show, kcdisp_disp_connect_store);
static DEVICE_ATTR(refresh_disable, S_IRUGO | S_IWUSR,
	kcdisp_refresh_disable_show, kcdisp_refresh_disable_store);

static struct attribute *kcdisp_attrs[] = {
	&dev_attr_test_mode.attr,
	&dev_attr_sre_mode.attr,
	&dev_attr_disp_connect.attr,
	&dev_attr_refresh_disable.attr,
	NULL,
};

static struct attribute_group kcdisp_attr_group = {
	.attrs = kcdisp_attrs,
};

#if 0
static int kcdisp_open(struct inode *inode, struct file *filp)
{
	(void)inode;
	(void)filp;
	return 0;
}

static int kcdisp_release(struct inode *inode, struct file *filp)
{
	(void)inode;
	(void)filp;
	return 0;
}

static long kcdisp_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	(void)filp;
	(void)cmd;
	(void)arg;

	return 0;
}
#endif

static const struct file_operations kcdisp_fops = {
	.owner = THIS_MODULE,
#if 0
	.open           = kcdisp_open,
	.release        = kcdisp_release,
	.unlocked_ioctl = kcdisp_ioctl,
	.compat_ioctl	= kcdisp_ioctl,
#endif
};

static int kc_lcm_init_sysfs(struct kcdisp_data *kcdisp)
{
	int ret = 0;

	ret = alloc_chrdev_region(&kcdisp->dev_num, 0, 1, DRIVER_NAME);
	if (ret  < 0) {
		pr_err("alloc_chrdev_region failed ret = %d\n", ret);
		goto done;
	}

	kcdisp->class = class_create(THIS_MODULE, CLASS_NAME);
	if (IS_ERR(kcdisp->class)) {
		pr_err("%s class_create error!\n",__func__);
		goto done;
	}

	kcdisp->device = device_create(kcdisp->class, NULL,
		kcdisp->dev_num, NULL, DRIVER_NAME);
	if (IS_ERR(kcdisp->device)) {
		ret = PTR_ERR(kcdisp->device);
		pr_err("device_create failed %d\n", ret);
		goto done;
	}

	dev_set_drvdata(kcdisp->device, kcdisp);

	cdev_init(&kcdisp->cdev, &kcdisp_fops);
	ret = cdev_add(&kcdisp->cdev,
			MKDEV(MAJOR(kcdisp->dev_num), 0), 1);
	if (ret < 0) {
		pr_err("cdev_add failed %d\n", ret);
		goto done;
	}

	ret = sysfs_create_group(&kcdisp->device->kobj,
			&kcdisp_attr_group);
	if (ret)
		pr_err("unable to register rotator sysfs nodes\n");

done:
	return ret;
}

static int kcdisp_reboot_notify(struct notifier_block *nb,
				unsigned long code, void *unused)
{
	pr_notice("[KCDISP]%s +\n", __func__);
	
	if (kc_lcm_get_panel_detect() == PANEL_NOT_FOUND) {
		pr_err("[KCDISP]%s skip for panel not detect\n",__func__);
		return NOTIFY_DONE;
	}
	
	kdisp_mtkfb_reboot_shutdown();
	
	pr_notice("[KCDISP]%s -\n", __func__);

	return NOTIFY_DONE;
}

static int kc_lcm_platform_probe(struct platform_device *pdev)
{
	struct kcdisp_data *kcdisp;
	int error;

	pr_notice("[KCDISP]%s\n", __func__);

	kcdisp = &kcdisp_data;

	memset((char *)kcdisp, 0, sizeof(struct kcdisp_data));
	kcdisp->test_mode           = 0;
	kcdisp->sre_mode            = 0;
	kcdisp->refresh_disable     = 0;
	kcdisp->panel_detect_status = PANEL_NO_CHECK;
	kcdisp->panel_detection_1st = 0;

	kc_lcm_init_sysfs(kcdisp);

	kdisp_drv_init();
	kcdisp->reboot_notifier.notifier_call = kcdisp_reboot_notify;
	error = register_reboot_notifier(&kcdisp->reboot_notifier);
	if (error) {
		dev_err(&pdev->dev, "[KCDISP] %s: failed to register reboot notifier: %d\n",
			__func__, error);
	}

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id lcm_of_ids[] = {
	{.compatible = "kc,kc_lcm",},
	{}
};
MODULE_DEVICE_TABLE(of, lcm_of_ids);
#endif

static struct platform_driver lcm_driver = {
	.probe = kc_lcm_platform_probe,
	.driver = {
		   .name = "kc_lcm",
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = lcm_of_ids,
#endif
		   },
};

static int __init kc_lcm_init_st7701si(void)
{
	int ret = 0;

	pr_notice("[KCDISP]%s: panel_detect_status=%d\n",
			__func__, kcdisp_data.panel_detect_status);

	if (kcdisp_data.panel_detect_status == PANEL_FOUND) {
		pr_notice("[KCDISP]LCM: Register lcm driver\n");
		if (platform_driver_register(&lcm_driver)) {
			pr_err("[KCDISP]LCM: failed to register disp driver\n");
			ret = -ENODEV;
		}
    }

	return ret;
}

static void __exit kc_lcm_exit_st7701si(void)
{
	platform_driver_unregister(&lcm_driver);
	pr_notice("[KCLCM]LCM: Unregister lcm driver done\n");
}

late_initcall(kc_lcm_init_st7701si);
module_exit(kc_lcm_exit_st7701si);
MODULE_AUTHOR("Kyocera");
MODULE_DESCRIPTION("KC Display subsystem Driver");
MODULE_LICENSE("GPL");

///////////////////////////////////////////////////////////////

static int kc_lcm_get_panel_detect(void)
{
	if (!kcdisp_data.panel_detection_1st) {
		if (kcdisp_data.panel_detect_status == PANEL_NOT_FOUND) {
			kcdisp_data.refresh_disable = 1;
			kc_wled_bled_connect_set(LIGHT_MAIN_WLED_LCD_EN, 0);
			kc_wled_bled_en_set(LIGHT_MAIN_WLED_LCD_PWRDIS);
			kc_wled_bled_connect_set(LIGHT_MAIN_WLED_LCD_DIS, 0);
		} else {
			kc_wled_bled_connect_set(LIGHT_MAIN_WLED_LCD_EN, 0);
		}

		kcdisp_data.panel_detection_1st = 1;
	}

	return kcdisp_data.panel_detect_status;
}

static unsigned int lcm_check_refresh_disable(void)
{
	return kcdisp_data.refresh_disable;
}

static void lcm_notify_panel_detect(int isDSIConnected)
{
	if (isDSIConnected == 0) {
		kcdisp_data.panel_detect_status = PANEL_NOT_FOUND;
	} else {
		kcdisp_data.panel_detect_status = PANEL_FOUND;
	}

	pr_notice("[KCDISP][st7701si]%s panel detect:%d\n",__func__,
					kcdisp_data.panel_detect_status);
}

static void lcm_set_util_funcs(const struct LCM_UTIL_FUNCS *util)
{
	memcpy(&lcm_util, util, sizeof(struct LCM_UTIL_FUNCS));
}


static void lcm_get_params(struct LCM_PARAMS *params)
{
	pr_notice("[KCDISP][st7701si]%s\n", __func__);

	memset(params, 0, sizeof(struct LCM_PARAMS));

	params->type = LCM_TYPE_DSI;

	params->width = FRAME_WIDTH;
	params->height = FRAME_HEIGHT;
	params->physical_width = LCM_PHYSICAL_WIDTH/1000;
	params->physical_height = LCM_PHYSICAL_HEIGHT/1000;
	params->physical_width_um = LCM_PHYSICAL_WIDTH;
	params->physical_height_um = LCM_PHYSICAL_HEIGHT;
	params->density = LCM_DENSITY;


	params->dsi.mode = SYNC_EVENT_VDO_MODE;
	params->dsi.switch_mode = CMD_MODE;
	lcm_dsi_mode = SYNC_EVENT_VDO_MODE;

	LCM_LOGI("[KCDISP][st7701si]lcm_get_params lcm_dsi_mode %d\n", lcm_dsi_mode);
	params->dsi.switch_mode_enable = 0;

	/* DSI */
	/* Command mode setting */
	params->dsi.LANE_NUM = LCM_TWO_LANE;
	/* The following defined the fomat for data coming from LCD engine. */
	params->dsi.data_format.color_order = LCM_COLOR_ORDER_RGB;
	params->dsi.data_format.trans_seq = LCM_DSI_TRANS_SEQ_MSB_FIRST;
	params->dsi.data_format.padding = LCM_DSI_PADDING_ON_LSB;
	params->dsi.data_format.format = LCM_DSI_FORMAT_RGB888;

	/* Highly depends on LCD driver capability. */
	params->dsi.packet_size = 256;
	/* video mode timing */

	params->dsi.PS = LCM_PACKED_PS_24BIT_RGB888;

	params->dsi.vertical_sync_active = 2;
	params->dsi.vertical_backporch = 20;
	params->dsi.vertical_frontporch = 20;
	params->dsi.vertical_active_line = FRAME_HEIGHT;

	params->dsi.horizontal_sync_active = 5;
	params->dsi.horizontal_backporch = 66;
	params->dsi.horizontal_frontporch = 52;
	params->dsi.horizontal_active_pixel = FRAME_WIDTH;
	params->dsi.ssc_disable = 1;
	params->dsi.ssc_range = 5;
	params->dsi.PLL_CLOCK = 200;
	params->dsi.PLL_CK_VDO = 200;
	params->dsi.CLK_HS_POST = 36;
	params->dsi.clk_lp_per_line_enable = 0;
	params->dsi.esd_check_enable = 1;
	params->dsi.customization_esd_check_enable = 1;
	params->dsi.lcm_esd_check_table[0].cmd = 0x0A;
	params->dsi.lcm_esd_check_table[0].count = 1;
	params->dsi.lcm_esd_check_table[0].para_list[0] = 0x9C;
    params->dsi.lcm_esd_check_table[1].cmd = 0x05;
    params->dsi.lcm_esd_check_table[1].count = 1;
    params->dsi.lcm_esd_check_table[1].para_list[0] = 0x00;
}

static void lcm_init_power(void)
{
	pr_notice("[KCDISP][st7701si]lcm_init_power +\n");
	pr_notice("[KCDISP][st7701si]lcm_init_power -\n");
}

static void lcm_pre_off(void)
{
	pr_notice("[KCDISP][st7701si]lcm_pre_off\n");

	if (kc_lcm_get_panel_detect() == PANEL_NOT_FOUND) {
		pr_err("[KCDISP][st7701si]%s skip for panel not detect\n",__func__);
		return;
	}

	kc_wled_bled_en_set(LIGHT_MAIN_WLED_LCD_PWRDIS);
}

static void lcm_post_off(void)
{
	if (kc_lcm_get_panel_detect() == PANEL_NOT_FOUND) {
		pr_err("[KCDISP][st7701si]%s skip for panel not detect\n",__func__);
		return;
	}

	pr_notice("[KCDISP][st7701si]lcm_post_off\n");

	KCDISP_LOG_SEQ("[KCDISP][st7701si]%s: 1.8v off\n", __func__);
	disp_dts_gpio_select_state(DTS_GPIO_STATE_LCD_VMIPI_OUT0);
	usleep_range(1*1000, 2*1000);

	KCDISP_LOG_SEQ("[KCDISP][st7701si]%s: 2.8v off\n", __func__);
	disp_dts_gpio_select_state(DTS_GPIO_STATE_LCD_BIAS_OUT0);
	usleep_range(10*1000, 11*1000);
}

static void lcm_pre_on(void)
{
	pr_notice("[KCDISP][st7701si]lcm_pre_on\n");

	if (kc_lcm_get_panel_detect() == PANEL_NOT_FOUND) {
		pr_err("[KCDISP][st7701si]%s skip for panel not detect\n",__func__);
		return;
	}

	KCDISP_LOG_SEQ("[KCDISP][st7701si]%s: 1.8v ON\n", __func__);
	disp_dts_gpio_select_state(DTS_GPIO_STATE_LCD_BIAS_OUT1);

	KCDISP_LOG_SEQ("[KCDISP][st7701si]%s: 2.8v ON\n", __func__);
	disp_dts_gpio_select_state(DTS_GPIO_STATE_LCD_VMIPI_OUT1);
	usleep_range(2*1000, 3*1000);
}

static void lcm_post_on(void)
{
	pr_notice("[KCDISP][st7701si]lcm_post_on\n");

	if (kc_lcm_get_panel_detect() == PANEL_NOT_FOUND) {
		pr_err("[KCDISP][st7701si]%s skip for panel not detect\n",__func__);
		return;
	}

	kc_wled_bled_en_set(LIGHT_MAIN_WLED_LCD_PWREN);
}

static void lcm_init(void)
{
   LCM_LOGI("[KCDISP][st7701si]lcm_init +\n");

	if (kc_lcm_get_panel_detect() == PANEL_NOT_FOUND) {
		pr_err("[KCDISP][st7701si]%s skip for panel not detect\n",__func__);
		return;
	}

	KCDISP_LOG_SEQ("[KCDISP][st7701si]%s: RST H\n", __func__);
	disp_dts_gpio_select_state(DTS_GPIO_STATE_LCD_RST_ACTIVE);
	usleep_range(7*1000, 8*1000);

	KCDISP_LOG_SEQ("[KCDISP][st7701si]%s: RST L\n", __func__);
	disp_dts_gpio_select_state(DTS_GPIO_STATE_LCD_RST_SLEEP);
	usleep_range(2*1000, 3*1000);

	KCDISP_LOG_SEQ("[KCDISP][st7701si]%s: RST H\n", __func__);
	disp_dts_gpio_select_state(DTS_GPIO_STATE_LCD_RST_ACTIVE);
	usleep_range(120*1000, 121*1000);

	KCDISP_LOG_SEQ("[KCDISP][st7701si]%s: init_setting\n", __func__);
	dsi_set_cmdq_V3(init_setting, sizeof(init_setting) / sizeof(struct LCM_setting_table_V3), 1);

    LCM_LOGI("[KCDISP][st7701si]lcm_init -\n");
}

static void lcm_suspend(void)
{
	LCM_LOGI("[KCDISP][st7701si]lcm_suspend +\n");

	if (kc_lcm_get_panel_detect() == PANEL_NOT_FOUND) {
		pr_err("[KCDISP][st7701si]%s skip for panel not detect\n",__func__);
		return;
	}

	KCDISP_LOG_SEQ("[KCDISP][st7701si]%s: lcm_suspend_setting\n", __func__);
	dsi_set_cmdq_V3(lcm_suspend_setting, sizeof(lcm_suspend_setting) / sizeof(struct LCM_setting_table_V3), 1);

	KCDISP_LOG_SEQ("[KCDISP][st7701si]%s: RST L\n", __func__);
	disp_dts_gpio_select_state(DTS_GPIO_STATE_LCD_RST_SLEEP);
	usleep_range(3*1000, 4*1000);

	LCM_LOGI("[KCDISP][st7701si]lcm_suspend -\n");
}

static void lcm_resume(void)
{
	LCM_LOGI("[KCDISP][st7701si]lcm_resume +\n");

	if (kc_lcm_get_panel_detect() == PANEL_NOT_FOUND) {
		pr_err("[KCDISP][st7701si]%s skip for panel not detect\n",__func__);
		return;
	}

	lcm_init();

	LCM_LOGI("[KCDISP][st7701si]lcm_resume -\n");
}


/* return TRUE: need recovery */
/* return FALSE: No need recovery */
static unsigned int lcm_esd_check(void)
{
	char buffer[3];
	int array[4];

	if (kc_lcm_get_panel_detect() == PANEL_NOT_FOUND) {
		pr_err("[KCDISP][st7701si]%s skip for panel not detect\n",__func__);
		return FALSE;
	}

	array[0] = 0x00013700;
	dsi_set_cmdq(array, 1, 1);

	read_reg_v2(0x0A, buffer, 1);

	if (buffer[0] != 0x9C) {
		LCM_LOGI("[KCDISP][st7701si]%s: LCM ERROR [0x53]=0x%02x\n", __func__, buffer[0]);
		return TRUE;
	}
	LCM_LOGI("[KCDISP][st7701si]%s: LCM NORMAL [0x53]=0x%02x\n", __func__, buffer[0]);

	return FALSE;
}

struct LCM_DRIVER kc_st7701si_fwvga_dsi_vdo_lcm_drv = {
	.name = "kc_st7701si_fwvga_dsi_vdo",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params = lcm_get_params,
	.init = lcm_init,
	.suspend = lcm_suspend,
	.resume = lcm_resume,
	.init_power = lcm_init_power,
	.esd_check = lcm_esd_check,
	.kdisp_lcm_drv.pre_off = lcm_pre_off,
	.kdisp_lcm_drv.post_off = lcm_post_off,
	.kdisp_lcm_drv.pre_on = lcm_pre_on,
	.kdisp_lcm_drv.post_on = lcm_post_on,
	.kdisp_lcm_drv.check_refresh_disable = lcm_check_refresh_disable,
	.kdisp_lcm_drv.panel_detect_notify = lcm_notify_panel_detect,
};


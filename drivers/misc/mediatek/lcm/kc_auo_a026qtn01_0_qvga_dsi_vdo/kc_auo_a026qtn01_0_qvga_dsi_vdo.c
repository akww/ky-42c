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

#include <linux/kc_leds_drv.h>

#define LCM_LOGI(fmt, args...)  pr_notice("[KERNEL/"LOG_TAG"]"fmt, ##args)
#define LCM_LOGD(fmt, args...)  pr_notice("[KERNEL/"LOG_TAG"]"fmt, ##args)

static struct LCM_UTIL_FUNCS lcm_util;

//#define SET_RESET_PIN(v)	(lcm_util.set_reset_pin((v)))
//#define MDELAY(n)		msleep(n)  //(n >= 20ms)
//#define UDELAY(n)		udelay(n)  //(n < 20ms)
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
//#include <linux/delay.h> 
#include <linux/uaccess.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/platform_device.h>

#define LCM_DSI_CMD_MODE	1
#define FRAME_WIDTH		(240)
#define FRAME_HEIGHT	(320)
#define LCM_DENSITY		(240)

#define LCM_PHYSICAL_WIDTH	(39600)
#define LCM_PHYSICAL_HEIGHT	(52800)

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

//static struct LCM_setting_table_V3 pre_off_setting[] = {
//    {0x05, 0x28, 0, {}},
//    {REGFLAG_ESCAPE_ID, REGFLAG_DELAY_MS_V3, 20, {}},
//};

static struct LCM_setting_table_V3 lcm_suspend_setting[] = {
    {0x05, 0x28, 0, {}},
    {0x05, 0x10, 0, {}},
    {REGFLAG_ESCAPE_ID, REGFLAG_DELAY_MS_V3, 110, {}},
};

static struct LCM_setting_table_V3 post_init_setting[] = {
	{0x05, 0x11, 0, {} },
	{REGFLAG_ESCAPE_ID, REGFLAG_DELAY_MS_V3, 120, {}},
	{0x05, 0x29, 0, {} },
	{0x05, 0x35, 1, {0x00} },
	{REGFLAG_ESCAPE_ID, REGFLAG_DELAY_MS_V3, 40, {}},
};


#define KCDISP_DETECT_TIMEOUT			500
#define PANEL_NO_CHECK					0
#define PANEL_NOT_FOUND					-1
#define PANEL_FOUND						1
#define PANEL_DETECT_CHECK_RETRY_NUM	5

#define LCD_OFF							0
#define MAIN_LCD 						0x01
#define SUB_LCD 						0x02
#define LCD_MASK						0x03
#define POWER_ON    true
#define POWER_OFF   false

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
};

static struct mutex	kc_ctrl_lock;
static struct kcdisp_data kdata;
static int kc_lcm_get_panel_detect(void);
static int power_state = LCD_OFF;
void VLCD_POWER_CTRL(int lcd_type, bool on);
void VLCD_RESET_CTRL(bool on);
extern int disp_ext_sub_panel_reset_cntrl(int onoff);

///////////////////////////////////////////////////////////////

#define CLASS_NAME "kcdisp"
#define DRIVER_NAME "kcdisp"
static ssize_t kcdisp_test_mode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct kcdisp_data *kcdisp = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", kcdisp->test_mode);
}

static ssize_t kcdisp_test_mode_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct kcdisp_data *kcdisp = dev_get_drvdata(dev);
	int rc = 0;

	rc = kstrtoint(buf, 10, &kcdisp->test_mode);
	if (rc) {
		pr_err("kstrtoint failed. rc=%d\n", rc);
		return rc;
	}

	pr_err("[KCLCM]test_mode=%d\n", kcdisp->test_mode);

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

	pr_err("[KCLCM]sre_mode=%d\n", kcdisp->sre_mode);

	return count;
}

static ssize_t kcdisp_disp_connect_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct kcdisp_data *kcdisp = dev_get_drvdata(dev);
	int panel_detect_status;

	if (kcdisp->panel_detect_status == PANEL_FOUND) {
		panel_detect_status = 0x98;
	} else {
		panel_detect_status = 0;
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

	pr_err("[KCLCM]panel_detect_status=%d\n", kcdisp->panel_detect_status);

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

struct kcdisp_data *kdisp_drv_get_data(void)
{
	return &kdata;
}

static void kdisp_drv_init(void)
{
	struct kcdisp_data *kdata;

	pr_notice("[KCDISP]kdisp_drv_init +\n");

	kdata = kdisp_drv_get_data();

	pr_notice("[KCDISP]kdisp_drv_init -\n");
}

void VLCD_POWER_CTRL(int lcd_type, bool on)
{
	pr_err("[KCDISP]%s + :lcd_type = %d, on = %d, power_state = %d\n"
		,__func__, lcd_type, on, power_state);

	mutex_lock(&kc_ctrl_lock);

	if (on){
		if (power_state == LCD_OFF) {
//			UDELAY(10*1000);
			disp_dts_gpio_select_state(DTS_GPIO_STATE_LCD_VMIPI_OUT1);
			UDELAY(5*1000);
			disp_dts_gpio_select_state(DTS_GPIO_STATE_LCD_BIAS_OUT1);
			UDELAY(10*1000);
		}
		power_state |= lcd_type;
	} else {
		power_state ^= lcd_type;
		if (power_state == LCD_OFF) {
			disp_dts_gpio_select_state(DTS_GPIO_STATE_TE_MODE_GPIO);
			disp_dts_gpio_select_state(DTS_GPIO_STATE_LCD_RST_SLEEP);
			UDELAY(120*1000);
			disp_ext_sub_panel_reset_cntrl(0);

			disp_dts_gpio_select_state(DTS_GPIO_STATE_LCD_BIAS_OUT0);
			UDELAY(3*1000);
			disp_dts_gpio_select_state(DTS_GPIO_STATE_LCD_VMIPI_OUT0);
			UDELAY(3*1000);
		}
	}

	mutex_unlock(&kc_ctrl_lock);

	pr_err("[KCDISP]%s - :lcd_type = %d, on = %d, power_state = %d\n"
		,__func__, lcd_type, on, power_state);
}

void VLCD_RESET_CTRL(bool on)
{
	pr_err("[KCDISP]%s + :on = %d, power_state = %d\n"
		,__func__, on, power_state);

	mutex_lock(&kc_ctrl_lock);

	if (on){
		if (power_state != LCD_OFF) {
			disp_dts_gpio_select_state(DTS_GPIO_STATE_LCD_RST_ACTIVE);
			UDELAY(1*1000);
			disp_ext_sub_panel_reset_cntrl(1);
			UDELAY(30*1000);
		}
	} else {
		if (power_state != LCD_OFF) {
			disp_dts_gpio_select_state(DTS_GPIO_STATE_LCD_RST_SLEEP);
			UDELAY(120*1000);
			disp_ext_sub_panel_reset_cntrl(0);
		}
	}

	mutex_unlock(&kc_ctrl_lock);

	pr_err("[KCDISP]%s - :on = %d, power_state = %d\n"
		,__func__, on, power_state);
}

static const struct file_operations kcdisp_fops = {
	.owner = THIS_MODULE,
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

static int kc_lcm_platform_probe(struct platform_device *pdev)
{
	struct kcdisp_data *kcdisp;

	kcdisp = &kdata;

	kcdisp->test_mode           = 0;
	kcdisp->sre_mode            = 0;
	kcdisp->refresh_disable     = 0;
//	kcdisp->panel_detect_status = PANEL_NO_CHECK;
	kcdisp->panel_detection_1st = 0;

	kc_lcm_init_sysfs(kcdisp);
	kdisp_drv_init();

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

static int __init kc_lcm_init_auo_a026qtn01_0_qvga_dsi(void)
{
	int ret = 0;

	pr_notice("[KCDISP]%s: panel_detect_status=%d\n",
			__func__, kdata.panel_detect_status);

	pr_notice("[KCDISP]LCM: Register lcm driver\n");
	if (platform_driver_register(&lcm_driver)) {
		pr_err("[KCDISP]LCM: failed to register disp driver\n");
		ret = -ENODEV;
	}

	return ret;
}

static void __exit kc_lcm_exit_auo_a026qtn01_0_qvga_dsi(void)
{
	platform_driver_unregister(&lcm_driver);
	pr_notice("[KCLCM]LCM: Unregister lcm driver done\n");
}

late_initcall(kc_lcm_init_auo_a026qtn01_0_qvga_dsi);
module_exit(kc_lcm_exit_auo_a026qtn01_0_qvga_dsi);
MODULE_AUTHOR("Kyocera");
MODULE_DESCRIPTION("KC Display subsystem Driver");
MODULE_LICENSE("GPL");

///////////////////////////////////////////////////////////////

static int kc_lcm_get_panel_detect(void)
{
	if (!kdata.panel_detection_1st) {
		if (kdata.panel_detect_status == PANEL_NOT_FOUND) {
			kdata.refresh_disable = 1;
			kc_wled_bled_en_set(0);
		}

		kdata.panel_detection_1st = 1;
	}

	return kdata.panel_detect_status;
}

static unsigned int lcm_check_refresh_disable(void)
{
	return kdata.refresh_disable;
}

static void lcm_notify_panel_detect(int isDSIConnected)
{
	if (isDSIConnected == 0) {
		kdata.panel_detect_status = PANEL_NOT_FOUND;
	} else {
		kdata.panel_detect_status = PANEL_FOUND;
	}

	pr_notice("[KCDISP]%s panel detect:%d\n",__func__,
					kdata.panel_detect_status);
}

static void lcm_set_util_funcs(const struct LCM_UTIL_FUNCS *util)
{
	memcpy(&lcm_util, util, sizeof(struct LCM_UTIL_FUNCS));
}


static void lcm_get_params(struct LCM_PARAMS *params)
{
	pr_notice("[KCDISP]kc_auo_a026qtn01_0_qvga_dsi\n");

	memset(params, 0, sizeof(struct LCM_PARAMS));

	mutex_init(&kc_ctrl_lock);

	params->type = LCM_TYPE_DSI;

	params->width = FRAME_WIDTH;
	params->height = FRAME_HEIGHT;
	params->physical_width = LCM_PHYSICAL_WIDTH/1000;
	params->physical_height = LCM_PHYSICAL_HEIGHT/1000;
	params->physical_width_um = LCM_PHYSICAL_WIDTH;
	params->physical_height_um = LCM_PHYSICAL_HEIGHT;
	params->density = LCM_DENSITY;


#if (LCM_DSI_CMD_MODE)
	params->dsi.mode = CMD_MODE;
	params->dsi.switch_mode = SYNC_PULSE_VDO_MODE;
	lcm_dsi_mode = CMD_MODE;
#else
//	params->dsi.mode = SYNC_PULSE_VDO_MODE;
	params->dsi.mode = SYNC_EVENT_VDO_MODE;
	params->dsi.switch_mode = CMD_MODE;
	lcm_dsi_mode = SYNC_EVENT_VDO_MODE;
#endif
	LCM_LOGI("lcm_get_params lcm_dsi_mode %d\n", lcm_dsi_mode);
	params->dsi.switch_mode_enable = 0;

	/* DSI */
	/* Command mode setting */
	params->dsi.LANE_NUM = LCM_ONE_LANE;
	/* The following defined the fomat for data coming from LCD engine. */
	params->dsi.data_format.color_order = LCM_COLOR_ORDER_RGB;
	params->dsi.data_format.trans_seq = LCM_DSI_TRANS_SEQ_MSB_FIRST;
	params->dsi.data_format.padding = LCM_DSI_PADDING_ON_LSB;
	params->dsi.data_format.format = LCM_DSI_FORMAT_RGB888;

	/* Highly depends on LCD driver capability. */
	params->dsi.packet_size = 256;
	/* video mode timing */

	params->dsi.PS = LCM_PACKED_PS_24BIT_RGB888;

	params->dsi.vertical_sync_active = 8;
	params->dsi.vertical_backporch = 16;
	params->dsi.vertical_frontporch = 16;
	params->dsi.vertical_active_line = FRAME_HEIGHT;

	params->dsi.horizontal_sync_active = 6;
	params->dsi.horizontal_backporch = 38;
	params->dsi.horizontal_frontporch = 38;
	params->dsi.horizontal_active_pixel = FRAME_WIDTH;
	params->dsi.ssc_disable = 1;
	params->dsi.ssc_range = 5;
	params->dsi.PLL_CLOCK = 85;
	params->dsi.PLL_CK_CMD = 85;
//	params->dsi.PLL_CK_VDO = 170;
	params->dsi.CLK_HS_POST = 36;
	params->dsi.clk_lp_per_line_enable = 0;
    params->dsi.esd_check_enable = 1;
    params->dsi.customization_esd_check_enable = 0;
    params->dsi.lcm_esd_check_table[0].cmd = 0x0A;
    params->dsi.lcm_esd_check_table[0].count = 1;
    params->dsi.lcm_esd_check_table[0].para_list[0] = 0x9C;

//	params->dsi.lcm_ext_te_enable = 1;
//	params->dsi.cont_clock = 1;
}

static void lcm_pre_off(void)
{
	pr_notice("[KCDISP]lcm_pre_off\n");

	if (kc_lcm_get_panel_detect() == PANEL_NOT_FOUND) {
		pr_err("[KCDISP]%s skip for panel not detect\n",__func__);
		return;
	}

//	dsi_set_cmdq_V3(pre_off_setting, sizeof(pre_off_setting) / sizeof(struct LCM_setting_table_V3), 1);
}

static void lcm_post_off(void)
{
	if (kc_lcm_get_panel_detect() == PANEL_NOT_FOUND) {
		pr_err("[KCDISP]%s skip for panel not detect\n",__func__);
		return;
	}

	pr_notice("[KCDISP]lcm_post_off\n");

	VLCD_POWER_CTRL(MAIN_LCD, POWER_OFF);
}

static void lcm_pre_on(void)
{
	pr_notice("[KCDISP]lcm_pre_on +\n");

	if (kc_lcm_get_panel_detect() == PANEL_NOT_FOUND) {
		pr_err("[KCDISP]%s skip for panel not detect\n",__func__);
		return;
	}

	VLCD_POWER_CTRL(MAIN_LCD, POWER_ON);
	pr_notice("[KCDISP]lcm_pre_on -\n");
}

static void lcm_post_on(void)
{
	pr_notice("[KCDISP]lcm_post_on +\n");

	if (kc_lcm_get_panel_detect() == PANEL_NOT_FOUND) {
		pr_err("[KCDISP]%s skip for panel not detect\n",__func__);
		return;
	}

	dsi_set_cmdq_V3(post_init_setting, sizeof(post_init_setting) / sizeof(struct LCM_setting_table_V3), 1);
    
	MDELAY(20);
    kc_wled_bled_en_set(1);
    
	pr_notice("[KCDISP]lcm_post_on -\n");
}

static void lcm_init(void)
{
	LCM_LOGI("[KCDISP]lcm_init +\n");

	if (kc_lcm_get_panel_detect() == PANEL_NOT_FOUND) {
		pr_err("[KCDISP]%s skip for panel not detect\n",__func__);
		return;
	}

	VLCD_RESET_CTRL(POWER_ON);

	disp_dts_gpio_select_state(DTS_GPIO_STATE_TE_MODE_TE);

	disp_dts_gpio_select_state(DTS_GPIO_STATE_LCD_RST_SLEEP);
	UDELAY(10);

	disp_dts_gpio_select_state(DTS_GPIO_STATE_LCD_RST_ACTIVE);
	UDELAY(5*1000);

//	dsi_set_cmdq_V3(init_setting, sizeof(init_setting) / sizeof(struct LCM_setting_table_V3), 1);

    LCM_LOGI("[KCDISP]lcm_init -\n");
}

static void lcm_suspend(void)
{
	LCM_LOGI("[KCDISP]lcm_suspend +\n");

	if (kc_lcm_get_panel_detect() == PANEL_NOT_FOUND) {
		pr_err("[KCDISP]%s skip for panel not detect\n",__func__);
		return;
	}
    
    kc_wled_bled_en_set(0);

	dsi_set_cmdq_V3(lcm_suspend_setting, sizeof(lcm_suspend_setting) / sizeof(struct LCM_setting_table_V3), 1);

	LCM_LOGI("[KCDISP]lcm_suspend -\n");
}

static void lcm_resume(void)
{
	LCM_LOGI("[KCDISP]lcm_resume +\n");

	if (kc_lcm_get_panel_detect() == PANEL_NOT_FOUND) {
		pr_err("[KCDISP]%s skip for panel not detect\n",__func__);
		return;
	}

	lcm_init();

	LCM_LOGI("[KCDISP]lcm_resume -\n");
}

static void lcm_update(unsigned int x, unsigned int y, unsigned int width,
	unsigned int height)
{
	unsigned int x0 = x;
	unsigned int y0 = y;
	unsigned int x1 = x0 + width - 1;
	unsigned int y1 = y0 + height - 1;

	unsigned char x0_MSB = ((x0 >> 8) & 0xFF);
	unsigned char x0_LSB = (x0 & 0xFF);
	unsigned char x1_MSB = ((x1 >> 8) & 0xFF);
	unsigned char x1_LSB = (x1 & 0xFF);
	unsigned char y0_MSB = ((y0 >> 8) & 0xFF);
	unsigned char y0_LSB = (y0 & 0xFF);
	unsigned char y1_MSB = ((y1 >> 8) & 0xFF);
	unsigned char y1_LSB = (y1 & 0xFF);

	unsigned int data_array[16];

	LCM_LOGI("[KCDISP]lcm_update +\n");

	data_array[0] = 0x00053902;
	data_array[1] = (x1_MSB << 24) | (x0_LSB << 16) | (x0_MSB << 8) | 0x2a;
	data_array[2] = (x1_LSB);
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00053902;
	data_array[1] = (y1_MSB << 24) | (y0_LSB << 16) | (y0_MSB << 8) | 0x2b;
	data_array[2] = (y1_LSB);
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x002c3909;
	dsi_set_cmdq(data_array, 1, 0);
	LCM_LOGI("[KCDISP]lcm_update -\n");
}


/* return TRUE: need recovery */
/* return FALSE: No need recovery */
static unsigned int lcm_esd_check(void)
{
	char buffer[3];
	int array[4];

	if (kc_lcm_get_panel_detect() == PANEL_NOT_FOUND) {
		pr_err("[KCDISP]%s skip for panel not detect\n",__func__);
		return FALSE;
	}

	array[0] = 0x00013700;
	dsi_set_cmdq(array, 1, 1);

	read_reg_v2(0x0A, buffer, 1);

	if (buffer[0] != 0x9C) {
		LCM_LOGI("[LCM ERROR] [0x53]=0x%02x\n", buffer[0]);
		return TRUE;
	}
	LCM_LOGI("[LCM NORMAL] [0x53]=0x%02x\n", buffer[0]);

	return FALSE;
}

struct LCM_DRIVER kc_auo_a026qtn01_0_qvga_dsi_vdo_lcm_drv = {
	.name = "kc_auo_a026qtn01_0_qvga_dsi_vdo",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params = lcm_get_params,
	.init = lcm_init,
	.suspend = lcm_suspend,
	.resume = lcm_resume,
	.esd_check = lcm_esd_check,
	.update = lcm_update,
	.kdisp_lcm_drv.pre_off = lcm_pre_off,
	.kdisp_lcm_drv.post_off = lcm_post_off,
	.kdisp_lcm_drv.pre_on = lcm_pre_on,
	.kdisp_lcm_drv.post_on = lcm_post_on,
	.kdisp_lcm_drv.check_refresh_disable = lcm_check_refresh_disable,
	.kdisp_lcm_drv.panel_detect_notify = lcm_notify_panel_detect,
};


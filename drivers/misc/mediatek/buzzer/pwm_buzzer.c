/*
 * This software is contributed or developed by KYOCERA Corporation.
 * (C) 2022 KYOCERA Corporation
 */
/* Copyright (c) 2014-2015, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/atomic.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/workqueue.h>
#include <linux/of_gpio.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/power_supply.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/pm_wakeup.h>
#include <mt-plat/mtk_pwm.h>
#include <mt-plat/mtk_pwm_hal_pub.h>

#define PWM_BUZZER_RING_SWITCHING_TIME_MS 60
#define PWM_BUZZER_RING_MIN_VOL_COUNT 50
#define PWM_BUZZER_RING_MIN_VOL_COUNT_LOW 7
#define PWM_ERR_CHECK_INTERVAL_MS 100
#define PWM_ERR_CHECK_COUNT 3
#define GPIO_HIGH  1
#define SW_KC_BUZZER_ERROR 0x14 /*event ID*/
#define BUZZER_5V_CNT_ENABLE_TEMP -5

enum buzzer_ctrl_id {
	BUZZER_RING_OFF = 0,
	BUZZER_IDLE_RING_ON,
	BUZZER_CALL_RING_ON,
	BUZZER_3950Hz_RING_ON = 99
};

enum pinctrl_id {
	PINCTRL_PWM_GPIO_DEFAULT = 0,
	PINCTRL_PWM_GPIO_SET,
	PINCTRL_5V_CNT_ACTIVE,
	PINCTRL_5V_CNT_SUSPEND,
	PINCTRL_ID_MAX
};

struct pwm_buzzer_data {
	struct device *dev;
	atomic_t ringing;
	atomic_t shutdown;
	struct pinctrl *pin;
	struct pinctrl_state *pin_state[PINCTRL_ID_MAX];
	struct delayed_work buzzer_ring_dwork;
	struct delayed_work buzzer_det_dwork;
	struct input_dev *input_dev;
};

static struct pwm_buzzer_data *pdata;

static struct mutex lock_sysfs;
static struct mutex buzzer_irq_lock;

static struct wakeup_source buzzer_ring_wake_lock_attach;

static bool buzzer_err_state = false;
static bool buzzer_err_irq_enable = false;
static int  buzzer_err_check_cnt = 0;
static int  buzzer_err_det_gpio = 0;
static int  buzzer_ring_cnt = 0;

static const char * const pwm_buzzer_pin_names[PINCTRL_ID_MAX] = {
	[PINCTRL_PWM_GPIO_DEFAULT]    = "buzzer_pwm_gpio_default",
	[PINCTRL_PWM_GPIO_SET]        = "buzzer_pwm_gpio_set",
	// [PINCTRL_5V_CNT_SUSPEND]      = "buzzer_5v_cnt_suspend",
	[PINCTRL_5V_CNT_SUSPEND]      = "buzzer_5v_cnt_active",
	[PINCTRL_5V_CNT_ACTIVE]       = "buzzer_5v_cnt_active",
};

unsigned int pwm_buzzer_count_01 = 0;
unsigned int pwm_buzzer_count_02 = 0;
unsigned int pwm_buzzer_count_03 = 0;
unsigned int pwm_buzzer_count_04 = 0;
unsigned int pwm_buzzer_count_05 = 0;
unsigned int pwm_buzzer_count_06 = 0;
unsigned int pwm_buzzer_count_07 = 0;
unsigned int pwm_buzzer_count_08 = 0;
unsigned int pwm_buzzer_count_09 = 0;

int pwm_buzzer_get_battery_temp(void)
{
	struct power_supply *battery_psy = NULL;
	union power_supply_propval ret = {0,};

	battery_psy = power_supply_get_by_name("battery");

	if (battery_psy)
		power_supply_get_property(battery_psy, POWER_SUPPLY_PROP_TEMP, &ret);

	pr_debug("%s battery temp = %d\n", __func__, ret.intval/10);

	return ret.intval/10;
}

int32_t pwm_buzzer_pin_select(struct device *dev, enum pinctrl_id id)
{
	struct pwm_buzzer_data *data = dev_get_drvdata(dev);
	int32_t ret;

	if (!data->pin) {
		dev_err(dev, "%s pin is null\n", __func__);
		return -EINVAL;
	}

	if (data->pin_state[id] == NULL) {
		dev_err(dev, "%s pin_state[%d] is invalid\n", __func__, id);
		return -EINVAL;
	}

	ret = pinctrl_select_state(data->pin, data->pin_state[id]);
	if (ret) {
		dev_err(dev, "%s cannot select state[%d]\n", __func__, id);
		return -EINVAL;
	}

	dev_info(dev, "%s Selected state[%d]\n", __func__, id);

	return ret;
}

int32_t pwm_buzzer_pin_initialize(struct device *dev)
{
	struct pwm_buzzer_data *data = dev_get_drvdata(dev);
	int32_t ret = 0;
	uint32_t i;

	dev_info(dev, "%s start\n", __func__);

	data->pin = devm_pinctrl_get(dev);
	if (IS_ERR(data->pin)) {
		if (PTR_ERR(data->pin) == -EPROBE_DEFER) {
			dev_err(dev, "%s pinctrl not ready\n", __func__);
			data->pin = NULL;
			ret = -EPROBE_DEFER;
			goto err;
		}
		dev_err(dev, "%s Target does not use pinctrl\n", __func__);
		data->pin = NULL;
		ret = -EINVAL;
		goto err;
	}

	for (i = 0; i < PINCTRL_ID_MAX; i++) {
		const char *n = pwm_buzzer_pin_names[i];
		struct pinctrl_state *state = pinctrl_lookup_state(data->pin, n);
		if (IS_ERR(state)) {
			dev_err(dev, "%s cannot find %s\n", __func__, n);
			ret = -EPROBE_DEFER;
			goto err;
		}
		dev_info(dev, "%s found pin control %s\n", __func__, n);
		data->pin_state[i] = state;
	}

	pwm_buzzer_pin_select(data->dev, PINCTRL_PWM_GPIO_DEFAULT);
	pwm_buzzer_pin_select(data->dev, PINCTRL_5V_CNT_SUSPEND);

	dev_info(dev, "%s end\n", __func__);
	return 0;

err:
	if (data->pin) {
		devm_pinctrl_put(data->pin);
		data->pin = NULL;
	}

	return ret;
}

static irqreturn_t pwm_buzzer_det_handler(int irq, void *dev)
{
	bool result;
	struct pwm_buzzer_data *data = dev_get_drvdata(dev);

	dev_info(dev, "%s enter\n", __func__);

	result = cancel_delayed_work_sync(&data->buzzer_det_dwork);
	mutex_lock(&buzzer_irq_lock);
	if (result) {
		buzzer_err_check_cnt = 0;
		pr_debug("%s canceled, check count is 0 cleared\n", __func__);
	} else {
		pr_debug("%s not canceled, check count = %d\n", __func__, buzzer_err_check_cnt);
	}
	mutex_unlock(&buzzer_irq_lock);
	schedule_delayed_work(&data->buzzer_det_dwork, 0);

	return IRQ_HANDLED;
}

static void pwm_buzzer_set_irq(struct pwm_buzzer_data *data, bool enable)
{
	dev_info(data->dev, "%s enable = %d\n", __func__, enable);

	if (buzzer_err_irq_enable == enable) {
		pr_err("%s: irq is already:%d, skipped!!\n", __func__, enable);
		return;
	}

	if (enable) {
		enable_irq(gpio_to_irq(buzzer_err_det_gpio));
		enable_irq_wake(gpio_to_irq(buzzer_err_det_gpio));
	} else {
		disable_irq_wake(gpio_to_irq(buzzer_err_det_gpio));
		disable_irq(gpio_to_irq(buzzer_err_det_gpio));
	}

	buzzer_err_irq_enable = enable;
}

static void pwm_buzzer_error_detection(struct work_struct *work)
{
	int gpio_val;
	struct delayed_work *dwork;
	struct pwm_buzzer_data *data;
	
	dwork = to_delayed_work(work);
	data = container_of(dwork, struct pwm_buzzer_data, buzzer_det_dwork);

	dev_info(data->dev, "%s start\n", __func__);

	mutex_lock(&buzzer_irq_lock);
	if (!data->input_dev) {
		dev_err(data->dev, "%s input_dev NULL\n", __func__);
		mutex_unlock(&buzzer_irq_lock);
		return;
	}

	if (buzzer_err_check_cnt == 0) {
		buzzer_err_check_cnt++;
		schedule_delayed_work(&data->buzzer_det_dwork, msecs_to_jiffies(PWM_ERR_CHECK_INTERVAL_MS));
	} else {
		gpio_val = gpio_get_value(buzzer_err_det_gpio);
		dev_info(data->dev, "%s: GPIO value = %x, check count = %d\n", __func__, gpio_val, buzzer_err_check_cnt);
		if (gpio_val == GPIO_HIGH) {
			if (buzzer_err_check_cnt < PWM_ERR_CHECK_COUNT) {
				dev_info(data->dev, "%s: check count = %d\n", __func__, buzzer_err_check_cnt);
				buzzer_err_check_cnt++;
				schedule_delayed_work(&data->buzzer_det_dwork, msecs_to_jiffies(PWM_ERR_CHECK_INTERVAL_MS));
			} else {
				dev_info(data->dev, "%s check count = %d, report event!!\n", __func__, buzzer_err_check_cnt);
				input_report_switch(data->input_dev, SW_KC_BUZZER_ERROR, 1);
				input_sync(data->input_dev);
				buzzer_err_check_cnt = 0;
				buzzer_err_state = true;
			}
		} else {
			dev_info(data->dev, "%s: Low detected, not err.\n", __func__);
			buzzer_err_check_cnt = 0;
			if (buzzer_err_state) {
				input_report_switch(data->input_dev, SW_KC_BUZZER_ERROR, 0);
				buzzer_err_state= false;
			}
		}
	}
	mutex_unlock(&buzzer_irq_lock);
}

static void pwm_buzzer_ring_control(struct work_struct *work)
{
	struct delayed_work *dwork;
	struct pwm_buzzer_data *data;
	struct pwm_easy_config conf;
	int ret = 0;
	int temp = 0;

	dwork = to_delayed_work(work);
	data = container_of(dwork, struct pwm_buzzer_data, buzzer_ring_dwork);
	temp = pwm_buzzer_get_battery_temp();

	dev_dbg(data->dev, "%s\n", __func__);

	if (atomic_read(&data->shutdown)) {
		dev_err(data->dev,"%s cancel for shutdown %d\n", __func__,atomic_read(&data->shutdown));
		pwm_buzzer_count_06++;
		return;
	}

	pwm_buzzer_count_03++;
	buzzer_ring_cnt++;

	if( temp > BUZZER_5V_CNT_ENABLE_TEMP ) {
		switch (atomic_read(&data->ringing)) {
		case BUZZER_CALL_RING_ON:
			if( buzzer_ring_cnt < PWM_BUZZER_RING_MIN_VOL_COUNT ) {
				if( buzzer_ring_cnt % 2 ) {
					dev_dbg(data->dev, "%s buzzer on 3950Hz vol MIN\n", __func__);
					conf.pwm_no = 0;
					conf.clk_div = CLK_DIV1;
					conf.clk_src = PWM_CLK_OLD_MODE_BLOCK;
					conf.duration = 6582;
					conf.duty = buzzer_ring_cnt;
					conf.pmic_pad = 0;
				} else {
					dev_dbg(data->dev, "%s buzzer on 4400Hz vol MIN\n", __func__);
					conf.pwm_no = 0;
					conf.clk_div = CLK_DIV1;
					conf.clk_src = PWM_CLK_OLD_MODE_BLOCK;
					conf.duration = 5909;
					conf.duty = buzzer_ring_cnt;
					conf.pmic_pad = 0;
				}
				break;
			}
		case BUZZER_IDLE_RING_ON:
			if( buzzer_ring_cnt % 2 ) {
				dev_dbg(data->dev, "%s buzzer on 3950Hz vol MAX\n", __func__);
				conf.pwm_no = 0;
				conf.clk_div = CLK_DIV1;
				conf.clk_src = PWM_CLK_OLD_MODE_BLOCK;
				conf.duration = 6582;
				conf.duty = 50;
				conf.pmic_pad = 0;
			} else {
				dev_dbg(data->dev, "%s buzzer on 4400Hz vol MAX\n", __func__);
				conf.pwm_no = 0;
				conf.clk_div = CLK_DIV1;
				conf.clk_src = PWM_CLK_OLD_MODE_BLOCK;
				conf.duration = 5909;
				conf.duty = 50;
				conf.pmic_pad = 0;
			}
			break;
		case BUZZER_3950Hz_RING_ON:
			if( buzzer_ring_cnt % 2 ) {
				dev_dbg(data->dev, "%s buzzer on 3950Hz vol MAX\n", __func__);
				conf.pwm_no = 0;
				conf.clk_div = CLK_DIV1;
				conf.clk_src = PWM_CLK_OLD_MODE_BLOCK;
				conf.duration = 6582;
				conf.duty = 50;
				conf.pmic_pad = 0;
			} else {
				dev_dbg(data->dev, "%s buzzer on 3950Hz vol MAX\n", __func__);
				conf.pwm_no = 0;
				conf.clk_div = CLK_DIV1;
				conf.clk_src = PWM_CLK_OLD_MODE_BLOCK;
				conf.duration = 6582;
				conf.duty = 50;
				conf.pmic_pad = 0;
			}
			break;
		case 90:
			if( buzzer_ring_cnt % 2 ) {
				dev_dbg(data->dev, "%s buzzer on 4000Hz vol MAX\n", __func__);
				conf.pwm_no = 0;
				conf.clk_div = CLK_DIV1;
				conf.clk_src = PWM_CLK_OLD_MODE_BLOCK;
				conf.duration = 6500;
				conf.duty = 50;
				conf.pmic_pad = 0;
			} else {
				dev_dbg(data->dev, "%s buzzer on 4000Hz vol MAX\n", __func__);
				conf.pwm_no = 0;
				conf.clk_div = CLK_DIV1;
				conf.clk_src = PWM_CLK_OLD_MODE_BLOCK;
				conf.duration = 6500;
				conf.duty = 50;
				conf.pmic_pad = 0;
			}
			break;
		case 91:
			if( buzzer_ring_cnt % 2 ) {
				dev_dbg(data->dev, "%s buzzer on 4100Hz vol MAX\n", __func__);
				conf.pwm_no = 0;
				conf.clk_div = CLK_DIV1;
				conf.clk_src = PWM_CLK_OLD_MODE_BLOCK;
				conf.duration = 6341;
				conf.duty = 50;
				conf.pmic_pad = 0;
			} else {
				dev_dbg(data->dev, "%s buzzer on 4100Hz vol MAX\n", __func__);
				conf.pwm_no = 0;
				conf.clk_div = CLK_DIV1;
				conf.clk_src = PWM_CLK_OLD_MODE_BLOCK;
				conf.duration = 6341;
				conf.duty = 50;
				conf.pmic_pad = 0;
			}
			break;
		case 92:
			if( buzzer_ring_cnt % 2 ) {
				dev_dbg(data->dev, "%s buzzer on 4200Hz vol MAX\n", __func__);
				conf.pwm_no = 0;
				conf.clk_div = CLK_DIV1;
				conf.clk_src = PWM_CLK_OLD_MODE_BLOCK;
				conf.duration = 6190;
				conf.duty = 50;
				conf.pmic_pad = 0;
			} else {
				dev_dbg(data->dev, "%s buzzer on 4200Hz vol MAX\n", __func__);
				conf.pwm_no = 0;
				conf.clk_div = CLK_DIV1;
				conf.clk_src = PWM_CLK_OLD_MODE_BLOCK;
				conf.duration = 6190;
				conf.duty = 50;
				conf.pmic_pad = 0;
			}
			break;
		case 93:
			if( buzzer_ring_cnt % 2 ) {
				dev_dbg(data->dev, "%s buzzer on 4300Hz vol MAX\n", __func__);
				conf.pwm_no = 0;
				conf.clk_div = CLK_DIV1;
				conf.clk_src = PWM_CLK_OLD_MODE_BLOCK;
				conf.duration = 6046;
				conf.duty = 50;
				conf.pmic_pad = 0;
			} else {
				dev_dbg(data->dev, "%s buzzer on 4300Hz vol MAX\n", __func__);
				conf.pwm_no = 0;
				conf.clk_div = CLK_DIV1;
				conf.clk_src = PWM_CLK_OLD_MODE_BLOCK;
				conf.duration = 6046;
				conf.duty = 50;
				conf.pmic_pad = 0;
			}
			break;
		case 94:
			if( buzzer_ring_cnt % 2 ) {
				dev_dbg(data->dev, "%s buzzer on 4400Hz vol MAX\n", __func__);
				conf.pwm_no = 0;
				conf.clk_div = CLK_DIV1;
				conf.clk_src = PWM_CLK_OLD_MODE_BLOCK;
				conf.duration = 5909;
				conf.duty = 50;
				conf.pmic_pad = 0;
			} else {
				dev_dbg(data->dev, "%s buzzer on 4400Hz vol MAX\n", __func__);
				conf.pwm_no = 0;
				conf.clk_div = CLK_DIV1;
				conf.clk_src = PWM_CLK_OLD_MODE_BLOCK;
				conf.duration = 5909;
				conf.duty = 50;
				conf.pmic_pad = 0;
			}
			break;
		case BUZZER_RING_OFF:
		default:
			dev_dbg(data->dev, "%s buzzer off\n", __func__);
			conf.pwm_no = 0;
			conf.clk_div = CLK_DIV1;
			conf.clk_src = PWM_CLK_OLD_MODE_BLOCK;
			conf.duration = 0;
			conf.duty = 0;
			conf.pmic_pad = 0;
			break;
		}
	} else {
		switch (atomic_read(&data->ringing)) {
		case BUZZER_CALL_RING_ON:
			if( buzzer_ring_cnt < PWM_BUZZER_RING_MIN_VOL_COUNT_LOW ) {
				if( buzzer_ring_cnt % 2 ) {
					dev_dbg(data->dev, "%s buzzer on 3950Hz vol MIN\n", __func__);
					conf.pwm_no = 0;
					conf.clk_div = CLK_DIV1;
					conf.clk_src = PWM_CLK_OLD_MODE_BLOCK;
					conf.duration = 6582;
					conf.duty = buzzer_ring_cnt;
					conf.pmic_pad = 0;
				} else {
					dev_dbg(data->dev, "%s buzzer on 4400Hz vol MIN\n", __func__);
					conf.pwm_no = 0;
					conf.clk_div = CLK_DIV1;
					conf.clk_src = PWM_CLK_OLD_MODE_BLOCK;
					conf.duration = 5909;
					conf.duty = buzzer_ring_cnt;
					conf.pmic_pad = 0;
				}
				break;
			}
		case BUZZER_IDLE_RING_ON:
			if( buzzer_ring_cnt % 2 ) {
				dev_dbg(data->dev, "%s buzzer on 3950Hz vol MAX\n", __func__);
				conf.pwm_no = 0;
				conf.clk_div = CLK_DIV1;
				conf.clk_src = PWM_CLK_OLD_MODE_BLOCK;
				conf.duration = 6582;
				conf.duty = 7;
				conf.pmic_pad = 0;
			} else {
				dev_dbg(data->dev, "%s buzzer on 4400Hz vol MAX\n", __func__);
				conf.pwm_no = 0;
				conf.clk_div = CLK_DIV1;
				conf.clk_src = PWM_CLK_OLD_MODE_BLOCK;
				conf.duration = 5909;
				conf.duty = 7;
				conf.pmic_pad = 0;
			}
			break;
		case BUZZER_RING_OFF:
		default:
			dev_dbg(data->dev, "%s buzzer off\n", __func__);
			conf.pwm_no = 0;
			conf.clk_div = CLK_DIV1;
			conf.clk_src = PWM_CLK_OLD_MODE_BLOCK;
			conf.duration = 0;
			conf.duty = 0;
			conf.pmic_pad = 0;
			break;
		}
	}

	pwm_buzzer_count_04++;

	if(atomic_read(&data->ringing)) {
		schedule_delayed_work(&data->buzzer_ring_dwork, msecs_to_jiffies(PWM_BUZZER_RING_SWITCHING_TIME_MS));
	}
	
//#ifdef PWM_HW_V_1_0
//	mt_pwm_clk_sel_hal(pwm_no, CLK_26M);
//#else
//	mt_pwm_26M_clk_enable_hal(1);
//#endif
	ret = pwm_set_easy_config(&conf);
	if(ret != RSUCCESS) {
		dev_err(data->dev,"%s pwm set error : %d\n", __func__,ret);
		pwm_buzzer_count_09++;
	}

	pwm_buzzer_count_05++;
	dev_dbg(data->dev, "%s end\n", __func__);
}

static ssize_t buzzer_status_show_01(struct device *dev,
	struct device_attribute *attr, char *buf)
{
  dev_info(dev, "%s %d\n", __func__, pwm_buzzer_count_01);
	return scnprintf(buf, PAGE_SIZE, "%d\n", pwm_buzzer_count_01);
}
static DEVICE_ATTR(buzzer_status_01, S_IRUSR, buzzer_status_show_01, NULL);

static ssize_t buzzer_status_show_02(struct device *dev,
	struct device_attribute *attr, char *buf)
{
  dev_info(dev, "%s %d\n", __func__, pwm_buzzer_count_02);
	return scnprintf(buf, PAGE_SIZE, "%d\n", pwm_buzzer_count_02);
}
static DEVICE_ATTR(buzzer_status_02, S_IRUSR, buzzer_status_show_02, NULL);

static ssize_t buzzer_status_show_03(struct device *dev,
	struct device_attribute *attr, char *buf)
{
  dev_info(dev, "%s %d\n", __func__, pwm_buzzer_count_03);
	return scnprintf(buf, PAGE_SIZE, "%d\n", pwm_buzzer_count_03);
}
static DEVICE_ATTR(buzzer_status_03, S_IRUSR, buzzer_status_show_03, NULL);

static ssize_t buzzer_status_show_04(struct device *dev,
	struct device_attribute *attr, char *buf)
{
  dev_info(dev, "%s %d\n", __func__, pwm_buzzer_count_04);
	return scnprintf(buf, PAGE_SIZE, "%d\n", pwm_buzzer_count_04);
}
static DEVICE_ATTR(buzzer_status_04, S_IRUSR, buzzer_status_show_04, NULL);

static ssize_t buzzer_status_show_05(struct device *dev,
	struct device_attribute *attr, char *buf)
{
  dev_info(dev, "%s %d\n", __func__, pwm_buzzer_count_05);
	return scnprintf(buf, PAGE_SIZE, "%d\n", pwm_buzzer_count_05);
}
static DEVICE_ATTR(buzzer_status_05, S_IRUSR, buzzer_status_show_05, NULL);

static ssize_t buzzer_status_show_06(struct device *dev,
	struct device_attribute *attr, char *buf)
{
  dev_info(dev, "%s %d\n", __func__, pwm_buzzer_count_06);
	return scnprintf(buf, PAGE_SIZE, "%d\n", pwm_buzzer_count_06);
}
static DEVICE_ATTR(buzzer_status_06, S_IRUSR, buzzer_status_show_06, NULL);

static ssize_t buzzer_status_show_07(struct device *dev,
	struct device_attribute *attr, char *buf)
{
  dev_info(dev, "%s %d\n", __func__, pwm_buzzer_count_07);
	return scnprintf(buf, PAGE_SIZE, "%d\n", pwm_buzzer_count_07);
}
static DEVICE_ATTR(buzzer_status_07, S_IRUSR, buzzer_status_show_07, NULL);

static ssize_t buzzer_status_show_08(struct device *dev,
	struct device_attribute *attr, char *buf)
{
  dev_info(dev, "%s %d\n", __func__, pwm_buzzer_count_08);
	return scnprintf(buf, PAGE_SIZE, "%d\n", pwm_buzzer_count_08);
}
static DEVICE_ATTR(buzzer_status_08, S_IRUSR, buzzer_status_show_08, NULL);

static ssize_t buzzer_status_show_09(struct device *dev,
	struct device_attribute *attr, char *buf)
{
  dev_info(dev, "%s %d\n", __func__, pwm_buzzer_count_09);
	return scnprintf(buf, PAGE_SIZE, "%d\n", pwm_buzzer_count_09);
}
static DEVICE_ATTR(buzzer_status_09, S_IRUSR, buzzer_status_show_09, NULL);

static ssize_t ring_enable_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n", atomic_read(&pdata->ringing));
}

static ssize_t ring_enable_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int32_t ret = 0;
	uint32_t enable = 0;
	
	if(!atomic_read(&pdata->ringing)) {
		pwm_buzzer_count_01 = 0;
		pwm_buzzer_count_02 = 0;
		pwm_buzzer_count_03 = 0;
		pwm_buzzer_count_04 = 0;
		pwm_buzzer_count_05 = 0;
		pwm_buzzer_count_06 = 0;
		pwm_buzzer_count_07 = 0;
		pwm_buzzer_count_08 = 0;
		pwm_buzzer_count_09 = 0;
	}

	dev_info(dev, "%s start\n", __func__);

	if (atomic_read(&pdata->shutdown)) {
		dev_err(dev,"%s can't enable! shutdown %d\n", __func__, atomic_read(&pdata->shutdown));
		pwm_buzzer_count_06++;
		goto exit;
	}

	mutex_lock(&lock_sysfs);

	ret = sscanf(buf, "%u", &enable);
	if (ret != 1) {
		dev_err(dev, "%s invalid param ret=%u\n", __func__, ret);
		pwm_buzzer_count_07++;
		count = -EIO;
	}

	mutex_lock(&buzzer_irq_lock);
	if (buzzer_err_state) {
		dev_err(dev, "%s buzzer state is err\n", __func__);
		pwm_buzzer_count_08++;
		mutex_unlock(&buzzer_irq_lock);
		count = -EPERM;
		goto exit;
	}
	mutex_unlock(&buzzer_irq_lock);

	dev_info(dev, "%s enable %u, ringing %d\n", __func__, enable, atomic_read(&pdata->ringing));

	cancel_delayed_work_sync(&pdata->buzzer_ring_dwork);

	if (enable && !atomic_read(&pdata->ringing)) {
		__pm_stay_awake(&buzzer_ring_wake_lock_attach);
		atomic_set(&pdata->ringing, enable);
		buzzer_ring_cnt = 0;
		pwm_buzzer_set_irq(pdata, 0);
		pwm_buzzer_pin_select(pdata->dev, PINCTRL_PWM_GPIO_SET);
		pwm_buzzer_pin_select(pdata->dev, PINCTRL_5V_CNT_ACTIVE);
		usleep_range(5000, 5000);
		pwm_buzzer_count_01++;
		dev_info(dev, "%s start\n", __func__);
		schedule_delayed_work(&pdata->buzzer_ring_dwork, 0);
	}
	else if (!enable && atomic_read(&pdata->ringing)) {
		atomic_set(&pdata->ringing, 0);
		schedule_delayed_work(&pdata->buzzer_ring_dwork, 0);
		pwm_buzzer_pin_select(pdata->dev, PINCTRL_PWM_GPIO_DEFAULT);
		pwm_buzzer_pin_select(pdata->dev, PINCTRL_5V_CNT_SUSPEND);
		if (gpio_get_value(buzzer_err_det_gpio) == GPIO_HIGH) {
			pr_debug("%s: detected high, start error_detection!!\n", __func__);
			schedule_delayed_work(&pdata->buzzer_det_dwork, 0);
		}
		pwm_buzzer_set_irq(pdata, 1);
		__pm_relax(&buzzer_ring_wake_lock_attach);
		pwm_buzzer_count_02++;
		dev_info(dev, "%s stop\n", __func__);
	}

exit:
	mutex_unlock(&lock_sysfs);
	return count;
}

static DEVICE_ATTR(ring_enable, S_IRUSR | S_IWUSR, ring_enable_show, ring_enable_store);

static struct attribute *ring_enable_attrs[] = {
	&dev_attr_ring_enable.attr,
	&dev_attr_buzzer_status_01.attr,
	&dev_attr_buzzer_status_02.attr,
	&dev_attr_buzzer_status_03.attr,
	&dev_attr_buzzer_status_04.attr,
	&dev_attr_buzzer_status_05.attr,
	&dev_attr_buzzer_status_06.attr,
	&dev_attr_buzzer_status_07.attr,
	&dev_attr_buzzer_status_08.attr,
	&dev_attr_buzzer_status_09.attr,
	NULL,
};

static struct attribute_group ring_enable_group = {
	.attrs = ring_enable_attrs,
};

static const struct attribute_group *buzzer_group[] = {
	&ring_enable_group,
	NULL
};

static struct led_classdev led_buzzer = {
	.name		= "buzzer",
	.groups		= buzzer_group,
};

static int32_t pwm_buzzer_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	int32_t rc = 0;
	int32_t sysfs_rc = -1;
	struct device_node *np = dev->of_node;
	struct pwm_buzzer_data *data = devm_kzalloc(dev, sizeof(struct pwm_buzzer_data),GFP_KERNEL);

	dev_info(dev, "%s: start\n", __func__);

	if (!data) {
		dev_err(dev,
			"%s: failed to allocate memory for struct pwm_buzzer_data\n", __func__);
		rc = -ENOMEM;
		return rc;
	}

	data->dev = dev;

	dev_set_drvdata(dev, data);

	atomic_set(&data->ringing, 0);
	atomic_set(&data->shutdown, 0);
	mutex_init(&lock_sysfs);
	mutex_init(&buzzer_irq_lock);
	wakeup_source_init(&buzzer_ring_wake_lock_attach, "buzzer_ring_attach");

	if (!np) {
		dev_err(dev, "%s: no of node found\n", __func__);
		rc = -EINVAL;
		goto err;
	}

	rc = pwm_buzzer_pin_initialize(dev);
	if (rc) {
		dev_err(dev, "pwm_buzzer_pinctrl_initialize fail\n");
		goto err;
	}

	rc = sysfs_rc = devm_led_classdev_register(dev, &led_buzzer);
	if (rc) {
		dev_err(dev, "could not create sysfs\n");
		goto err;
	}

	buzzer_err_det_gpio = of_get_named_gpio(np, "kc,buzzer_det-gpio", 0);
	if (buzzer_err_det_gpio < 0) {
		pr_err("%s: Failed to get %s (gpio no.%d)\n",__func__, np->full_name, buzzer_err_det_gpio);
		rc = -EINVAL;
		goto err;
	}
	dev_info(dev, "%s: buzzer_det-gpio:%d val:%d\n", __func__, buzzer_err_det_gpio, gpio_get_value(buzzer_err_det_gpio));

	data->input_dev = input_allocate_device();
	if (!data->input_dev) {
		pr_err("%s: Failed to allocate input_dev memory\n",__func__);
		goto input_dev_free;
	}
	data->input_dev->name = "pwm buzzer error";
	data->input_dev->phys = "BUZZER";
	input_set_capability(data->input_dev, EV_SW, SW_KC_BUZZER_ERROR);
	rc = input_register_device(data->input_dev);
	if (rc) {
		pr_err("%s: Failed to regist input_dev\n",__func__);
		goto input_dev_free;
	}

	INIT_DELAYED_WORK(&data->buzzer_det_dwork, pwm_buzzer_error_detection);

	rc = request_irq(gpio_to_irq(buzzer_err_det_gpio), pwm_buzzer_det_handler, IRQF_TRIGGER_RISING, "buzzer state change", dev);
	if (rc) {
		pr_err("%s: Failed to request buzzer irq  %d\n", __func__, gpio_to_irq(buzzer_err_det_gpio));
		goto err_detect_irq;
	}

	buzzer_err_state = true;
	schedule_delayed_work(&data->buzzer_det_dwork, 0);
	pwm_buzzer_set_irq(data, 1);

	INIT_DELAYED_WORK(&data->buzzer_ring_dwork, pwm_buzzer_ring_control);

	pdata = data;

	dev_info(dev, "%s: ok\n", __func__);

	return 0;

err_detect_irq:
	gpio_free(buzzer_err_det_gpio);

input_dev_free:
	input_free_device(data->input_dev);

err:
	dev_err(dev, "%s: ng %d\n", __func__, rc);

	if (!sysfs_rc) {
		devm_led_classdev_unregister(dev, &led_buzzer);
	}
	mutex_destroy(&lock_sysfs);
	mutex_destroy(&buzzer_irq_lock);

	return rc;
}

static int32_t pwm_buzzer_remove(struct platform_device *pdev)
{
	struct pwm_buzzer_data *data = dev_get_drvdata(&pdev->dev);

	dev_info(data->dev, "%s\n", __func__);

	atomic_set(&data->shutdown, 1);
	cancel_delayed_work_sync(&data->buzzer_ring_dwork);
	cancel_delayed_work_sync(&data->buzzer_det_dwork);
	free_irq(gpio_to_irq(buzzer_err_det_gpio), data);
	gpio_free(buzzer_err_det_gpio);
	mutex_destroy(&lock_sysfs);
	mutex_destroy(&buzzer_irq_lock);
	wakeup_source_trash(&buzzer_ring_wake_lock_attach);
	devm_led_classdev_unregister(&pdev->dev, &led_buzzer);
	if (data->pin) {
		devm_pinctrl_put(data->pin);
		data->pin = NULL;
	}
	if(!data->input_dev)
		input_free_device(data->input_dev);
	return 0;
}

static void pwm_buzzer_shutdown(struct platform_device *pdev)
{
	struct pwm_buzzer_data *data = dev_get_drvdata(&pdev->dev);

	dev_info(data->dev, "%s\n", __func__);

	atomic_set(&data->shutdown, 1);
	cancel_delayed_work_sync(&data->buzzer_ring_dwork);
	cancel_delayed_work_sync(&data->buzzer_det_dwork);
	free_irq(gpio_to_irq(buzzer_err_det_gpio), data);
	gpio_free(buzzer_err_det_gpio);
	mutex_destroy(&lock_sysfs);
	mutex_destroy(&buzzer_irq_lock);
	if(!data->input_dev)
		input_free_device(data->input_dev);
}

static const struct of_device_id pwm_buzzer_of_match[] = {
	{ .compatible = "kc,pwm_buzzer", },
	{}
};
MODULE_DEVICE_TABLE(of, pwm_buzzer_of_match);

static struct platform_driver pwm_buzzer_driver = {
	.driver = {
		.name	= "pwm_buzzer",
		.owner	= THIS_MODULE,
		.of_match_table = pwm_buzzer_of_match,
	},
	.probe	= pwm_buzzer_probe,
	.remove	= pwm_buzzer_remove,
	.shutdown	= pwm_buzzer_shutdown,
};

static int32_t pwm_buzzer_init(void)
{
	int32_t rc = platform_driver_register(&pwm_buzzer_driver);

	if (!rc)
		pr_info("%s OK\n", __func__);
	else
		pr_err("%s %d\n", __func__, rc);

	return rc;
}

static void pwm_buzzer_exit(void)
{
	pr_info("%s\n", __func__);
	platform_driver_unregister(&pwm_buzzer_driver);
}

module_init(pwm_buzzer_init);
module_exit(pwm_buzzer_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("pwm_buzzer");
MODULE_ALIAS("pwm_buzzer");

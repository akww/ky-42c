/*
 * This software is contributed or developed by KYOCERA Corporation.
 * (C) 2015 KYOCERA Corporation
 * (C) 2019 KYOCERA Corporation
 * (C) 2020 KYOCERA Corporation
 * (C) 2021 KYOCERA Corporation
 * (C) 2022 KYOCERA Corporation
 */
/* include/asm/mach-msm/htc_pwrsink.h
 *
 * Copyright (C) 2008 HTC Corporation.
 * Copyright (C) 2007 Google, Inc.
 * Copyright (c) 2011 Code Aurora Forum. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
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

#include <linux/delay.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/gpio.h>
#include <linux/hrtimer.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/ktime.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/workqueue.h>
#include <linux/regulator/consumer.h>
#include <linux/of_gpio.h>
#include "../staging/android/timed_output.h"
#include <linux/pinctrl/pinctrl.h>

#ifndef __devinit
  #define __devinit
  #define __devexit
#endif/* __devinit */
#ifndef __devexit_p
  #define __devexit_p(f)	f
#endif/* __devexit_p */

enum vib_status
{
	VIB_STANDBY,
	VIB_ON,
	VIB_OFF,
};
enum vib_add_time_flag
{
	VIB_ADD_TIME_FLAG_OFF,
	VIB_ADD_TIME_FLAG_ON,
};

#define DEBUG_VIB_DRV2604				1

#define VIB_DRV_NAME					"DRV2604"
#define VIB_ON_WORK_NUM					(5)
#define I2C_RETRIES_NUM					(5)
#define I2C_WRITE_MSG_NUM				(1)
#define I2C_READ_MSG_NUM				(2)
#define VIB_STANDBY_DELAY_TIME			(15)
#define VIB_TIME_MIN					(25)
#define VIB_TIME_MAX					(15000)

int g_gpio_vib_en = -1;
int g_gpio_vib_intrig = -1;
#define MSMGPIO_LIMTR_EN				g_gpio_vib_en
#define MSMGPIO_LIMTR_INTRIG			g_gpio_vib_intrig

#define VIB_I2C_EN_DELAY				(250)	/* us */

#define VIB_HAPTICS_ON  (1)
#define VIB_HAPTICS_OFF (0)

#define HAPTICS_TIME_10         10  /* 10ms */
#define HAPTICS_TIME_20         20  /* 20ms */
#define HAPTICS_TIME_30         30  /* 30ms */
#define HAPTICS_TIME_40         40  /* 40ms */
#define HAPTICS_TIME_50         50  /* 50ms */
#define HAPTICS_TIME_60         60  /* 60ms */
#define HAPTICS_TIME_70         70  /* 70ms */
#define HAPTICS_TIME_80         80  /* 80ms */
#define HAPTICS_TIME_90         90  /* 90ms */
#define HAPTICS_TIME_100        100 /* 100ms */

#define SENSOR_VIB_INTERLOCKING

struct vib_on_work_data
{
	struct work_struct	work_vib_on;
	int					time;
};
struct drv2604_work_data {
	struct vib_on_work_data vib_on_work_data[VIB_ON_WORK_NUM];
	struct work_struct work_vib_off;
	struct work_struct work_vib_standby;
};
struct drv2604_data_t {
	struct i2c_client *drv2604_i2c_client;
	struct hrtimer vib_off_timer;
	struct hrtimer vib_standby_timer;
	int work_vib_on_pos;
	enum vib_status vib_cur_status;
	enum vib_add_time_flag add_time_flag;
};

static atomic_t haptics;

static struct mutex vib_mutex;
static struct mutex vib_en_mutex;
static u8 write_buf[2] = {0x00, 0x00};
static u8 read_buf[4] = {0x00, 0x00, 0x00, 0x00};
struct drv2604_work_data drv2604_work;
struct drv2604_data_t drv2604_data;
void _vib_gpio_set_value(int g, int v, int verify);

#define	VIB_AMPLITUDE_NUM	9
#define	VIB_AMPLITUDE_DEF	(VIB_AMPLITUDE_NUM - 1)

static int	vib_amplitude = VIB_AMPLITUDE_DEF;
static int	vib_curr_amplitude = VIB_AMPLITUDE_DEF;

struct vib_pow_data {
	u32 rated[VIB_AMPLITUDE_NUM];
	u32 clamp[VIB_AMPLITUDE_NUM];
};

struct reg_setting_data {
	struct vib_pow_data vib_pow;
	u32 vib_comp;
	u32 vib_bemf;
	u32 vib_gain;
	u32 vib_drivetime;
	u32 vib_blank_idiss;
	u32 vib_ext_blank_idiss;
	u32 vib_autocal_gain;
};

static struct reg_setting_data drv2604_reg_setting;
static struct reg_setting_data drv2604l_reg_setting;

#define VIB_DEVICE_ID_DRV2604    4
#define VIB_DEVICE_ID_DRV2604L   6

static int g_vib_device_id = VIB_DEVICE_ID_DRV2604L;

#define VIB_LOG(md, fmt, ... ) \
printk(md "[VIB] %s(%d): " fmt, __func__, __LINE__, ## __VA_ARGS__)
#if DEBUG_VIB_DRV2604
#define VIB_DEBUG_LOG(md, fmt, ... ) \
printk(md "[VIB] %s(%d): " fmt, __func__, __LINE__, ## __VA_ARGS__)
#define vib_gpio_set_value(g, v)	_vib_gpio_set_value(g, v, 1)
#else
#define VIB_DEBUG_LOG(md, fmt, ... )
#define vib_gpio_set_value(g, v)	_vib_gpio_set_value(g, v, 0)
#endif /* DEBUG_VIB_DRV2604 */

#define debugk(fmt,...)	\
printk("%s(%d)[0x%p]:" fmt, __func__, __LINE__, __builtin_return_address(0), ##__VA_ARGS__)

#define VIB_SET_REG(reg, data) { \
	write_buf[0] = (u8)(reg); \
	write_buf[1] = (u8)(data); \
	drv2604_i2c_write_data(drv2604_data.drv2604_i2c_client, write_buf, sizeof(write_buf)); }

#define VIB_GET_REG(reg) \
	drv2604_i2c_read_data(drv2604_data.drv2604_i2c_client, reg, read_buf, 1)

struct pinctrl *pinctrl_vib;
struct pinctrl_state *vib_en, *vib_disen, *vib_trig, *vib_distrig;

#ifdef SENSOR_VIB_INTERLOCKING
static int drv2604_get_vib_time(struct timed_output_dev *dev);
static void drv2604_enable(struct timed_output_dev *dev, int value);

static struct timed_output_dev drv2604_output_dev = {
	.name = "vibrator",
	.get_time = drv2604_get_vib_time,
	.enable = drv2604_enable,
};
#endif

int g_voltage_rated = 0;
int g_voltage_clamp = 0;
#define VOLTAGE_MAX_RATED	0x73
#define VOLTAGE_MAX_CLAMP	0x88
#define SET_VOLTAGE_MODE_VALUE	10000

int g_vib_autocal = 0;
bool g_vib_set_en_high = false;
bool g_vib_initialized = false;

static int drv2604_i2c_write_data(struct i2c_client *client, u8 *buf, u16 len)
{
	int ret = 0;
	int retry = 0;
	struct i2c_msg msg[I2C_WRITE_MSG_NUM];

	if (client == NULL || buf == NULL)
	{
		VIB_LOG(KERN_ERR, "client=0x%08x,buf=0x%08x\n",
				(unsigned int)client, (unsigned int)buf);
		return 0;
	}

	msg[0].addr = client->addr;
	msg[0].flags = 0;
	msg[0].len = len;
	msg[0].buf = buf;

	do
	{
		ret = i2c_transfer(client->adapter, msg, I2C_WRITE_MSG_NUM);
		VIB_DEBUG_LOG(KERN_INFO, "i2c_transfer(write) ret=%d\n", ret);
	} while ((ret != I2C_WRITE_MSG_NUM) && (++retry < I2C_RETRIES_NUM));

	if (ret != I2C_WRITE_MSG_NUM)
	{
		ret = -1;
		pr_err("VIB: i2c write error (try:%d)\n", retry);
	}
	else
	{
		ret = 0;
	}

	return ret;
}

static int drv2604_i2c_read_data(struct i2c_client *client, u8 reg, u8 *buf, u16 len)
{
	int ret = 0;
	int retry = 0;
	u8 start_reg = 0;
	struct i2c_msg msg[I2C_READ_MSG_NUM];

	if (client == NULL || buf == NULL)
	{
		VIB_LOG(KERN_ERR, "client=0x%08x\n",
				(unsigned int)client);
		return 0;
	}

	msg[0].addr = client->addr;
	msg[0].flags = 0;
	msg[0].len = 1;
	msg[0].buf = &start_reg;
	start_reg = reg;

	msg[1].addr = client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = len;
	msg[1].buf = buf;

	do
	{
		ret = i2c_transfer(client->adapter, msg, I2C_READ_MSG_NUM);
	} while ((ret != I2C_READ_MSG_NUM) && (++retry < I2C_RETRIES_NUM));

	if (ret != I2C_READ_MSG_NUM)
	{
		ret = -1;
		pr_err("VIB: i2c read error (try:%d)\n", retry);
	}
	else
	{
		ret = 0;
	}

	return ret;
}

/*
 * Initialization Procedure
 */
static void drv2604_initialization(void)
{
	VIB_DEBUG_LOG(KERN_INFO, "called.\n");

	memset(read_buf,  0x00, sizeof(read_buf));
	memset(write_buf, 0x00, sizeof(write_buf));

	/* After power up, wait at least 250us before the DRV2604 will accept I2C commands. */
	udelay(VIB_I2C_EN_DELAY);

	/* Assert the EN pin (logic high). The EN pin may be asserted any time during or after the 250us wait period. */
	//vib_gpio_set_value(MSMGPIO_LIMTR_EN, 1);
	pinctrl_select_state(pinctrl_vib, vib_en);
	udelay(VIB_I2C_EN_DELAY);
	g_vib_set_en_high = true;

	/* DEVICE_ID Read */
	if (VIB_GET_REG(0x00) != 0) {
		VIB_DEBUG_LOG(KERN_ERR, "DEVICE_ID Read Error.\n");
		return;
	}
	g_vib_device_id = (read_buf[0] >> 5) & 0x07;
	VIB_DEBUG_LOG(KERN_INFO, "DEVICE_ID=%d\n", g_vib_device_id);

	if (g_vib_device_id == VIB_DEVICE_ID_DRV2604) {
		VIB_SET_REG(0x01, 0x00);
		VIB_SET_REG(0x16, drv2604_reg_setting.vib_pow.rated[VIB_AMPLITUDE_DEF]);
		VIB_SET_REG(0x17, drv2604_reg_setting.vib_pow.clamp[VIB_AMPLITUDE_DEF]);
		VIB_SET_REG(0x18, drv2604_reg_setting.vib_comp);
		VIB_SET_REG(0x19, drv2604_reg_setting.vib_bemf);
		VIB_SET_REG(0x1a, drv2604_reg_setting.vib_gain);
		VIB_SET_REG(0x1b, drv2604_reg_setting.vib_drivetime);
		VIB_SET_REG(0x1c, drv2604_reg_setting.vib_blank_idiss);
		VIB_SET_REG(0x1d, 0x84);
		VIB_SET_REG(0x03, 0x00);
		VIB_SET_REG(0x01, 0x03);
	} else {
		/* DRV2604L Setting */
		VIB_SET_REG(0x01, 0x00);
		VIB_SET_REG(0x16, drv2604l_reg_setting.vib_pow.rated[VIB_AMPLITUDE_DEF]);
		VIB_SET_REG(0x17, drv2604l_reg_setting.vib_pow.clamp[VIB_AMPLITUDE_DEF]);
		VIB_SET_REG(0x18, drv2604l_reg_setting.vib_comp);
		VIB_SET_REG(0x19, drv2604l_reg_setting.vib_bemf);
		VIB_SET_REG(0x1a, drv2604l_reg_setting.vib_gain);
		VIB_SET_REG(0x1b, drv2604l_reg_setting.vib_drivetime);
		VIB_SET_REG(0x1c, drv2604l_reg_setting.vib_blank_idiss);
		VIB_SET_REG(0x1d, 0x84);
		VIB_SET_REG(0x1f, drv2604l_reg_setting.vib_ext_blank_idiss);
		VIB_SET_REG(0x03, 0x00);
		VIB_SET_REG(0x01, 0x03);
	}

	//vib_gpio_set_value(MSMGPIO_LIMTR_EN, 0);
	pinctrl_select_state(pinctrl_vib, vib_disen);
	g_vib_set_en_high = false;

	vib_curr_amplitude = VIB_AMPLITUDE_DEF;
	g_vib_initialized = true;

	VIB_DEBUG_LOG(KERN_INFO, "end.\n");
}

static bool drv2604_vib_chk_hapticstime(int value)
{
#if 1
	if (value > 0 &&
	    value <= HAPTICS_TIME_100)
	{
		return true;
	}
#else
	if (value > HAPTICS_TIME_10 &&
	    value <= HAPTICS_TIME_100)
	{
		return true;
	}
#endif

	return false;
}

static int drv2604_vib_set_hapticstime(int value)
{
#if 1
#else
	if (value > HAPTICS_TIME_10 &&
	    value <= HAPTICS_TIME_40)
	{
		value = value + HAPTICS_TIME_40;
	}
	else if (value > HAPTICS_TIME_40 &&
	         value <= HAPTICS_TIME_60)
	{
		value = value + HAPTICS_TIME_30;
	}
	else if (value > HAPTICS_TIME_60 &&
	         value <= HAPTICS_TIME_80)
	{
		value = value + HAPTICS_TIME_20;
	}
	else if (value > HAPTICS_TIME_80 &&
	         value <= HAPTICS_TIME_100)
	{
		value = value + HAPTICS_TIME_10;
	}
#endif

	return value;
}

static int drv2604_vib_haptics(int value)
{
	/* range confirmation of haptics time */
	if (drv2604_vib_chk_hapticstime(value))
	{
		/* haptics flg on */
		atomic_set(&haptics, VIB_HAPTICS_ON);
		/* add haptics time */
		value = drv2604_vib_set_hapticstime(value);
	} else {
		/* haptics flg off */
		atomic_set(&haptics, VIB_HAPTICS_OFF);
	}

	return value;
}

static void drv2604_set_vib_amplitude(int level)
{
	if ((level >= 0) && (level <= VIB_AMPLITUDE_NUM - 1))
	{
		mutex_lock(&vib_en_mutex);
		if (g_vib_set_en_high == false) {
			/* EN pin "High" */
			pinctrl_select_state(pinctrl_vib, vib_en);
			udelay(VIB_I2C_EN_DELAY);
		}

		if (g_vib_device_id == VIB_DEVICE_ID_DRV2604) {
			VIB_SET_REG(0x16, drv2604_reg_setting.vib_pow.rated[level]);
			VIB_SET_REG(0x17, drv2604_reg_setting.vib_pow.clamp[level]);
		} else {
			/* DRV2604L Setting */
			VIB_SET_REG(0x16, drv2604l_reg_setting.vib_pow.rated[level]);
			VIB_SET_REG(0x17, drv2604l_reg_setting.vib_pow.clamp[level]);
		}

		if (g_vib_set_en_high == false) {
			/* EN pin "Low" */
			pinctrl_select_state(pinctrl_vib, vib_disen);
		}
		mutex_unlock(&vib_en_mutex);

		pr_info("VIB: amplitude=%d\n", level);
	}
	else
	{
		pr_err("VIB: amplitude parameter [%d] is out of range.\n", level);
	}

	return;
}

#ifdef SENSOR_VIB_INTERLOCKING
static void drv2604_vib_send_uevent(struct device *dev, enum vib_status status)
{
	char event_string[20];
	char *envp[] = { event_string, NULL };

	if (!dev) {
		dev_err(dev, "dev NULL\n");
		return;
	}

	if ((status != VIB_ON) && (status != VIB_OFF)) {
		return;
	}

	if (status == VIB_ON)
		sprintf(event_string, "KC_VIB=ON");
	else
		sprintf(event_string, "KC_VIB=OFF");

	kobject_uevent_env(&dev->kobj, KOBJ_CHANGE, envp);

	pr_debug("%s: dev = 0x%lx, &dev->kobj = 0x%lx\n", __func__,
				(unsigned long)dev, (unsigned long)&dev->kobj);
	return;
}
#endif

static void drv2604_set_vib(enum vib_status status, int time)
{
	enum vib_status cur_status = drv2604_data.vib_cur_status;

	VIB_DEBUG_LOG(KERN_INFO, "called. status=%d,time=%d,cur_status=%d\n",
							  status, time, cur_status);
	mutex_lock(&vib_mutex);

	switch (status) {
		case VIB_ON:
			VIB_DEBUG_LOG(KERN_INFO, "VIB_ON\n");
			/* STANDBY => ON */
			if (cur_status == VIB_STANDBY)
			{
				/* enable */
				mutex_lock(&vib_en_mutex);
				//vib_gpio_set_value(MSMGPIO_LIMTR_EN, 1);
				pinctrl_select_state(pinctrl_vib, vib_en);
				udelay(VIB_I2C_EN_DELAY);
				g_vib_set_en_high = true;
				mutex_unlock(&vib_en_mutex);
			}
			else
			{
				VIB_DEBUG_LOG(KERN_INFO, "VIB_ON standby cancel skip.\n");
			}

			/* OFF/STANDBY => ON */
			if (cur_status != VIB_ON)
			{
				/* start vibrator */
				//vib_gpio_set_value(MSMGPIO_LIMTR_INTRIG, 1);
				pinctrl_select_state(pinctrl_vib, vib_trig);

#ifdef SENSOR_VIB_INTERLOCKING
				drv2604_vib_send_uevent(drv2604_output_dev.dev, status);
#endif
			}
			else
			{
				VIB_DEBUG_LOG(KERN_INFO, "VIB_ON skip.\n");
			}
			/* Set vib off timer (ON => OFF) */
			VIB_DEBUG_LOG(KERN_INFO, "hrtimer_start(vib_off_timer). time=%d\n", time);
			hrtimer_start(&drv2604_data.vib_off_timer,
							ktime_set(time / 1000, 
							(time % 1000) * 1000000),
							HRTIMER_MODE_REL);

			drv2604_data.vib_cur_status = status;
			VIB_DEBUG_LOG(KERN_INFO, "set cur_status=%d\n", drv2604_data.vib_cur_status);
			break;
		case VIB_OFF:
			VIB_DEBUG_LOG(KERN_INFO, "VIB_OFF\n");
			/* ON => OFF */
			if (cur_status == VIB_ON)
			{
				/* stop vibrator */
				//vib_gpio_set_value(MSMGPIO_LIMTR_INTRIG, 0);
				pinctrl_select_state(pinctrl_vib, vib_distrig);
				drv2604_data.vib_cur_status = status;
				VIB_DEBUG_LOG(KERN_INFO, "set cur_status=%d\n", drv2604_data.vib_cur_status);

				/* Set vib standby timer (OFF => STANDBY) */
				VIB_DEBUG_LOG(KERN_INFO, "hrtimer_start(vib_standby_timer).\n");
				hrtimer_start(&drv2604_data.vib_standby_timer,
								ktime_set(VIB_STANDBY_DELAY_TIME / 1000,
								(VIB_STANDBY_DELAY_TIME % 1000) * 1000000),
								HRTIMER_MODE_REL);

#ifdef SENSOR_VIB_INTERLOCKING
				drv2604_vib_send_uevent(drv2604_output_dev.dev, status);
#endif
			}
			else
			{
				VIB_DEBUG_LOG(KERN_INFO, "VIB_OFF skip.\n");
			}
			break;
		case VIB_STANDBY:
			VIB_DEBUG_LOG(KERN_INFO, "VIB_STANDBY\n");
			/* OFF => STANDBY */
			if (cur_status == VIB_OFF)
			{
				/* disable */
				mutex_lock(&vib_en_mutex);
				//vib_gpio_set_value(MSMGPIO_LIMTR_EN, 0);
				pinctrl_select_state(pinctrl_vib, vib_disen);
				g_vib_set_en_high = false;
				mutex_unlock(&vib_en_mutex);
				drv2604_data.vib_cur_status = status;
				VIB_DEBUG_LOG(KERN_INFO, "set cur_status=%d\n", drv2604_data.vib_cur_status);
			}
			else
			{
				VIB_DEBUG_LOG(KERN_INFO, "VIB_STANDBY skip.\n");
			}
			break;
		default:
			VIB_LOG(KERN_ERR, "parameter error. status=%d\n", status);
			break;
	}
	mutex_unlock(&vib_mutex);
	return;
}

static void drv2604_vib_on(struct work_struct *work)
{
	struct vib_on_work_data *work_data = container_of
										(work, struct vib_on_work_data, work_vib_on);

	VIB_DEBUG_LOG(KERN_INFO, "called. work=0x%08x\n", (unsigned int)work);
	VIB_DEBUG_LOG(KERN_INFO, "work_data=0x%08x,time=%d\n",
					(unsigned int)work_data, work_data->time);
	drv2604_set_vib(VIB_ON, work_data->time);

	return;
}

static void drv2604_vib_off(struct work_struct *work)
{
	VIB_DEBUG_LOG(KERN_INFO, "called. work=0x%08x\n", (unsigned int)work);
	drv2604_set_vib(VIB_OFF, 0);
	return;
}

static void drv2604_vib_standby(struct work_struct *work)
{
	VIB_DEBUG_LOG(KERN_INFO, "called. work=0x%08x\n", (unsigned int)work);

	drv2604_set_vib(VIB_STANDBY, 0);
	return;
}

static void drv2604_timed_vib_on(struct timed_output_dev *dev, int timeout_val)
{
	int ret = 0;

	VIB_DEBUG_LOG(KERN_INFO, "called. dev=0x%08x, timeout_val=%d\n",
					(unsigned int)dev, timeout_val);
	drv2604_work.vib_on_work_data[drv2604_data.work_vib_on_pos].time = timeout_val;

	ret = schedule_work
			(&(drv2604_work.vib_on_work_data[drv2604_data.work_vib_on_pos].work_vib_on));
	if (ret != 0)
	{
		drv2604_data.work_vib_on_pos++;
		if (drv2604_data.work_vib_on_pos >= VIB_ON_WORK_NUM) {
			drv2604_data.work_vib_on_pos = 0;
		}
		VIB_DEBUG_LOG(KERN_INFO, "schedule_work(). work_vib_on_pos=%d\n",
			drv2604_data.work_vib_on_pos);
		VIB_DEBUG_LOG(KERN_INFO, "vib_on_work_data[%d].time=%d\n",
			drv2604_data.work_vib_on_pos,
			drv2604_work.vib_on_work_data[drv2604_data.work_vib_on_pos].time);
	}
	return;
}

static void drv2604_timed_vib_off(struct timed_output_dev *dev)
{
	int ret = 0;

	VIB_DEBUG_LOG(KERN_INFO, "called. dev=0x%08x\n", (unsigned int)dev);
	ret = schedule_work(&drv2604_work.work_vib_off);
	if (ret == 0)
	{
		VIB_LOG(KERN_ERR, "schedule_work error. ret=%d\n",ret);
	}
	return;
}

static void drv2604_timed_vib_standby(struct timed_output_dev *dev)
{
	int ret = 0;

	VIB_DEBUG_LOG(KERN_INFO, "called. dev=0x%08x\n", (unsigned int)dev);
	ret = schedule_work(&drv2604_work.work_vib_standby);
	if (ret == 0)
	{
		VIB_LOG(KERN_ERR, "schedule_work error. ret=%d\n",ret);
	}
	return;
}

static void drv2604_enable(struct timed_output_dev *dev, int value)
{
	VIB_DEBUG_LOG(KERN_INFO, "called. dev=0x%08x,value=%d\n", (unsigned int)dev, value);
	VIB_DEBUG_LOG(KERN_INFO, "add_time_flag=%d\n", drv2604_data.add_time_flag);

	if (g_vib_initialized == false) {
		VIB_DEBUG_LOG(KERN_INFO, "DRV2604 not initialized.\n");
		return;
	}

	if ((value <= 0) && (drv2604_data.add_time_flag == VIB_ADD_TIME_FLAG_ON))
	{
		VIB_DEBUG_LOG(KERN_INFO, "skip. value=%d,add_time_flag=%d\n",
						value, drv2604_data.add_time_flag);
		return;
	}

	if (atomic_read(&haptics) && !(value))
	{
		atomic_set(&haptics, VIB_HAPTICS_OFF);
		return;
	}

	VIB_DEBUG_LOG(KERN_INFO, "hrtimer_cancel(vib_off_timer)\n");
	hrtimer_cancel(&drv2604_data.vib_off_timer);

	value = drv2604_vib_haptics(value);
	if (value <= 0)
	{
		drv2604_timed_vib_off(dev);
	}
	else
	{
		VIB_DEBUG_LOG(KERN_INFO, "hrtimer_cancel(vib_standby_timer)\n");
		hrtimer_cancel(&drv2604_data.vib_standby_timer);
#if 0
		if (value < VIB_TIME_MIN)
		{
			value = VIB_TIME_MIN;
			drv2604_data.add_time_flag = VIB_ADD_TIME_FLAG_ON;
			VIB_DEBUG_LOG(KERN_INFO, "set add_time_flag=%d\n", drv2604_data.add_time_flag);
		}
		else
#endif
		{
			drv2604_data.add_time_flag = VIB_ADD_TIME_FLAG_OFF;
			VIB_DEBUG_LOG(KERN_INFO, "set add_time_flag=%d\n", drv2604_data.add_time_flag);
		}
		drv2604_timed_vib_on(dev, value);
	}
	return;
}

static int drv2604_get_vib_time(struct timed_output_dev *dev)
{
	int ret = 0;

	VIB_DEBUG_LOG(KERN_INFO, "called. dev=0x%08x\n", (unsigned int)dev);
	mutex_lock(&vib_mutex);

	ret = hrtimer_active(&drv2604_data.vib_off_timer);
	if (ret != 0)
	{
		ktime_t r = hrtimer_get_remaining(&drv2604_data.vib_off_timer);
		struct timeval t = ktime_to_timeval(r);
		mutex_unlock(&vib_mutex);

		return t.tv_sec * 1000 + t.tv_usec / 1000;
	}
	mutex_unlock(&vib_mutex);
	return 0;
}

static enum hrtimer_restart drv2604_off_timer_func(struct hrtimer *timer)
{
	VIB_DEBUG_LOG(KERN_INFO, "called. timer=0x%08x\n", (unsigned int)timer);
	drv2604_data.add_time_flag = VIB_ADD_TIME_FLAG_OFF;
	VIB_DEBUG_LOG(KERN_INFO, "set add_time_flag=%d\n", drv2604_data.add_time_flag);

	drv2604_timed_vib_off(NULL);
	return HRTIMER_NORESTART;
}

static enum hrtimer_restart drv2604_standby_timer_func(struct hrtimer *timer)
{
	VIB_DEBUG_LOG(KERN_INFO, "called. timer=0x%08x\n", (unsigned int)timer);
	drv2604_timed_vib_standby(NULL);
	return HRTIMER_NORESTART;
}

#ifndef SENSOR_VIB_INTERLOCKING
static struct timed_output_dev drv2604_output_dev = {
	.name = "vibrator",
	.get_time = drv2604_get_vib_time,
	.enable = drv2604_enable,
};
#endif

static ssize_t amplitude_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n", vib_amplitude);
}

static ssize_t amplitude_store(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf, size_t size)
{
	int res;
	long tmp;

	res = kstrtoul(buf, 0, &tmp);
	if (res < 0)
		return -1;

	if (g_vib_initialized == false) {
		VIB_DEBUG_LOG(KERN_INFO, "DRV2604 not initialized.\n");
		return size;
	}

	if ((tmp >= 1) && (tmp <= 255)) {
		vib_amplitude = (int)((tmp - 1) * VIB_AMPLITUDE_NUM / 255);
		if (vib_amplitude != vib_curr_amplitude) {
			drv2604_set_vib_amplitude(vib_amplitude);
			vib_curr_amplitude = vib_amplitude;
		}
	} else if (tmp == SET_VOLTAGE_MODE_VALUE) {
		mutex_lock(&vib_en_mutex);
		if (g_vib_set_en_high == false) {
			/* EN pin "High" */
			pinctrl_select_state(pinctrl_vib, vib_en);
			udelay(VIB_I2C_EN_DELAY);
		}

		VIB_SET_REG(0x16, g_voltage_rated);
		VIB_SET_REG(0x17, g_voltage_clamp);

		if (g_vib_set_en_high == false) {
			/* EN pin "Low" */
			pinctrl_select_state(pinctrl_vib, vib_disen);
		}
		mutex_unlock(&vib_en_mutex);

		vib_curr_amplitude = SET_VOLTAGE_MODE_VALUE;

		pr_info("VIB: set voltage mode: rated=0x%x, clamp=0x%x\n", g_voltage_rated, g_voltage_clamp);
	}

	VIB_DEBUG_LOG(KERN_DEBUG, "receive amplitude=%d\n", (int)tmp);

	return size;
}

static DEVICE_ATTR(amplitude, S_IRUGO | S_IWUSR, amplitude_show, amplitude_store);

static ssize_t voltage_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "0x%x, 0x%x\n", g_voltage_rated, g_voltage_clamp);
}

static ssize_t voltage_store(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf, size_t size)
{
	int res;
	long tmp;

	res = kstrtoul(buf, 16, &tmp);
	if (res < 0)
		return -1;

	g_voltage_clamp = tmp & 0xff;
	if (g_voltage_clamp > VOLTAGE_MAX_CLAMP) {
		g_voltage_clamp = VOLTAGE_MAX_CLAMP;
	}

    g_voltage_rated = (tmp / (16 * 16)) & 0xff;
	if (g_voltage_rated > VOLTAGE_MAX_RATED) {
		g_voltage_rated = VOLTAGE_MAX_RATED;
	}

	VIB_DEBUG_LOG(KERN_DEBUG, "receive voltage:rated=0x%x, clamp=0x%x\n", g_voltage_rated, g_voltage_clamp);

	return size;
}

static DEVICE_ATTR(voltage, S_IRUGO | S_IWUSR, voltage_show, voltage_store);

static void drv2604_vib_auto_calibration(void)
{
	int cnt;
	u8  BEMFGain = 0x00;

	VIB_DEBUG_LOG(KERN_INFO, "auto calibration start.(DEVICE_ID=%d)\n", g_vib_device_id);

	if (g_vib_initialized == false) {
		VIB_DEBUG_LOG(KERN_INFO, "DRV2604 not initialized.\n");
		return;
	}

	/* EN pin "High" */
	pinctrl_select_state(pinctrl_vib, vib_en);
	udelay(VIB_I2C_EN_DELAY);
	g_vib_set_en_high = true;

	if (g_vib_device_id == VIB_DEVICE_ID_DRV2604) {
		VIB_SET_REG(0x01, 0x07);
		VIB_SET_REG(0x16, drv2604_reg_setting.vib_pow.rated[VIB_AMPLITUDE_DEF]);
		VIB_SET_REG(0x17, drv2604_reg_setting.vib_pow.clamp[VIB_AMPLITUDE_DEF]);
		VIB_SET_REG(0x1a, drv2604_reg_setting.vib_autocal_gain);
		VIB_SET_REG(0x1b, drv2604_reg_setting.vib_drivetime);
		VIB_SET_REG(0x1c, drv2604_reg_setting.vib_blank_idiss);
		VIB_SET_REG(0x1d, 0x84);
		VIB_SET_REG(0x1e, 0x20);
		VIB_SET_REG(0x0c, 0x01);
	} else {
		/* DRV2604L Setting */
		VIB_SET_REG(0x01, 0x07);
		VIB_SET_REG(0x16, drv2604l_reg_setting.vib_pow.rated[VIB_AMPLITUDE_DEF]);
		VIB_SET_REG(0x17, drv2604l_reg_setting.vib_pow.clamp[VIB_AMPLITUDE_DEF]);
		VIB_SET_REG(0x1a, drv2604l_reg_setting.vib_autocal_gain);
		VIB_SET_REG(0x1b, drv2604l_reg_setting.vib_drivetime);
		VIB_SET_REG(0x1c, drv2604l_reg_setting.vib_blank_idiss);
		VIB_SET_REG(0x1d, 0x84);
		VIB_SET_REG(0x1e, 0x20);
		VIB_SET_REG(0x1f, drv2604l_reg_setting.vib_ext_blank_idiss);
		VIB_SET_REG(0x0c, 0x01);
	}

	/* Wait 1000ms */
	msleep(1000);

	for (cnt = 0; cnt < 50; cnt++) {
		VIB_GET_REG(0x0c);
		if (!(read_buf[0] & 0x01))
			break;
		mdelay(10);
	}

	if (read_buf[0] & 0x01) {
		VIB_DEBUG_LOG(KERN_INFO, "auto calibration not complete.\n");
	} else {
		VIB_GET_REG(0x00);
		if (read_buf[0] & 0x08) {
			VIB_DEBUG_LOG(KERN_INFO, "auto calibration fail.\n");
		} else {
			VIB_DEBUG_LOG(KERN_INFO, "auto calibration success.\n");
		}
	}

	VIB_GET_REG(0x18);
	VIB_DEBUG_LOG(KERN_INFO, "A_CAL_COMP[0x18] D[7:0] (after AutoCal): 0x%x\n", (unsigned)read_buf[0]);
	VIB_GET_REG(0x19);
	VIB_DEBUG_LOG(KERN_INFO, "A_CAL_BEMF[0x19] D[7:0] (after AutoCal): 0x%x\n", (unsigned)read_buf[0]);
	VIB_GET_REG(0x1a);
	BEMFGain = (unsigned)(read_buf[0] & 0x03);
	VIB_DEBUG_LOG(KERN_INFO, "BEMF_GAIN[0x1a] D[1:0] (after AutoCal): 0x%x\n", BEMFGain);

	/* Restore Settings */
	if (g_vib_device_id == VIB_DEVICE_ID_DRV2604) {
		VIB_SET_REG(0x1a, (drv2604_reg_setting.vib_gain & 0xfc) | BEMFGain);
	} else {
		/* DRV2604L Setting */
		VIB_SET_REG(0x1a, (drv2604l_reg_setting.vib_gain & 0xfc) | BEMFGain);
	}
	VIB_SET_REG(0x01, 0x03);

	/* EN pin "Low" */
	pinctrl_select_state(pinctrl_vib, vib_disen);
	g_vib_set_en_high = false;

	VIB_DEBUG_LOG(KERN_INFO, "auto calibration end.\n");
}

static ssize_t autocal_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n", g_vib_autocal);
}

static ssize_t autocal_store(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf, size_t size)
{
	int res;
	long tmp;

	res = kstrtoul(buf, 0, &tmp);
	if (res < 0)
		return -1;

	g_vib_autocal = (int)tmp;
	if (g_vib_autocal == 1) {
		drv2604_vib_auto_calibration();
	}

	VIB_DEBUG_LOG(KERN_DEBUG, "receive autocal=%d\n", (int)tmp);

	return size;
}

static DEVICE_ATTR(autocal, S_IRUGO | S_IWUSR, autocal_show, autocal_store);

int vib_get_gpio_info(struct device *dev)
{
	int ret;

	pinctrl_vib = devm_pinctrl_get(dev);
	if (IS_ERR(pinctrl_vib)) {
		ret = PTR_ERR(pinctrl_vib);
		pr_err("%s Couldn't find pinctrl_vib!\n", __func__);
		return ret;
	}
	vib_en = pinctrl_lookup_state(pinctrl_vib, "vib_enable_pin");
	if (IS_ERR(vib_en)) {
		ret = PTR_ERR(vib_en);
		pr_err("%s Cannot find pinctrl vib_en!\n", __func__);
		return ret;
	}

	vib_disen = pinctrl_lookup_state(pinctrl_vib, "vib_disable_pin");
	if (IS_ERR(vib_disen)) {
		ret = PTR_ERR(vib_disen);
		pr_err("%s Cannot find pinctrl vib_disen!\n", __func__);
		return ret;
	}

	vib_trig = pinctrl_lookup_state(pinctrl_vib, "vib_trig_pin");
	if (IS_ERR(vib_trig)) {
		ret = PTR_ERR(vib_trig);
		pr_err("%s Cannot find pinctrl vib_trig!\n", __func__);
		return ret;
	}

	vib_distrig = pinctrl_lookup_state(pinctrl_vib, "vib_distrig_pin");
	if (IS_ERR(vib_distrig)) {
		ret = PTR_ERR(vib_distrig);
		pr_err("%s Cannot find pinctrl vib_en!\n", __func__);
		return ret;
	}

	return 0;
}

static int __devinit drv2604_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int ret = 0;
	int count = 0;

	VIB_DEBUG_LOG(KERN_INFO, "called. id=0x%08x\n", (unsigned int)id);
	drv2604_data.drv2604_i2c_client = client;
	drv2604_data.vib_cur_status = VIB_STANDBY;
	drv2604_data.add_time_flag = VIB_ADD_TIME_FLAG_OFF;

	atomic_set(&haptics, VIB_HAPTICS_OFF);

	memset(&drv2604_reg_setting, 0x00, sizeof drv2604_reg_setting);
	memset(&drv2604l_reg_setting, 0x00, sizeof drv2604l_reg_setting);

	if (client->dev.of_node)
	{
		/* for DRV2604 Reg Setting */
		of_property_read_u32_array(client->dev.of_node, "oem,drv2604-rated", drv2604_reg_setting.vib_pow.rated, VIB_AMPLITUDE_NUM);
		of_property_read_u32_array(client->dev.of_node, "oem,drv2604-clamp", drv2604_reg_setting.vib_pow.clamp, VIB_AMPLITUDE_NUM);
		of_property_read_u32_array(client->dev.of_node, "oem,drv2604-comp", &drv2604_reg_setting.vib_comp, 1);
		of_property_read_u32_array(client->dev.of_node, "oem,drv2604-bemf", &drv2604_reg_setting.vib_bemf, 1);
		of_property_read_u32_array(client->dev.of_node, "oem,drv2604-gain", &drv2604_reg_setting.vib_gain, 1);
		of_property_read_u32_array(client->dev.of_node, "oem,drv2604-drivetime", &drv2604_reg_setting.vib_drivetime, 1);
		of_property_read_u32_array(client->dev.of_node, "oem,drv2604-blank-idiss", &drv2604_reg_setting.vib_blank_idiss, 1);
		of_property_read_u32_array(client->dev.of_node, "oem,drv2604-autocal-gain", &drv2604_reg_setting.vib_autocal_gain, 1);

		/* for DRV2604L Reg Setting */
		of_property_read_u32_array(client->dev.of_node, "oem,drv2604l-rated", drv2604l_reg_setting.vib_pow.rated, VIB_AMPLITUDE_NUM);
		of_property_read_u32_array(client->dev.of_node, "oem,drv2604l-clamp", drv2604l_reg_setting.vib_pow.clamp, VIB_AMPLITUDE_NUM);
		of_property_read_u32_array(client->dev.of_node, "oem,drv2604l-comp", &drv2604l_reg_setting.vib_comp, 1);
		of_property_read_u32_array(client->dev.of_node, "oem,drv2604l-bemf", &drv2604l_reg_setting.vib_bemf, 1);
		of_property_read_u32_array(client->dev.of_node, "oem,drv2604l-gain", &drv2604l_reg_setting.vib_gain, 1);
		of_property_read_u32_array(client->dev.of_node, "oem,drv2604l-drivetime", &drv2604l_reg_setting.vib_drivetime, 1);
		of_property_read_u32_array(client->dev.of_node, "oem,drv2604l-blank-idiss", &drv2604l_reg_setting.vib_blank_idiss, 1);
		of_property_read_u32_array(client->dev.of_node, "oem,drv2604l-ext-blank-idiss", &drv2604l_reg_setting.vib_ext_blank_idiss, 1);
		of_property_read_u32_array(client->dev.of_node, "oem,drv2604l-autocal-gain", &drv2604l_reg_setting.vib_autocal_gain, 1);
	}

	ret = vib_get_gpio_info(&client->dev);
	if (ret != 0) {
		pr_err("%s get vibrator pin-ctrl fail\n", __func__);
		goto probe_dev_register_error;
	}

	mutex_init(&vib_mutex);
	mutex_init(&vib_en_mutex);

	for (count = 0; count < VIB_ON_WORK_NUM; count++)
	{
		INIT_WORK(&(drv2604_work.vib_on_work_data[count].work_vib_on),
					drv2604_vib_on);
		drv2604_work.vib_on_work_data[count].time = 0;
	}
	drv2604_data.work_vib_on_pos = 0;
	INIT_WORK(&drv2604_work.work_vib_off, drv2604_vib_off);
	INIT_WORK(&drv2604_work.work_vib_standby, drv2604_vib_standby);

	hrtimer_init(&drv2604_data.vib_off_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	drv2604_data.vib_off_timer.function = drv2604_off_timer_func;
	hrtimer_init(&drv2604_data.vib_standby_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	drv2604_data.vib_standby_timer.function = drv2604_standby_timer_func;

	ret = timed_output_dev_register(&drv2604_output_dev);
	VIB_DEBUG_LOG(KERN_INFO, "timed_output_dev_register() ret=%d\n", ret);
	if (ret != 0)
	{
		VIB_LOG(KERN_ERR, "timed_output_dev_register() ERROR ret=%d\n", ret);
		goto probe_dev_register_error;
	}
	VIB_DEBUG_LOG(KERN_INFO, "timed_output_dev_register() ok\n");

	/* add "amplitude" onto "/sys/class/timed_output/vibrator". */
	ret = device_create_file(drv2604_output_dev.dev, &dev_attr_amplitude);
	if (ret != 0)
	{
		pr_err("device_create_file(amplitude) ERROR ret=%d\n", ret);
		goto probe_dev_register_error;
	}

	/* add "voltage" onto "/sys/class/timed_output/vibrator". */
	ret = device_create_file(drv2604_output_dev.dev, &dev_attr_voltage);
	if (ret != 0)
	{
		pr_err("device_create_file(voltage) ERROR ret=%d\n", ret);
		goto probe_dev_register_error;
	}

	/* add "autocal" onto "/sys/class/timed_output/vibrator". */
	ret = device_create_file(drv2604_output_dev.dev, &dev_attr_autocal);
	if (ret != 0)
	{
		pr_err("device_create_file(autocal) ERROR ret=%d\n", ret);
		goto probe_dev_register_error;
	}

	drv2604_initialization();

	VIB_DEBUG_LOG(KERN_INFO, "drv2604_probe() end\n");

	return 0;

probe_dev_register_error:
	mutex_destroy(&vib_mutex);
	mutex_destroy(&vib_en_mutex);
	return ret;
}
/*
static int32_t __devexit drv2604_remove(struct i2c_client *pst_client)
{
	int ret = 0;

	VIB_DEBUG_LOG(KERN_INFO, "called. pst_client=0x%08x\n", (unsigned int)pst_client);

	timed_output_dev_unregister(&drv2604_output_dev);

	mutex_destroy(&vib_mutex);
	return ret;
}

static int32_t drv2604_suspend(struct i2c_client *pst_client, pm_message_t mesg)
{
	VIB_DEBUG_LOG(KERN_INFO, "called. pst_client=0x%08x,mesg=%d\n",
					(unsigned int)pst_client, mesg.event);
	VIB_DEBUG_LOG(KERN_INFO, "end.\n");
	return 0;
}

static int32_t drv2604_resume(struct i2c_client *pst_client)
{
	VIB_DEBUG_LOG(KERN_INFO, "called. pst_client=0x%08x\n", (unsigned int)pst_client);
	VIB_DEBUG_LOG(KERN_INFO, "end.\n");
	return 0;
}
*/

static struct i2c_device_id drv2604_idtable[] = {
	{VIB_DRV_NAME, 0},
	{ },
};
MODULE_DEVICE_TABLE(i2c, drv2604_idtable);

static struct of_device_id drv2604_of_match_table[] = {
        {
                .compatible = "DRV2604",
        },
        {},
};

static struct i2c_driver drv2604_driver = {
	.driver		= {
		.name	= VIB_DRV_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = drv2604_of_match_table,
	},
	.probe		= drv2604_probe,
	//.remove		= __devexit_p(drv2604_remove),
	//.suspend	= drv2604_suspend,
	//.resume		= drv2604_resume,
	.id_table	= drv2604_idtable,
};

static int __init drv2604_init(void)
{
	int ret = 0;

	VIB_DEBUG_LOG(KERN_INFO, "called.\n");
	ret = i2c_add_driver(&drv2604_driver);
	VIB_DEBUG_LOG(KERN_INFO, "i2c_add_driver() ret=%d\n", ret);
	if (ret != 0)
	{
		VIB_LOG(KERN_ERR, "i2c_add_driver() ret=%d\n", ret);
	}

	return ret;
}

static void __exit drv2604_exit(void)
{
	VIB_DEBUG_LOG(KERN_INFO, "called.\n");
	i2c_del_driver(&drv2604_driver);
	VIB_DEBUG_LOG(KERN_INFO, "i2c_del_driver()\n");

	return;
}

void _vib_gpio_set_value(int g, int v, int verify)
{
	int v2;

	if (g < 0) {
		debugk("gpio_set_value(%d,%d) failed.\n", g, v);
		return;
	}

	gpio_set_value(g, 0);

	if (!verify) {
		return;
	}

	v2 = gpio_get_value(g);

	if (v == v2) {
		debugk("gpio_set_value(%d,%d) ok.\n", g, v);
	}
	else {
		debugk("gpio_set_value(%d,%d) failed. v2=%d\n", g, v, v2);
	}
}

module_init(drv2604_init);
module_exit(drv2604_exit);
MODULE_DESCRIPTION("timed output DRV2604 vibrator device");
MODULE_LICENSE("GPL");

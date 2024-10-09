/*
 * This software is contributed or developed by KYOCERA Corporation.
 * (C) 2021 KYOCERA Corporation
 * (C) 2022 KYOCERA Corporation
 */
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

#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/input.h>
//#include <linux/wakelock.h>
#include "kc_leds_wakelock.h"
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <asm/uaccess.h>
#include <linux/miscdevice.h>
#include <linux/reboot.h>

#include <linux/leds.h>
#include <linux/kc_leds_drv.h>


// function switch
//#define DISABLE_DISP_DETECT
//#define FORCE_WLED_ALWAYS_ON
//#define FORCE_WLED_ON
#define KEEP_DEVICE_STATE_ON_PROBE
//#define ENABLE_PWM

#ifdef FORCE_WLED_ALWAYS_ON
#ifndef FORCE_WLED_ON
#define FORCE_WLED_ON
#endif
#ifdef KEEP_DEVICE_STATE_ON_PROBE
#undef KEEP_DEVICE_STATE_ON_PROBE
#endif
#endif

/* Registr */
#define KCLIGHT_REG_MASTER_FADER1	0x46

#define KCLIGHT_REG_LED0_PWM		0x16
#define KCLIGHT_REG_LED1_PWM		0x17
#define KCLIGHT_REG_LED2_PWM		0x18
#define KCLIGHT_REG_LED0_CURRENT	0x22
#define KCLIGHT_REG_LED1_CURRENT	0x23
#define KCLIGHT_REG_LED2_CURRENT	0x24

// debug
//#define DEBUG (1)
//#define KCLIGHT_DEBUG

#ifdef KCLIGHT_DEBUG
#define KCLIGHT_V_LOG(msg, ...)	\
	pr_notice("[LEDDRV][LP5569][%s][V](%d): " msg "\n", __func__, __LINE__, ## __VA_ARGS__)
#define KCLIGHT_D_LOG(msg, ...)	\
	pr_notice("[LEDDRV][LP5569][%s][D](%d): " msg "\n", __func__, __LINE__, ## __VA_ARGS__)
#else
#define KCLIGHT_V_LOG(msg, ...)
#define KCLIGHT_D_LOG(msg, ...)	\
	pr_debug ("[LEDDRV][LP5569][%s][D](%d): " msg "\n", __func__, __LINE__, ## __VA_ARGS__)
#endif

#define KCLIGHT_E_LOG(msg, ...)	\
	pr_err   ("[LEDDRV][LP5569][%s][E](%d): " msg "\n", __func__, __LINE__, ## __VA_ARGS__)
#define KCLIGHT_N_LOG(msg, ...)	\
	pr_notice("[LEDDRV][LP5569][%s][N](%d): " msg "\n", __func__, __LINE__, ## __VA_ARGS__)


#define KCLIGHT_DRV_NAME "LP5569"

#define I2C_RETRIES_NUM				(5)
#define KCLIGHT_I2C_WRITE_MSG_NUM	(1)
#define KCLIGHT_I2C_READ_MSG_NUM		(2)
#define KCLIGHT_WRITE_BUF_LEN		(2)

#define KCLIGHT_COMBINATION_ILLUM1		0x82818181
#define KCLIGHT_COMBINATION_ILLUM1_MAX	10
static uint32_t combination_illumination_tbl[KCLIGHT_COMBINATION_ILLUM1_MAX] = {
	0x00FF0000,
	0x0000FF00,
	0x000000FF,
	0x00FFFF00,
	0x00FF00FF,
	0x0000FFFF,
	0x00FF00FF,
	0x00FFFF00,
	0x000000FF,
	0x0000FF00
};
static int kclight_led_illum_count = 0;

struct kclight_reg_info {
	uint8_t	value;
	bool	written;
};

struct kclight_reg_write_info {
	uint8_t	reg;
	uint8_t	val;
	uint8_t	mask;
};

#define KCLIGHT_REG_ON_SEQ_MAX (sizeof(kclight_reg_poweron_seq)/sizeof(struct kclight_reg_write_info))

#define BLINK_COLOR_ARRAY_POS		12
#define BLINK_COLOR_PWM_ARRAY_POS	24

#define KCLIGHT_REG_SRAM_PATTERN_SEQ_MAX (sizeof(kclight_reg_pattern_seq)/sizeof(struct kclight_reg_write_info))

#define KCLIGHT_REG_SRAM_SETPATTERNONOFF_SEQ_MAX (sizeof(kclight_reg_setpatternoff_seq)/sizeof(struct kclight_reg_write_info))

#define LABEL_MLED		"mled"
#define LABEL_RGB		"rgb"
#define LABEL_CLED		"cled"
#define LABEL_KEYLED	"keyled"
#define LABEL_SLED		"sled"
#define LABEL_ALED		"aled"

#define NUM_OF_COLORVARIATION	(5)
#define COLORVARIATION_DEFAULT	(3)

enum {
	LED_COLOR_OFF = 0,
	LED_COLOR_R,
	LED_COLOR_G,
	LED_COLOR_B,
	LED_COLOR_RG,
	LED_COLOR_GB,
	LED_COLOR_RB,
	LED_COLOR_RGB,
	LED_COLOR_MAX
};

static int rgb_table[LED_COLOR_MAX+1] = {
	LED_COLOR_OFF,
	LED_COLOR_R,
	LED_COLOR_G,
	LED_COLOR_RG,
	LED_COLOR_B,
	LED_COLOR_RB,
	LED_COLOR_GB,
	LED_COLOR_RGB
};

#define	kclight_get_color(n)	rgb_table[n]

enum {
	LEDLIGHT_PATTERN_MANUAL = 0,
	LEDLIGHT_PATTERN_1,
	LEDLIGHT_PATTERN_2,
	LEDLIGHT_PATTERN_3,
	LEDLIGHT_PATTERN_4,
	LEDLIGHT_PATTERN_5,
	LEDLIGHT_PATTERN_6,
	LEDLIGHT_PATTERN_7,
	LEDLIGHT_PATTERN_8,
	LEDLIGHT_PATTERN_9,
	LEDLIGHT_PATTERN_10,
	LEDLIGHT_PATTERN_11,
	LEDLIGHT_PATTERN_12,
	LEDLIGHT_PATTERN_MAX
};

#define BRIGHTNESS_MAX	(0xFFFFFFFFul)
#define BL_BRIGHTNESS_MAX	255

enum {
	LED_TYPE_MLED = 0,
	LED_TYPE_RGB,
	LED_TYPE_CLED,
	LED_TYPE_KEYLED,
	LED_TYPE_SLED,
	LED_TYPE_ALED,
	LED_TYPE_MAX
};

enum {
	CTRL_MODE_MANUAL = 0,
	CTRL_MODE_ALC,
	CTRL_MODE_ALC_CAMERA,
	CTRL_MODE_DIM,
	CTRL_MODE_MAX
};

#define GET_CTRL_MODE(a)			(a & 0x0f)
#define CTRL_MODE_EXT_CAMERA		0x10

#define	kclight_get_blink_pwm_val(n)	rgb_blink_pwm_table[n]

struct kclight_led_param {
	uint32_t				value;

	// for rgb
	uint32_t				blink_control;
	uint32_t				blink_low_pause_time;
	uint32_t				blink_high_pause_time;
	uint32_t				blink_off_color;

	// for mled/keyled
	uint32_t				ctrl_mode_mled;
	uint32_t				ctrl_mode_keyled;

	// for keyled
	uint32_t				keyled_threshold;
};

struct kclight_data;
struct kclight_led_data;
typedef void (*kclight_ctrl_func_t)(struct kclight_led_data *led,
	struct kclight_led_param *param_next);

struct kclight_led_data {
	struct kclight_data			*parent;
	struct led_classdev			cdev;
	bool						cdev_registered;
	struct work_struct			work;
	struct workqueue_struct		*workqueue;
	int							led_type;
	kclight_ctrl_func_t			ctrl_func;
	struct mutex				param_next_lock;
	struct kclight_led_param		param_next;
	struct kclight_led_param		param_prev;
};

struct kclight_blink_ctl {
	bool						next;
	struct kclight_led_param		param;
};

struct kclight_data {
	struct i2c_client			*client;
	int							reset_gpio;
	int							keyled_gpio;
	int							keyled_on_gpio;
	int							sled_gpio;

	struct mutex				control_lock;
	struct kclight_led_data		led_data[LED_TYPE_MAX];
	struct mutex				param_lock;
	struct kclight_led_param	param;
	int							power_state;
	int							power_vkey_state;
	struct delayed_work			blink_work;
	struct kclight_blink_ctl	blink_ctl;
	struct wake_lock			wake_lock;
	struct workqueue_struct		*work_queue;
	int							colorvariation;
	struct mutex				mled_lock;
	struct notifier_block		reboot_notifier;
};

static struct kclight_data *kclight_data;

static atomic_t g_panel_class = ATOMIC_INIT(0);

#define KCLIGHT_REG_BL_LEVEL_SEQ_MAX (sizeof(mled_bl_level_seq)/sizeof(struct kclight_reg_write_info))

struct kclight_rgb_current_regs {
	uint8_t	reg16;
	uint8_t	reg17;
	uint8_t	reg18;
	uint8_t	reg22;
	uint8_t	reg23;
	uint8_t	reg24;
};

struct kclight_rgb_current_info {
	struct kclight_rgb_current_regs reg_color[LED_COLOR_MAX];
};

static struct kclight_reg_write_info kclight_reg_poweron_seq[19];
static struct kclight_reg_write_info kclight_reg_pattern_seq[78];
static struct kclight_reg_write_info kclight_reg_setpattern1_seq[3];
static struct kclight_reg_write_info kclight_reg_setpattern2_seq[3];
static struct kclight_reg_write_info kclight_reg_setpatternoff_seq[3];
static int rgb_blink_pwm_table[LED_COLOR_MAX] = {0};
static uint8_t mled_bl_level_reg[256] = {0};
static struct kclight_reg_write_info mled_bl_level_seq[6];
static struct kclight_rgb_current_info rgb_current_setting[NUM_OF_COLORVARIATION];

#define KCLIGHT_WLED_BOOT_DEFAULT	102
static bool board_check = false;
static int bl_skip_flag = 1;

/* Local Function */
#if 0
static int kclight_i2c_read(struct i2c_client *client, uint8_t uc_reg, uint8_t *rbuf, int len);
#endif //LIGHT_ALC_ENABLE
static int kclight_i2c_write(struct i2c_client *client, uint8_t uc_reg, uint8_t uc_val);
static void kclight_work(struct work_struct *work);
static void kclight_blink_work(struct work_struct *work);
static int kclight_remove(struct i2c_client *client);
static int kclight_probe(struct i2c_client *client, const struct i2c_device_id *id);
static bool light_led_disp_enabled(void);
static void kclight_rgb_current_write(struct i2c_client *client, int color);
static bool light_lcd_power_off(void);
static int32_t light_led_disp_set(e_light_main_wled_disp disp_status);
static int kclight_i2c_masked_write(struct i2c_client *client, uint8_t uc_reg, uint8_t uc_val, uint8_t mask);
static void kclight_rgb_onoff_write(struct i2c_client *client, int color);
static void lp5569_set_maxbrightness(int max_level, int enable);
static int32_t lp5569_light_led_disp_set_panel(e_light_main_wled_disp disp_status, e_light_lcd_panel panel_class);
static int32_t lp5569_light_led_disp_power_set(e_light_main_wled_disp disp_status);

struct kc_leds_funcs lp5569_kc_leds_funcs = {
	.pset_maxbrightness = lp5569_set_maxbrightness,
	.plight_led_disp_set_panel = lp5569_light_led_disp_set_panel,
	.plight_led_disp_power_set = lp5569_light_led_disp_power_set
};

static int kclight_reboot_notify(struct notifier_block *nb,
				unsigned long code, void *unused)
{
	struct kclight_data *data = kclight_data;

	pr_notice("[KCLIGHT]%s +\n", __func__);

	kclight_i2c_masked_write(data->client, 0x00, 0x00, 0xFF);
	gpio_set_value(data->reset_gpio, 0);
	usleep_range(15000, 15000);

	kfree(data);
	kclight_data = NULL;

	pr_notice("[KCLIGHT]%s -\n", __func__);

	return NOTIFY_DONE;
}

static void kclight_mled_set_bl(struct i2c_client *client, int level)
{
	struct kclight_reg_write_info *reg_info;
	int i;

	if (level > 255) {
		level = 255;
	}

	if (level) {
		KCLIGHT_V_LOG("[KCLIGHT] %s: level=%d\n", __func__, level);

		reg_info = mled_bl_level_seq;
		for (i = 0; i < KCLIGHT_REG_BL_LEVEL_SEQ_MAX; i++) {
			kclight_i2c_masked_write(client, reg_info->reg, mled_bl_level_reg[level], 0xFF);
			reg_info++;
		}
		kclight_i2c_masked_write(client, KCLIGHT_REG_MASTER_FADER1, 0xFF, 0xFF);
	} else {
		KCLIGHT_V_LOG("[KCLIGHT] %s: off\n", __func__);
		kclight_i2c_masked_write(client, KCLIGHT_REG_MASTER_FADER1, 0x00, 0xFF);
	}
}

#if 0
static int kclight_i2c_read(struct i2c_client *client, uint8_t uc_reg, uint8_t *rbuf, int len)
{
	int ret = 0;
	int retry = 0;
	struct i2c_msg i2cMsg[KCLIGHT_I2C_READ_MSG_NUM];
	u8 reg = 0;
	int i = 0;

	KCLIGHT_V_LOG("[IN] client=0x%p reg=0x%02X len=%d", client, uc_reg, len);

	if (client == NULL) {
		KCLIGHT_E_LOG("fail client=0x%p", client);
		return -ENODEV;
	}

	reg = uc_reg;

	i2cMsg[0].addr = client->addr;
	i2cMsg[0].flags = 0;
	i2cMsg[0].len = 1;
	i2cMsg[0].buf = &reg;

	i2cMsg[1].addr = client->addr;
	i2cMsg[1].flags = I2C_M_RD;
	i2cMsg[1].len = len;
	i2cMsg[1].buf = rbuf;

	do {
		ret = i2c_transfer(client->adapter, &i2cMsg[0], KCLIGHT_I2C_READ_MSG_NUM);
		KCLIGHT_V_LOG("i2c_transfer() call end ret=%d", ret);
	} while ((ret != KCLIGHT_I2C_READ_MSG_NUM) && (++retry < I2C_RETRIES_NUM));

	if (ret != KCLIGHT_I2C_READ_MSG_NUM) {
		KCLIGHT_E_LOG("fail (try:%d) uc_reg=0x%02x, rbuf=0x%02x ret=%d", retry, uc_reg, *rbuf, ret);
		ret = -1;
	} else {
		ret = 0;
		KCLIGHT_V_LOG("i2c read success");
		for (i = 0; i < len; i++)
		{
			KCLIGHT_D_LOG("i2c read  reg=0x%02x,value=0x%02x", (unsigned int)(uc_reg + i), (unsigned int)*(rbuf + i));
		}
	}
	KCLIGHT_V_LOG("[OUT] ret=%d", ret);

	return ret;
}
#endif

static int kclight_i2c_write(struct i2c_client *client, uint8_t uc_reg, uint8_t uc_val)
{
	int ret = 0;
	int retry = 0;
	struct i2c_msg i2cMsg;
	u8 ucwritebuf[KCLIGHT_WRITE_BUF_LEN];

	KCLIGHT_V_LOG("[IN] client=0x%p reg=0x%02x val=0x%02X", client, uc_reg, uc_val);

	if (client == NULL) {
		KCLIGHT_E_LOG("fail client=0x%p", client);
		return -ENODEV;
	}

	ucwritebuf[0] = uc_reg;
	ucwritebuf[1] = uc_val;
	i2cMsg.addr  = client->addr;
	i2cMsg.flags = 0;
	i2cMsg.len   =  sizeof(ucwritebuf);
	i2cMsg.buf   =  &ucwritebuf[0];

	KCLIGHT_D_LOG("i2c write reg=0x%02x,value=0x%02x", uc_reg, uc_val);

	do {
		ret = i2c_transfer(client->adapter, &i2cMsg, KCLIGHT_I2C_WRITE_MSG_NUM);
		KCLIGHT_V_LOG("i2c_transfer() call end ret=%d", ret);
	} while ((ret != KCLIGHT_I2C_WRITE_MSG_NUM) && (++retry < I2C_RETRIES_NUM));

	KCLIGHT_V_LOG("[OUT] ret=%d", ret);

	return ret;
}

static int kclight_i2c_masked_write(struct i2c_client *client, uint8_t uc_reg, uint8_t uc_val, uint8_t mask)
{
	int ret;

	KCLIGHT_V_LOG("[IN] reg=0x%02x val=0x%02x mask=0x%02x", uc_reg, uc_val, mask);

	ret = kclight_i2c_write(client, uc_reg, uc_val);

	KCLIGHT_V_LOG("[OUT] ret=%d", ret);

	return ret;
}

static void led_set(struct led_classdev *cdev, enum led_brightness value)
{
	struct kclight_led_data *led =
		container_of(cdev, struct kclight_led_data, cdev);
	struct kclight_data *data = led->parent;

	KCLIGHT_D_LOG("[IN] name=%s value=0x%08x", cdev->name, value);

	mutex_lock(&data->param_lock);
	mutex_lock(&led->param_next_lock);
	memcpy(&led->param_next, &data->param, sizeof(led->param_next));
	led->param_next.value = (uint32_t)value;
	mutex_unlock(&led->param_next_lock);
	mutex_unlock(&data->param_lock);

	queue_work(led->workqueue, &led->work);

	KCLIGHT_D_LOG("[OUT]");
}

static enum led_brightness led_get(struct led_classdev *cdev)
{
	int32_t lret = 0;
	struct kclight_led_data *led =
		container_of(cdev, struct kclight_led_data, cdev);

	KCLIGHT_D_LOG("[IN] name=%s", cdev->name);

	lret = (int32_t)led->param_next.value;

	KCLIGHT_D_LOG("[OUT] lret=0x%08x", lret);

	return lret;
}

static void kclight_ctrl_mled(struct kclight_led_data *led,
	struct kclight_led_param *param_next)
{
	struct kclight_data *data = led->parent;
	struct i2c_client *client = data->client;
	int level;

	KCLIGHT_V_LOG("[IN] value=0x%08x", param_next->value);
	mutex_lock(&data->mled_lock);

	if (!light_led_disp_enabled()) {
		KCLIGHT_V_LOG("discard light_led_disp_enabled()");
		goto exit;
	}

	param_next->value = min(param_next->value, led->cdev.max_brightness);
	KCLIGHT_V_LOG("[IN] min value=0x%08x", param_next->value);

	if (light_lcd_power_off()){
			param_next->value = 0;
	}

	if (param_next->value) {

		switch (GET_CTRL_MODE(param_next->ctrl_mode_mled)) {
		case CTRL_MODE_DIM:
			KCLIGHT_V_LOG("CTRL_MODE_DIM");
			kclight_mled_set_bl(client, 1);
			break;
		case CTRL_MODE_MANUAL:
			KCLIGHT_V_LOG("CTRL_MODE_MANUAL");
			/* FALL THROUGH */
		default:
			KCLIGHT_V_LOG("default");
			level = param_next->value & 0xFF;
			kclight_mled_set_bl(client, level);
			break;
		}
	} else {
		kclight_mled_set_bl(client, 0);
	}

exit:
	mutex_unlock(&data->mled_lock);
	KCLIGHT_V_LOG("[OUT]");
}

static void kclight_ctrl_sled(struct kclight_led_data *led,
							 struct kclight_led_param *param_next)
{
	struct kclight_data *data = led->parent;

	KCLIGHT_V_LOG("[IN]");

	if(param_next->value & 0x00ffffff){
		KCLIGHT_V_LOG("SUBLED ON");
		if (gpio_is_valid(data->sled_gpio))
			gpio_set_value(data->sled_gpio, 1);
		else
			KCLIGHT_E_LOG("No valid SUBLED GPIO specified %d", data->sled_gpio);
	} else {
		KCLIGHT_V_LOG("SUBLED OFF");
		if (gpio_is_valid(data->sled_gpio))
			gpio_set_value(data->sled_gpio, 0);
		else
			KCLIGHT_E_LOG("No valid SUBLED GPIO specified %d", data->sled_gpio);
	}

	KCLIGHT_V_LOG("[OUT]");
	return;
}

static void kclight_rgb_oneshot_onoff_write(struct i2c_client *client, int pattern)
{
	struct kclight_reg_write_info *reg_info;
	int i;
	KCLIGHT_V_LOG("[IN]");

	if (pattern == 1) {
		reg_info = kclight_reg_setpattern1_seq;
	} else if (pattern == 2) {
		reg_info = kclight_reg_setpattern2_seq;
	} else {
		reg_info = kclight_reg_setpatternoff_seq;
	}

	for (i = 0; i < KCLIGHT_REG_SRAM_SETPATTERNONOFF_SEQ_MAX; i++) {
		kclight_i2c_masked_write(client, reg_info->reg, reg_info->val, 0xFF);
		reg_info++;
	}

	KCLIGHT_V_LOG("[IN/OUT]");
}


static void kclight_rgb_off(struct kclight_data *data)
{
	struct i2c_client *client = data->client;
	KCLIGHT_V_LOG("[IN]");
	wake_unlock(&data->wake_lock);
	cancel_delayed_work(&data->blink_work);

	kclight_rgb_current_write(client, 0);
	kclight_rgb_onoff_write(client, 0);
	kclight_rgb_oneshot_onoff_write(client, 0);

	kclight_led_illum_count = 0;
	KCLIGHT_V_LOG("[OUT]");
}

static void kclight_rgb_on(struct i2c_client *client, uint32_t value)
{
	uint8_t rgb_on;
	int color;

	KCLIGHT_V_LOG("[IN] value=0x%08x", value);

	rgb_on = ((value & 0x00ff0000) ? 0x01 : 0) |
			 ((value & 0x0000ff00) ? 0x02 : 0) |
			 ((value & 0x000000ff) ? 0x04 : 0);

	// current & on
	color = kclight_get_color(rgb_on);
	kclight_rgb_current_write(client, color);
	kclight_rgb_onoff_write(client, color);

	KCLIGHT_V_LOG("[OUT]");
}

static void kclight_rgb_blink_oneshot(struct i2c_client *client, struct kclight_led_param *param)
{
	uint8_t i;
	struct kclight_reg_write_info *reg_info;
	int pattern = 0;
	int color;
	int rgb_on;
	int bgr_on;

	KCLIGHT_D_LOG("[IN] value=0x%08x off=0x%08x on=%dms off=%dms",
		param->value, param->blink_off_color,
		param->blink_high_pause_time, param->blink_low_pause_time);

	rgb_on = 0;
	bgr_on = 0;
	if (param->value&0x000000FF) {
		rgb_on |= 0x1;
		bgr_on |= 0x04;
	}
	if (param->value&0x0000FF00) {
		rgb_on |= 0x2;
		bgr_on |= 0x02;
	}
	if (param->value&0x00FF0000) {
		rgb_on |= 0x4;
		bgr_on |= 0x01;
	}

		if (param->blink_off_color == 0) {
			pattern = 1;
		} else {
			pattern = 2;
			bgr_on |= 0x01;
		}

	KCLIGHT_D_LOG("rgb_on=%x bgr_on=%x", rgb_on, bgr_on);

	reg_info = kclight_reg_pattern_seq;
	(reg_info+BLINK_COLOR_ARRAY_POS)->val = rgb_on;
	(reg_info+BLINK_COLOR_PWM_ARRAY_POS)->val = kclight_get_blink_pwm_val(rgb_on);

	for (i = 0; i < KCLIGHT_REG_SRAM_PATTERN_SEQ_MAX; i++) {
		kclight_i2c_masked_write(client, reg_info->reg, reg_info->val, 0xFF);
		reg_info++;
	}

	color = kclight_get_color(bgr_on);
	kclight_rgb_current_write(client, color);
	kclight_rgb_oneshot_onoff_write(client, pattern);


	KCLIGHT_D_LOG("[OUT]");
}

static void kclight_rgb_blink(struct kclight_data *data, struct kclight_led_param *param)
{
	bool blink;
	struct i2c_client *client = data->client;
	int32_t total_time;
	struct kclight_led_data *led = &data->led_data[LED_TYPE_RGB];

	KCLIGHT_V_LOG("[IN]");

	if (!param->value)
		return;

	if (param->blink_high_pause_time && param->blink_low_pause_time)
		blink = true;
	else
		blink = false;

	if (blink) {
		total_time = param->blink_high_pause_time + param->blink_low_pause_time;
		if ((total_time >= 1000)&&(!param->blink_control)) {
			kclight_rgb_blink_oneshot(client, param);
		} else {
			wake_lock(&data->wake_lock);

			data->blink_ctl.next  = true;
			data->blink_ctl.param = *param;
			queue_delayed_work(led->workqueue, &data->blink_work, msecs_to_jiffies(1));
		}
	} else {
		kclight_rgb_on(client, param->value);
	}

	KCLIGHT_V_LOG("[OUT]");
}

static void kclight_rgb_current_write(struct i2c_client *client, int color)
{
	struct kclight_data *data = i2c_get_clientdata(client);
	const struct kclight_rgb_current_regs *reg_color;
	KCLIGHT_V_LOG("[IN] colvar:%d color:%d",data->colorvariation, color);

	if (color >= 0 && color < LED_COLOR_MAX) {
		reg_color = &rgb_current_setting[data->colorvariation].reg_color[color];
		kclight_i2c_masked_write(client, KCLIGHT_REG_LED0_CURRENT, reg_color->reg22, 0xFF);
		kclight_i2c_masked_write(client, KCLIGHT_REG_LED1_CURRENT, reg_color->reg23, 0xFF);
		kclight_i2c_masked_write(client, KCLIGHT_REG_LED2_CURRENT, reg_color->reg24, 0xFF);
	}
	KCLIGHT_V_LOG("[IN/OUT]");
}

static void kclight_rgb_onoff_write(struct i2c_client *client, int color)
{
	struct kclight_data *data = i2c_get_clientdata(client);
	const struct kclight_rgb_current_regs *reg_color;
	KCLIGHT_V_LOG("[IN] colvar:%d color:%d",data->colorvariation, color);

	if (color >= 0 && color < LED_COLOR_MAX) {
		reg_color = &rgb_current_setting[data->colorvariation].reg_color[color];
		kclight_i2c_masked_write(client, KCLIGHT_REG_LED0_PWM, reg_color->reg16, 0xFF);
		kclight_i2c_masked_write(client, KCLIGHT_REG_LED1_PWM, reg_color->reg17, 0xFF);
		kclight_i2c_masked_write(client, KCLIGHT_REG_LED2_PWM, reg_color->reg18, 0xFF);
	}
	KCLIGHT_V_LOG("[IN/OUT]");
}

static void kclight_ctrl_rgb(struct kclight_led_data *led,
	struct kclight_led_param *param_next)
{
	struct kclight_data *data = led->parent;

	KCLIGHT_V_LOG("[IN] value=0x%08x", param_next->value);

	if (param_next->value) {
		kclight_rgb_off(data);
		kclight_rgb_blink(data, param_next);
	} else {
		kclight_rgb_off(data);
	}
	KCLIGHT_V_LOG("[OUT]");
}

static void kclight_ctrl_cled(struct kclight_led_data *led,
	struct kclight_led_param *param_next)
{
//	struct kclight_data *data = led->parent;
//	struct i2c_client *client = led->parent->client;

	KCLIGHT_V_LOG("[IN] value=0x%08x", param_next->value);

	if (param_next->value) {
	} else {
	}

	KCLIGHT_V_LOG("[OUT]");
}

static void kclight_ctrl_keyled(struct kclight_led_data *led,
	struct kclight_led_param *param_next)
{
	struct kclight_data *data = led->parent;

	KCLIGHT_V_LOG("[IN] value=0x%08x", param_next->value);

	if (param_next->value) {
			gpio_set_value(data->keyled_on_gpio, 1);
	} else {
		gpio_set_value(data->keyled_on_gpio, 0);
	}

	KCLIGHT_V_LOG("[OUT]");
}

static void kclight_blink_work(struct work_struct *work)
{

	struct kclight_data *data =
		container_of(work, struct kclight_data, blink_work.work);
	struct kclight_led_data *led = &data->led_data[LED_TYPE_RGB];
	uint32_t value;

	KCLIGHT_V_LOG("[IN]");

	mutex_lock(&data->control_lock);
	if (!data->blink_ctl.next) {
		data->blink_ctl.next = true;
		kclight_rgb_on(data->client, data->blink_ctl.param.blink_off_color);
		queue_delayed_work(led->workqueue, &data->blink_work,
			msecs_to_jiffies(data->blink_ctl.param.blink_low_pause_time));
	} else {
		data->blink_ctl.next = false;
		value = data->blink_ctl.param.value;
		if (value == (KCLIGHT_COMBINATION_ILLUM1&0x00ffffff)) {
			KCLIGHT_V_LOG("ILLUMI #1 value=%x count=%d", value, kclight_led_illum_count);
			if (kclight_led_illum_count > KCLIGHT_COMBINATION_ILLUM1_MAX) {
				kclight_led_illum_count = 0;
			}
			value = combination_illumination_tbl[kclight_led_illum_count];
			kclight_led_illum_count++;
			KCLIGHT_V_LOG("ILLUMI #2 value=%x count=%d", value, kclight_led_illum_count);
		}
		kclight_rgb_on(data->client, value);
		queue_delayed_work(led->workqueue, &data->blink_work,
			msecs_to_jiffies(data->blink_ctl.param.blink_high_pause_time));
	}
	mutex_unlock(&data->control_lock);

	KCLIGHT_V_LOG("[OUT]");
}

static void kclight_work(struct work_struct *work)
{
	struct kclight_led_data *led =
		container_of(work, struct kclight_led_data, work);
	struct kclight_data *data = led->parent;
	struct kclight_led_param param_next;

	KCLIGHT_V_LOG("[IN] led_type[%d]", led->led_type);

	mutex_lock(&led->param_next_lock);
	memcpy(&param_next, &led->param_next, sizeof(param_next));
	mutex_unlock(&led->param_next_lock);

	if (led->ctrl_func) {
		mutex_lock(&data->control_lock);
		led->ctrl_func(led, &param_next);
		mutex_unlock(&data->control_lock);
	}

	memcpy(&led->param_prev, &param_next, sizeof(led->param_prev));

	KCLIGHT_V_LOG("[OUT]");
}

//
// for device ioctl
//
#define LEDLIGHT_PARAM_NUM      (5)
#define LEDLIGHT                'L'
#define LEDLIGHT_SET_BLINK		_IOW(LEDLIGHT, 0, T_LEDLIGHT_IOCTL)
#define LEDLIGHT_SET_PARAM      _IOW(LEDLIGHT, 1, T_LEDLIGHT_IOCTL)
#define LEDLIGHT_SET_HWCHECK    _IOW(LEDLIGHT, 2, T_LEDLIGHT_IOCTL)

#define LEDLIGHT_PARAM_MLED     (1)
#define LEDLIGHT_PARAM_RGB      (2)
#define LEDLIGHT_PARAM_CLED     (3)
#define LEDLIGHT_PARAM_KEYLED   (4)

#define LEDLIGHT_PARAM_CHANGE_COLORVARIATION   (10)

#define LEDLIGHT_PARAM_HWCHECK_VLEDONOFF	1

typedef struct _t_ledlight_ioctl {
	uint32_t data[LEDLIGHT_PARAM_NUM];
} T_LEDLIGHT_IOCTL;

static int32_t leds_open(struct inode* inode, struct file* filp)
{
	KCLIGHT_V_LOG("[IN/OUT]");
	return 0;
}

static int32_t leds_release(struct inode* inode, struct file* filp)
{
	KCLIGHT_V_LOG("[IN/OUT]");
	return 0;
}

static long leds_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	int32_t ret = -1;
	T_LEDLIGHT_IOCTL st_ioctl;
	struct kclight_data *data = kclight_data;
	struct kclight_led_param *param = &data->param;

	KCLIGHT_V_LOG("[IN]");

	if (!data) {
		KCLIGHT_N_LOG("Error data == NULL");
		return -EFAULT;
	}
	switch (cmd) {
	case LEDLIGHT_SET_PARAM:
		KCLIGHT_V_LOG("LEDLIGHT_SET_CONTROL 0x%08x", LEDLIGHT_SET_PARAM);
		ret = copy_from_user(&st_ioctl,
					argp,
					sizeof(T_LEDLIGHT_IOCTL));
		if (ret) {
			KCLIGHT_N_LOG("Error leds_ioctl(cmd = LEDLIGHT_SET_CONTROL_MODE)");
			return -EFAULT;
		}
		KCLIGHT_V_LOG("st_ioctl data[0]=[0x%08x] data[1]=[0x%08x] data[2]=[0x%08x] data[3]=[0x%08x] data[4]=[0x%08x]",
			st_ioctl.data[0], st_ioctl.data[1], st_ioctl.data[2], st_ioctl.data[3], st_ioctl.data[4]);

		switch(st_ioctl.data[0]) {
		case LEDLIGHT_PARAM_MLED:
			mutex_lock(&data->param_lock);
			param->ctrl_mode_mled = st_ioctl.data[1];
			mutex_unlock(&data->param_lock);
			KCLIGHT_D_LOG("LEDLIGHT_PARAM_MLED ctrl_mode_mled=0x%x", param->ctrl_mode_mled);
			break;
		case LEDLIGHT_PARAM_RGB:
			mutex_lock(&data->param_lock);
			param->blink_high_pause_time = st_ioctl.data[1];
			param->blink_low_pause_time  = st_ioctl.data[2];
			param->blink_control         = st_ioctl.data[3];
			param->blink_off_color       = st_ioctl.data[4];
			mutex_unlock(&data->param_lock);
			KCLIGHT_D_LOG("LEDLIGHT_PARAM_RGB blink_control=%d high=%d low=%d off_color=0x%08x",
				param->blink_control, param->blink_high_pause_time,
				param->blink_low_pause_time, param->blink_off_color);
			break;
		case LEDLIGHT_PARAM_KEYLED:
			mutex_lock(&data->param_lock);
			param->ctrl_mode_keyled = st_ioctl.data[1];
			param->keyled_threshold = st_ioctl.data[2];
			mutex_unlock(&data->param_lock);
			KCLIGHT_D_LOG("LEDLIGHT_PARAM_KEYLED ctrl_mode_keyled=%d keyled_threshold=%d",
				param->ctrl_mode_keyled, param->keyled_threshold);
			break;
		default:
			KCLIGHT_N_LOG("invalid param 0x%08x", st_ioctl.data[0]);
			return -EFAULT;
			break;
		}
		break;
	case LEDLIGHT_SET_BLINK:
		KCLIGHT_N_LOG("not supported. cmd 0x%08x", cmd);
		break;
	default:
		KCLIGHT_N_LOG("invalid cmd 0x%08x", cmd);
		return -ENOTTY;
	}

	KCLIGHT_V_LOG("[OUT]");

	return 0;
}

static struct file_operations leds_fops = {
	.owner		  = THIS_MODULE,
	.open		 = leds_open,
	.release	= leds_release,
	.unlocked_ioctl = leds_ioctl,
};

static struct miscdevice leds_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name  = "leds-ledlight",
	.fops  = &leds_fops,
};


static void kclight_shutdown(struct i2c_client *client)
{
	// struct kclight_data *data = i2c_get_clientdata(client);

	// kclight_i2c_masked_write(client, 0x00, 0x00, 0xFF);
	// gpio_set_value(data->reset_gpio, 0);
	// usleep_range(15000, 15000);
}

static int kclight_remove(struct i2c_client *client)
{
	struct kclight_data *data = i2c_get_clientdata(client);
	struct kclight_led_data *led;
	int i;

	KCLIGHT_V_LOG("[IN]");

	kclight_data = NULL;

	if (data) {
		misc_deregister(&leds_device);

		for (i = 0; i < LED_TYPE_MAX; i++) {
			led = &data->led_data[i];

			if (led->cdev_registered) {
				led_classdev_unregister(&led->cdev);
				flush_work(&led->work);
				led->cdev_registered = false;
			}
			mutex_destroy(&led->param_next_lock);
		}
		destroy_workqueue(data->work_queue);
		data->work_queue = NULL;
//		if (gpio_is_valid(data->keyled_gpio)) {
//			gpio_free(data->keyled_gpio);
//		}
#ifndef KEEP_DEVICE_STATE_ON_PROBE
		gpio_free(data->reset_gpio);
#endif
		i2c_set_clientdata(client, NULL);
		mutex_destroy(&data->param_lock);
		mutex_destroy(&data->control_lock);
		mutex_destroy(&data->mled_lock);
		wake_lock_destroy(&data->wake_lock);
		kfree(data);
	}

	KCLIGHT_V_LOG("[OUT]");

	return 0;
}

static int kclight_init_client(struct i2c_client *client)
{
	struct kclight_data *data = i2c_get_clientdata(client);
	struct kclight_reg_write_info *reg_info;
	int i;
	int level;

	KCLIGHT_V_LOG("[IN] client=0x%p", client);

#ifndef KEEP_DEVICE_STATE_ON_PROBE
	KCLIGHT_V_LOG("[IN] reset L");
	// GPIO	HARDWARE Reset L->H
	gpio_set_value(data->reset_gpio, 0);
	usleep_range(30, 30);
#endif

	level = gpio_get_value(data->reset_gpio);
	KCLIGHT_V_LOG("[IN] reset #1 level=%d", level);

	KCLIGHT_V_LOG("[IN] reset H");
	gpio_set_value(data->reset_gpio, 1);
	usleep_range(3000, 4000);

	level = gpio_get_value(data->reset_gpio);
	KCLIGHT_V_LOG("[IN] reset #2 level=%d", level);

	reg_info = kclight_reg_poweron_seq;
	for (i = 0; i < KCLIGHT_REG_ON_SEQ_MAX; i++) {
		kclight_i2c_masked_write(client, reg_info->reg, reg_info->val, 0xFF);
		reg_info++;
	}

#ifdef FORCE_WLED_ON
	// force WLED ON
	KCLIGHT_V_LOG("force WLED ON");

	kclight_mled_set_bl(client, 255);
#endif
	KCLIGHT_V_LOG("[OUT]");

	return 0;
}

static int kclight_check_device(struct i2c_client *client)
{
	struct pinctrl				*dc_pinctrl; //dc: device check
	struct pinctrl_state        *dc_pd; //pd:pull down
	struct pinctrl_state        *dc_np; //np:non pull
	int							basecheck_gpio;

	dc_pinctrl = devm_pinctrl_get(&client->dev);
	basecheck_gpio = of_get_named_gpio(client->dev.of_node, "kc,basecheck-gpio", 0);
	if (!gpio_is_valid(basecheck_gpio) || IS_ERR_OR_NULL(dc_pinctrl)) {
		KCLIGHT_E_LOG("basecheck_gpio error");
	} else {
		// set pull down
		dc_pd = pinctrl_lookup_state(dc_pinctrl, "dev_check_pd");
		dc_np = pinctrl_lookup_state(dc_pinctrl, "dev_check_np");

		if ((IS_ERR(dc_pd)) || (IS_ERR(dc_np))) {
			KCLIGHT_E_LOG("pinctrl error");
		}

		pinctrl_select_state(dc_pinctrl, dc_pd);
		usleep_range(2000, 3000);
		if (gpio_get_value(basecheck_gpio) == 0) {
			// LV5216
			KCLIGHT_E_LOG("LV5216 detect");
		} else {
			// LP5569
			KCLIGHT_E_LOG("LP5569 detect");
			// basecheck_gpio change to NP
			pinctrl_select_state(dc_pinctrl, dc_np);
			board_check = true;
		}
	}

	return board_check;
}

static ssize_t kclight_mled_max_brightness_store(struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t count)
{
	unsigned long state;
	ssize_t ret;

	pr_err("[KCLIGHT] %s: max_brightness=%s\n", __func__, buf);

//	sscanf(buf, "%d %d %d %d", &led_type, &onoff, &on_time, &off_time);
	ret = kstrtoul(buf, 10, &state);
	if (ret) {
		pr_err("[KCLIGHT] %s: max_brightness err\n", __func__);
		return count;
	}

	pr_err("[KCLIGHT] %s: max_brightness=%ld\n", __func__, state);
	lp5569_set_maxbrightness(state, 1);

	return count;
}

static ssize_t kclight_mled_max_brightness_show(struct device *dev, struct device_attribute *dev_attr, char * buf)
{
	struct kclight_led_data *led;

	led = &kclight_data->led_data[LED_TYPE_MLED];

	return sprintf(buf, "%d\n", led->cdev.max_brightness);
}

static DEVICE_ATTR(kc_max_brightness, 0664, kclight_mled_max_brightness_show, kclight_mled_max_brightness_store);

static struct attribute *kclight_mled_attrs[] = {
	&dev_attr_kc_max_brightness.attr,
	NULL
};

static const struct attribute_group kclight_mled_attr_group = {
	.attrs = kclight_mled_attrs,
};

static void kc_read_dt_rgb_blink_pwm_table(struct device *pdev, int *rgb_info, char *cmd_key)
{
	int i = 0;
	const char *dt_data_p;
	int dt_data_len = 0;

	KCLIGHT_D_LOG("%s.",__func__);

	dt_data_p = of_get_property(pdev->of_node, cmd_key, &dt_data_len);
	if (!dt_data_p) {
		KCLIGHT_E_LOG("%s: [%s] dt_data_p is NULL.", __func__, cmd_key);
		return;
	}
	for (i=0; i<(dt_data_len); i++) {
		rgb_info[i] = *(dt_data_p);
		KCLIGHT_D_LOG("%s: [%s] [%d] rgb_info=0x%x", __func__, cmd_key, i, rgb_info[i]);
		dt_data_p +=1;
	}
}

static void kc_read_dt_mled_bl_level_reg(struct device *pdev, uint8_t *reg_info, char *cmd_key)
{
	int i = 0;
	const char *dt_data_p;
	int dt_data_len = 0;

	KCLIGHT_D_LOG("%s.",__func__);

	dt_data_p = of_get_property(pdev->of_node, cmd_key, &dt_data_len);
	if (!dt_data_p) {
		KCLIGHT_E_LOG("%s: [%s] dt_data_p is NULL.", __func__, cmd_key);
		return;
	}
	for (i=0; i<(dt_data_len); i++) {
		reg_info[i] = *(dt_data_p);
		KCLIGHT_D_LOG("%s: [%s] [%d] reg_info=0x%x", __func__, cmd_key, i, reg_info[i]);
		dt_data_p +=1;
	}
}

static void kc_read_dt_reg_write_info(struct device *pdev, struct kclight_reg_write_info *reg_info, char *cmd_key)
{
	int i;
	int j = 0;
	const char *dt_data_p;
	int dt_data_len = 0;

	KCLIGHT_D_LOG("%s.",__func__);

	dt_data_p = of_get_property(pdev->of_node, cmd_key, &dt_data_len);
	if (!dt_data_p) {
		KCLIGHT_E_LOG("%s: [%s] dt_data_p is NULL.", __func__, cmd_key);
		return;
	}
	for (i=0; i<(dt_data_len); i++) {
		if (i==0 || (i % 3) == 0) {
			reg_info[j].reg = *(dt_data_p);
			KCLIGHT_D_LOG("%s: [%s] i=[%d] j=[%d] reg=0x%x", __func__, cmd_key, i, j, reg_info[j].reg);
		} else if ((i % 3) == 1) {
			reg_info[j].val = *(dt_data_p);
			KCLIGHT_D_LOG("%s: [%s] i=[%d] j=[%d] val=0x%x", __func__, cmd_key, i, j, reg_info[j].val);
		} else if ((i % 3) == 2) {
			reg_info[j].mask = *(dt_data_p);
			KCLIGHT_D_LOG("%s: [%s] i=[%d] j=[%d] mask=0x%x", __func__, cmd_key, i, j, reg_info[j].mask);
			j++;
		}
		dt_data_p +=1;
	}
}

static void kc_read_dt_rgb_current_info(struct device *pdev, struct kclight_rgb_current_info *rgb_info, char *cmd_key)
{
	int i;
	int j = 0;
	const char *dt_data_p;
	int dt_data_len = 0;

	KCLIGHT_D_LOG("%s.",__func__);

	dt_data_p = of_get_property(pdev->of_node, cmd_key, &dt_data_len);
	if (!dt_data_p) {
		KCLIGHT_E_LOG("%s: [%s] dt_data_p is NULL.", __func__, cmd_key);
		return;
	}
	for (i=0; i<(dt_data_len); i++) {
		if (i==0 || (i % 6) == 0) {
			rgb_info->reg_color[j].reg16 = *(dt_data_p);
			KCLIGHT_D_LOG("%s: [%s] i=[%d] j=[%d] reg16=0x%x", __func__, cmd_key, i, j, rgb_info->reg_color[j].reg16);
		} else if ((i % 6) == 1) {
			rgb_info->reg_color[j].reg17 = *(dt_data_p);
			KCLIGHT_D_LOG("%s: [%s] i=[%d] j=[%d] reg17=0x%x", __func__, cmd_key, i, j, rgb_info->reg_color[j].reg17);
		} else if ((i % 6) == 2) {
			rgb_info->reg_color[j].reg18 = *(dt_data_p);
			KCLIGHT_D_LOG("%s: [%s] i=[%d] j=[%d] reg18=0x%x", __func__, cmd_key, i, j, rgb_info->reg_color[j].reg18);
		} else if ((i % 6) == 3) {
			rgb_info->reg_color[j].reg22 = *(dt_data_p);
			KCLIGHT_D_LOG("%s: [%s] i=[%d] j=[%d] reg22=0x%x", __func__, cmd_key, i, j, rgb_info->reg_color[j].reg22);
		} else if ((i % 6) == 4) {
			rgb_info->reg_color[j].reg23 = *(dt_data_p);
			KCLIGHT_D_LOG("%s: [%s] i=[%d] j=[%d] reg23=0x%x", __func__, cmd_key, i, j, rgb_info->reg_color[j].reg23);
		} else if ((i % 6) == 5) {
			rgb_info->reg_color[j].reg24 = *(dt_data_p);
			KCLIGHT_D_LOG("%s: [%s] i=[%d] j=[%d] reg24=0x%x", __func__, cmd_key, i, j, rgb_info->reg_color[j].reg24);
			j++;
		}
		dt_data_p +=1;
	}
}

static int kclight_probe(struct i2c_client *client,
				   const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct kclight_data *data;
	struct kclight_led_data *led;
	struct device_node *node;
	struct device_node *temp;
	const char *led_label;
	const char *linux_name;
	int i;
	int err = 0;

	KCLIGHT_V_LOG("[IN] client=0x%p", client);

	if (!kclight_check_device(client)) {
		KCLIGHT_E_LOG("LP5569 don't start");
		return -1;
	}

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE)) {
		err = -EIO;
		KCLIGHT_E_LOG("fail i2c_check_functionality");
		goto exit;
	}

	node = client->dev.of_node;
	if (node == NULL){
		KCLIGHT_E_LOG("client->dev.of_node == null");
		err = -ENODEV;
		goto exit;
	}

	data = kzalloc(sizeof(struct kclight_data), GFP_KERNEL);
	if (!data) {
		KCLIGHT_E_LOG("Failed kzalloc");
		err = -ENOMEM;
		goto exit;
	}

	kc_read_dt_reg_write_info(&client->dev, kclight_reg_poweron_seq, "kclight_reg_poweron_seq");
	kc_read_dt_reg_write_info(&client->dev, kclight_reg_pattern_seq, "kclight_reg_pattern_seq");
	kc_read_dt_reg_write_info(&client->dev, kclight_reg_setpattern1_seq, "kclight_reg_setpattern1_seq");
	kc_read_dt_reg_write_info(&client->dev, kclight_reg_setpattern2_seq, "kclight_reg_setpattern2_seq");
	kc_read_dt_reg_write_info(&client->dev, kclight_reg_setpatternoff_seq, "kclight_reg_setpatternoff_seq");
	kc_read_dt_rgb_blink_pwm_table(&client->dev, rgb_blink_pwm_table, "rgb_blink_pwm_table");
	kc_read_dt_mled_bl_level_reg(&client->dev, mled_bl_level_reg, "mled_bl_level_reg");
	kc_read_dt_reg_write_info(&client->dev, mled_bl_level_seq, "mled_bl_level_seq");
	kc_read_dt_rgb_current_info(&client->dev, rgb_current_setting, "rgb_current_setting");

	wake_lock_init(&data->wake_lock, WAKE_LOCK_SUSPEND, KCLIGHT_DRV_NAME);
	mutex_init(&data->control_lock);
	mutex_init(&data->param_lock);
	mutex_init(&data->mled_lock);
	data->client = client;
	i2c_set_clientdata(client, data);
	INIT_DELAYED_WORK(&data->blink_work, kclight_blink_work);

	data->work_queue = create_singlethread_workqueue(KCLIGHT_DRV_NAME);
	if (!data->work_queue) {
		err = -ENODEV;
		goto fail_create_workqueue;
	}

//	data->keyled_gpio = -ENOSYS;
	data->keyled_on_gpio = -ENOSYS;
	data->sled_gpio = -ENOSYS;
	temp = NULL;
	while ((temp = of_get_next_child(node, temp))) {

		KCLIGHT_V_LOG("read label");
		err = of_property_read_string(temp, "label", &led_label);
		if (err < 0) {
			KCLIGHT_E_LOG("Failure reading label, Dev=[0x%08x] err=[%d]", (unsigned int)&client->dev, err);
			continue;
		}
		KCLIGHT_V_LOG("read linux,name");
		err = of_property_read_string(temp, "linux,name", &linux_name);
		if (err < 0) {
			KCLIGHT_E_LOG("Failure reading linux,name, Dev=[0x%08x] err=[%d]", (unsigned int)&client->dev, err);
			continue;
		}

		KCLIGHT_V_LOG("label=%s linux,name=%s", led_label, linux_name);
		led = NULL;
		if (strcmp(led_label, LABEL_MLED) == 0) {
			KCLIGHT_V_LOG("probe MLED");
			led = &data->led_data[LED_TYPE_MLED];
			led->led_type = LED_TYPE_MLED;
			led->ctrl_func = kclight_ctrl_mled;
			led->workqueue = system_wq;
		} else if (strcmp(led_label, LABEL_RGB) == 0) {
			KCLIGHT_V_LOG("probe RGB");
			led = &data->led_data[LED_TYPE_RGB];
			led->led_type = LED_TYPE_RGB;
			led->ctrl_func = kclight_ctrl_rgb;
			led->workqueue = data->work_queue;
		} else if (strcmp(led_label, LABEL_CLED) == 0) {
			KCLIGHT_V_LOG("probe MLED");
			led = &data->led_data[LED_TYPE_CLED];
			led->led_type = LED_TYPE_CLED;
			led->ctrl_func = kclight_ctrl_cled;
			led->workqueue = system_wq;
		} else if (strcmp(led_label, LABEL_KEYLED) == 0) {
			KCLIGHT_V_LOG("probe KEYLED");
			led = &data->led_data[LED_TYPE_KEYLED];
			led->led_type = LED_TYPE_KEYLED;
			led->ctrl_func = kclight_ctrl_keyled;
			led->workqueue = system_wq;
			data->keyled_on_gpio = of_get_named_gpio(client->dev.of_node, "kc,keyled-on-gpio", 0);
			if (!gpio_is_valid(data->keyled_on_gpio)) {
				KCLIGHT_E_LOG("No valid KEYLED ON GPIO specified %d", data->keyled_on_gpio);
				goto fail_id_check;
			}
		} else if (strcmp(led_label, LABEL_SLED) == 0) {
			KCLIGHT_V_LOG("probe SLED");
			led = &data->led_data[LED_TYPE_SLED];
			led->led_type = LED_TYPE_SLED;
			led->ctrl_func = kclight_ctrl_sled;
			led->workqueue = system_wq;
			data->sled_gpio = of_get_named_gpio(client->dev.of_node, "kc,subled-gpio", 0);
			if (!gpio_is_valid(data->sled_gpio)) {
				KCLIGHT_E_LOG("No valid SUBLED GPIO specified %d", data->sled_gpio);
				goto fail_id_check;
			}
		} else if (strcmp(led_label, LABEL_ALED) == 0) {
			KCLIGHT_V_LOG("probe ALED");
			led = &data->led_data[LED_TYPE_ALED];
			led->led_type = LED_TYPE_ALED;
			led->ctrl_func = NULL;
			led->workqueue = system_wq;
		} else {
			KCLIGHT_N_LOG("unknown label:%s", led_label);
		}

		if (led) {
			mutex_init(&led->param_next_lock);
			led->cdev.brightness_set = led_set;
			led->cdev.brightness_get = led_get;
			led->cdev.name			 = linux_name;
			led->cdev.max_brightness = BRIGHTNESS_MAX;
			if ( strcmp(led_label, LABEL_MLED) == 0 ) {
				led->cdev.brightness = KCLIGHT_WLED_BOOT_DEFAULT;
				led->param_next.value = led->cdev.brightness;
				led->cdev.max_brightness = BL_BRIGHTNESS_MAX;
				KCLIGHT_N_LOG("brightness=%d", led->cdev.brightness);
			}
			INIT_WORK(&led->work, kclight_work);
			led->parent = data;
			err = led_classdev_register(&client->dev, &led->cdev);
			if (err < 0) {
				KCLIGHT_E_LOG("unable to register led %s", led->cdev.name);
				goto fail_id_check;
			}
			if (strcmp(led_label, LABEL_MLED) == 0) {
				err = sysfs_create_group(&led->cdev.dev->kobj, &kclight_mled_attr_group);
				if (err) {
					KCLIGHT_E_LOG("unable to register sysfs mled %s", led->cdev.name);
					goto fail_id_check;
				}
			}
			led->cdev_registered = true;
		}
	}

	err = misc_register(&leds_device);
	if (err < 0) {
		KCLIGHT_E_LOG("unable to register misc device");
		goto fail_misc_register;
	}

	data->reset_gpio = of_get_named_gpio(client->dev.of_node, "kc,reset-gpio", 0);
	if (!gpio_is_valid(data->reset_gpio)) {
		KCLIGHT_E_LOG("No valid RESET GPIO specified %d", data->reset_gpio);
		err = -ENODEV;
		goto fail_get_reset_gpio;
	}

#ifndef KEEP_DEVICE_STATE_ON_PROBE
	err = gpio_request(data->reset_gpio, KCLIGHT_DRV_NAME);
	KCLIGHT_V_LOG("gpio_request GPIO=%d err=%d", data->reset_gpio, err);
	if (err < 0) {
		KCLIGHT_E_LOG("failed to request GPIO=%d, ret=%d",
				data->reset_gpio,
				err);
		goto fail_request_reset_gpio;
	}
#endif

//	if (gpio_is_valid(data->keyled_gpio)) {
//		err = gpio_request(data->keyled_gpio, KCLIGHT_DRV_NAME);
//		KCLIGHT_V_LOG("gpio_request GPIO=%d err=%d", data->keyled_gpio, err);
//		if (err < 0) {
//			KCLIGHT_E_LOG("failed to request GPIO=%d, ret=%d",
//					data->keyled_gpio,
//					err);
//			goto fail_request_keyled_gpio;
//		}
//	}

	if (gpio_is_valid(data->keyled_on_gpio)) {
		err = gpio_request(data->keyled_on_gpio, KCLIGHT_DRV_NAME);
		KCLIGHT_V_LOG("gpio_request ON GPIO=%d err=%d", data->keyled_on_gpio, err);
		if (err < 0) {
			KCLIGHT_E_LOG("failed to request ON GPIO=%d, ret=%d",
					data->keyled_on_gpio,
					err);
			goto fail_request_keyled_on_gpio;
		}
	}

	err = kclight_init_client(client);
	if (err)
	{
		KCLIGHT_E_LOG("Failed kclight_init_client");
		goto fail_init_client;
	}

	data->colorvariation = COLORVARIATION_DEFAULT;

	data->reboot_notifier.notifier_call = kclight_reboot_notify;
	err = register_reboot_notifier(&data->reboot_notifier);
	if (err) {
		KCLIGHT_E_LOG("[KCLIGHT] %s: failed to register reboot notifier: %d\n", __func__, err);
	}

	kclight_data = data;

	kc_leds_funcs_register(lp5569_kc_leds_funcs);

	KCLIGHT_V_LOG("[OUT]");

	return 0;

fail_init_client:
	if (gpio_is_valid(data->keyled_on_gpio)) {
		gpio_free(data->keyled_on_gpio);
	}
fail_request_keyled_on_gpio:
//	if (gpio_is_valid(data->keyled_gpio)) {
//		gpio_free(data->keyled_gpio);
//	}
//fail_request_keyled_gpio:
#ifndef KEEP_DEVICE_STATE_ON_PROBE
	gpio_free(data->reset_gpio);
fail_request_reset_gpio:
#endif
fail_get_reset_gpio:
	misc_deregister(&leds_device);
fail_misc_register:
fail_id_check:
	for (i = 0; i < LED_TYPE_MAX; i++) {
		led = &data->led_data[i];

		if (led->cdev_registered) {
			led_classdev_unregister(&led->cdev);
			flush_work(&led->work);
			led->cdev_registered = false;
		}
		mutex_destroy(&led->param_next_lock);
	}
	destroy_workqueue(data->work_queue);
	data->work_queue = NULL;
fail_create_workqueue:
	i2c_set_clientdata(client, NULL);
	mutex_destroy(&data->param_lock);
	mutex_destroy(&data->control_lock);
	mutex_destroy(&data->mled_lock);
	wake_lock_destroy(&data->wake_lock);
	kfree(data);
exit:
	KCLIGHT_E_LOG("[OUT] err=%d", err);
	return err;
}

static const struct i2c_device_id kclight_id[] = {
	{ KCLIGHT_DRV_NAME, 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, kclight_id);

static struct of_device_id kclight_match_table[] = {
	{ .compatible = KCLIGHT_DRV_NAME,},
	{ },
};

static struct i2c_driver kclight_driver = {
	.driver = {
		.name   = KCLIGHT_DRV_NAME,
		.owner  = THIS_MODULE,
		.of_match_table = kclight_match_table,
	},
	.probe  = kclight_probe,
	.remove = kclight_remove,
	.shutdown = kclight_shutdown,
	.id_table = kclight_id,
};

static int32_t __init lp5569_led_init(void)
{
	int32_t rc = 0;

	KCLIGHT_V_LOG("[IN]");

	rc = i2c_add_driver(&kclight_driver);
	if (rc != 0) {
		KCLIGHT_E_LOG("can't add i2c driver");
		rc = -ENOTSUPP;
		return rc;
	}

	KCLIGHT_V_LOG("[OUT]");

	return rc;

}

static void __exit lp5569_led_exit(void)
{
	KCLIGHT_V_LOG("[IN]");

	i2c_del_driver(&kclight_driver);

	KCLIGHT_V_LOG("[OUT]");
}

static int32_t lp5569_light_led_disp_set_panel(e_light_main_wled_disp disp_status, e_light_lcd_panel panel_class)
{
	KCLIGHT_V_LOG("[IN] panel_class=0x%x", panel_class);
	
	atomic_set(&g_panel_class,panel_class);

	switch( panel_class ){
	case LIGHT_LCD_PANEL0:
		KCLIGHT_V_LOG("panel class = LIGHT_LCD_PANEL0");
		break;
	case LIGHT_LCD_PANEL1:
		KCLIGHT_V_LOG("panel class = LIGHT_LCD_PANEL1");
		break;
	case LIGHT_LCD_PANEL2:
		KCLIGHT_V_LOG("panel class = LIGHT_LCD_PANEL2");
		break;
	default:
		KCLIGHT_E_LOG("unknown panel class");
		break;
	}

	return light_led_disp_set(disp_status);
}

#ifndef DISABLE_DISP_DETECT

static void kclight_queue_mled_work(void)
{
	struct kclight_data *data = kclight_data;
	struct kclight_led_data *led;

	if (data) {
		led = &data->led_data[LED_TYPE_MLED];
		if (led->cdev_registered) {
			if (led->param_next.value) {
				queue_work(led->workqueue, &led->work);
			}
		}
	}
}

static atomic_t g_display_detect = ATOMIC_INIT(0);

static bool light_led_disp_enabled(void)
{
	return (atomic_read(&g_display_detect) == 1) ? true : false;
}

static int32_t light_led_disp_set(e_light_main_wled_disp disp_status)
{
	int32_t ret = 0;

	KCLIGHT_V_LOG("[IN] disp_status=[0x%x]", (uint32_t)disp_status);

	if ((atomic_read(&g_display_detect)) != 0) {
		KCLIGHT_V_LOG("already determined.");
		return ret;
	}

	switch(disp_status) {
	case LIGHT_MAIN_WLED_LCD_EN:
		KCLIGHT_N_LOG("LIGHT_MAIN_WLED_LCD_EN");
		atomic_set(&g_display_detect,1);
//		kclight_queue_mled_work();
		break;
	case LIGHT_MAIN_WLED_LCD_DIS:
		KCLIGHT_N_LOG("LIGHT_MAIN_WLED_LCD_DIS");
		atomic_set(&g_display_detect,-1);
		break;
	default:
		break;
	}

	KCLIGHT_V_LOG("[OUT] ret=%d", ret);
	return ret;
}

static atomic_t g_detect_lcd_pwren = ATOMIC_INIT(0);

static bool light_lcd_power_off(void)
{
	return (atomic_read(&g_detect_lcd_pwren) == -1) ? true : false;
}

static int32_t lp5569_light_led_disp_power_set(e_light_main_wled_disp disp_status)
{
	struct kclight_data *data = kclight_data;
	struct kclight_led_data *led;
	struct kclight_led_param param_next;
	int32_t ret = 0;

	KCLIGHT_V_LOG("[IN] disp_status=[0x%x]", (uint32_t)disp_status);

	if (!kclight_data) {
		KCLIGHT_E_LOG("kclight_data null");
		return 0;
	}

	switch(disp_status) {
	case LIGHT_MAIN_WLED_LCD_PWREN:
		KCLIGHT_V_LOG("LIGHT_MAIN_WLED_LCD_PWREN");
		atomic_set(&g_detect_lcd_pwren,1);
		if (light_led_disp_enabled()) {
			if (bl_skip_flag) {
				bl_skip_flag = 0;
			} else {
				kclight_queue_mled_work();
			}
		}
		break;
	case LIGHT_MAIN_WLED_LCD_PWRDIS:
		KCLIGHT_V_LOG("LIGHT_MAIN_WLED_LCD_PWRDIS");
		atomic_set(&g_detect_lcd_pwren,-1);
		if (light_led_disp_enabled()) {
			if (data) {
				led = &data->led_data[LED_TYPE_MLED];
				if (led->cdev_registered) {
	                mutex_lock(&led->param_next_lock);
	                memcpy(&param_next, &led->param_next, sizeof(param_next));
	                mutex_unlock(&led->param_next_lock);
				    kclight_ctrl_mled(led, &param_next);
					KCLIGHT_V_LOG(" prev.value=%x  next.value=%x \n", led->param_prev.value,led->param_next.value);
				}
			}
		}
		break;
	default:
		break;
	}

	KCLIGHT_V_LOG("[OUT] ret=%d", ret);
	return ret;
}

#else  /* DISABLE_DISP_DETECT */

static bool light_led_disp_enabled(void)
{
	return true;
}

static int32_t light_led_disp_set(e_light_main_wled_disp disp_status)
{
	KCLIGHT_N_LOG("DISABLE_DISP_DETECT");
	return 0;
}

static int32_t lp5569_light_led_disp_power_set(e_light_main_wled_disp disp_status)
{
	KCLIGHT_N_LOG("invalid DISCARD_BL_LCDPOWER_OFF disp_status=0x%x",  (uint32_t)disp_status);
	return 0;
}

#endif  /* DISABLE_DISP_DETECT */

int32_t lp5569_light_led_disp_test_led(int onoff)
{
	int32_t ret = 0;

	KCLIGHT_N_LOG("[IN] onoff=[0x%x]", (uint32_t)onoff);

	KCLIGHT_N_LOG("[OUT] ret=%d", ret);
	return ret;
}
EXPORT_SYMBOL(lp5569_light_led_disp_test_led);

static void lp5569_set_maxbrightness(int max_level, int enable)
{
	struct kclight_led_data *led;
	struct kclight_data *data = kclight_data;

	pr_notice("[KCLIGHT]%s: max_level=%d enable=%d", __func__, max_level, enable);

	if (!kclight_data) {
		KCLIGHT_E_LOG("kclight_data null");
		return;
	}

	mutex_lock(&data->mled_lock);

	led = &data->led_data[LED_TYPE_MLED];
	if (enable == 1) {
		led->cdev.max_brightness = max_level;
	} else {
		led->cdev.max_brightness = BL_BRIGHTNESS_MAX;
	}

	mutex_unlock(&data->mled_lock);

	kclight_queue_mled_work();
}

module_init(lp5569_led_init);
module_exit(lp5569_led_exit);

MODULE_AUTHOR("KYOCERA Corporation");
MODULE_DESCRIPTION("LED");
MODULE_LICENSE("GPL");

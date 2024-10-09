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

#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/input.h>
#include <linux/platform_device.h>

#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <asm/uaccess.h>
#include <linux/miscdevice.h>
#include <linux/leds.h>
#include <linux/regulator/consumer.h>

#include <linux/types.h>
#include <linux/device.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/workqueue.h>
#include <linux/of.h>
#include <linux/list.h>
#include "../../flashlight/flashlight-core.h"
#include "../../flashlight/flashlight-dt.h"

#define KCLIGHT_DRV_NAME "kc,kc_light_ktd2687"

#define I2C_RETRIES_NUM				(5)
#define GPIO_REQUEST_RETRIES_NUM	(10)
#define GPIO_REQUEST_RETRIES_DELAY	(20)

#define TORCH_BRIGHTNESS_LEVEL_MAX	5
#define I2C_RETRIES_NUM				(5)
#define KCLIGHT_I2C_WRITE_MSG_NUM	(1)
#define KCLIGHT_I2C_READ_MSG_NUM		(2)
#define KCLIGHT_WRITE_BUF_LEN		(2)

#define PIN_CONTROL_SKIP	0x80


static uint8_t fled_reg_torch_level[TORCH_BRIGHTNESS_LEVEL_MAX] =
{
	0x0D,	//level 1
	0x1A,	//level 2
	0x28,	//level 3
	0x36,	//level 4
	0x3D,	//level 5
};

struct fled_data {
	//struct platform_device			*pdev;
	struct i2c_client			*client;
	struct led_classdev			cdev;
	struct mutex				fled_lock;
	int							reset_gpio;
	int							strb0_gpio;
	int							strb1_gpio;
	int							state;
	int							torch_light_level;
	bool						torch_light_level_fixed;
	bool						pincontrol_skip_flag;
	
	struct pinctrl				*pinctrl;
	struct pinctrl_state		*i2c_active;
	struct pinctrl_state		*i2c_suspend;
};

extern int ktd2687_open(void);
extern int ktd2687_release(void);
extern int ktd2687_ioctl(unsigned int cmd, unsigned long arg);
extern ssize_t ktd2687_strobe_store(struct flashlight_arg arg);
extern int ktd2687_set_driver(int set);
static struct flashlight_operations ktd2687_ops = {
	ktd2687_open,
	ktd2687_release,
	ktd2687_ioctl,
	ktd2687_strobe_store,
	ktd2687_set_driver
};

/****************************************************************************s
 * DEBUG MACROS
 ***************************************************************************/

// debug
// #define KCLIGHT_DEBUG 1

#ifdef KCLIGHT_DEBUG
#define KCLIGHT_D_LOG(msg, ...)	\
	pr_err("[LEDDRV][%s][D](%d): " msg "\n", __func__, __LINE__, ## __VA_ARGS__)
#else
#define KCLIGHT_D_LOG(msg, ...)	\
	pr_debug("[LEDDRV][%s][D](%d): " msg "\n", __func__, __LINE__, ## __VA_ARGS__)
#endif

#define KCLIGHT_E_LOG(msg, ...)	\
	pr_err   ("[LEDDRV][%s][E](%d): " msg "\n", __func__, __LINE__, ## __VA_ARGS__)
#define KCLIGHT_N_LOG(msg, ...)	\
	pr_notice("[LEDDRV][%s][N](%d): " msg "\n", __func__, __LINE__, ## __VA_ARGS__)


#define FLED_OFF_VAL 0x00
#define FLED_FLASH_DEFAULT_REG 0x03
#define FLED_FLASH_DEFAULT_VAL 0x10
#define FLED_TORCH_REG 0x05
#define FLED_TORCH_DEFAULT_VAL 0x3D
#define FLED_FLASH_TIMER_DEFAULT_REG 0x08
#define FLED_FLASH_TIMER_DEFAULT_VAL 0x1D
#define FLED_SIGNAL_LED1_DEFAULT_REG 0x01
#define FLED_SIGNAL_LED1_DEFAULT_VAL 0x31

static void kclight_gpio_control(struct fled_data *data, int gpio, int level){
	gpio_set_value(gpio, level);
	
	if(gpio == data->reset_gpio)
		KCLIGHT_D_LOG("reset_gpio=%d",level);
	else if(gpio == data->strb0_gpio)
		KCLIGHT_D_LOG("strb0_gpio=%d",level);
	else if(gpio == data->strb1_gpio)
		KCLIGHT_D_LOG("strb1_gpio=%d",level);
	
}


static int kclight_i2c_write(struct fled_data *data, uint8_t uc_reg, uint8_t uc_val)
{
	int ret = 0;
	int retry = 0;
	struct i2c_msg i2cMsg;
	u8 ucwritebuf[KCLIGHT_WRITE_BUF_LEN];

	KCLIGHT_D_LOG("[IN] client=0x%p reg=0x%02x val=0x%02X", data->client, uc_reg, uc_val);

	if (data->client == NULL) {
		KCLIGHT_E_LOG("fail client=0x%p", data->client);
		return -ENODEV;
	}

	ucwritebuf[0] = uc_reg;
	ucwritebuf[1] = uc_val;
	i2cMsg.addr  = data->client->addr;
	i2cMsg.flags = 0;
	i2cMsg.len   =  sizeof(ucwritebuf);
	i2cMsg.buf   =  &ucwritebuf[0];

	KCLIGHT_D_LOG("i2c write reg=0x%02x,value=0x%02x", uc_reg, uc_val);

	do {
		ret = i2c_transfer(data->client->adapter, &i2cMsg, KCLIGHT_I2C_WRITE_MSG_NUM);
		KCLIGHT_D_LOG("i2c_transfer() call end ret=%d", ret);
		
		if(ret == KCLIGHT_I2C_WRITE_MSG_NUM)
			break;
		
		KCLIGHT_E_LOG("i2c error deteck.gpio set i2c function.");
		pinctrl_select_state(data->pinctrl,data->i2c_active);
		data->pincontrol_skip_flag = false;
		
	} while (++retry < I2C_RETRIES_NUM);

	KCLIGHT_D_LOG("[OUT] ret=%d", ret);

	return ret;
}

static void fled_common(struct fled_data *data)
{
	KCLIGHT_D_LOG("[IN]");

    if(data->pincontrol_skip_flag == false){
	    pinctrl_select_state(data->pinctrl,data->i2c_active);
        usleep_range(1, 1);
    }
	kclight_gpio_control(data,data->reset_gpio,1);
	usleep_range(1000, 1000);

/* change Flash mA */
	kclight_i2c_write(data, FLED_FLASH_DEFAULT_REG, FLED_FLASH_DEFAULT_VAL);
/* change torch mA */
	kclight_i2c_write(data, FLED_TORCH_REG, FLED_TORCH_DEFAULT_VAL);
/* change Flash timer */
	kclight_i2c_write(data, FLED_FLASH_TIMER_DEFAULT_REG, FLED_FLASH_TIMER_DEFAULT_VAL);
/* change Flash sig. LED1 enable */
	kclight_i2c_write(data, FLED_SIGNAL_LED1_DEFAULT_REG, FLED_SIGNAL_LED1_DEFAULT_VAL);

	KCLIGHT_D_LOG("[OUT]");
}

static void fled_off(struct fled_data *data)
{
	KCLIGHT_D_LOG("[IN]");

	kclight_gpio_control(data,data->reset_gpio,1);
	usleep_range(100, 200);
	kclight_gpio_control(data,data->strb0_gpio, 0);
	kclight_gpio_control(data,data->strb1_gpio, 0);

	usleep_range(100, 200);
	kclight_gpio_control(data,data->reset_gpio, 0);

	if(data->pincontrol_skip_flag == false)
		pinctrl_select_state(data->pinctrl,data->i2c_suspend);
	
	data->pincontrol_skip_flag = false;

	KCLIGHT_D_LOG("[OUT]");
}

static void fled_on_flash(struct fled_data *data)
{
	KCLIGHT_D_LOG("[IN]");
	
	fled_common(data);
	
	kclight_gpio_control(data,data->strb0_gpio, 1);
	usleep_range(100, 200);

	KCLIGHT_D_LOG("[OUT]");
}

static void fled_on_torch(struct fled_data *data, int level)
{
	KCLIGHT_D_LOG("[IN] level=[%d]", level);
	fled_common(data);
/* set torch mA */
	kclight_i2c_write(data, FLED_TORCH_REG, fled_reg_torch_level[level]);

	kclight_gpio_control(data,data->strb1_gpio, 1);
	usleep_range(100, 200);

	//pinctrl_select_state(data->pinctrl,data->i2c_suspend);

	KCLIGHT_D_LOG("[OUT]");
}

static void led_set(struct led_classdev *cdev, enum led_brightness value)
{
	struct fled_data *data =
		container_of(cdev, struct fled_data, cdev);

	KCLIGHT_D_LOG("[IN] name=%s value=0x%08x", cdev->name, value);
	
	if(value & PIN_CONTROL_SKIP){
		data->pincontrol_skip_flag = true;
		value &= ~PIN_CONTROL_SKIP;
	}
	
	mutex_lock(&data->fled_lock);

	if (!data->torch_light_level_fixed)
		data->torch_light_level = value - 1;

	fled_off(data);
	if ((value > 0)&&(value <= TORCH_BRIGHTNESS_LEVEL_MAX)) {
		fled_on_torch(data, data->torch_light_level);
	} else if (value == 0x10) {
		fled_on_flash(data);
	}

	mutex_unlock(&data->fled_lock);

	KCLIGHT_D_LOG("[OUT]");
}

static enum led_brightness led_get(struct led_classdev *cdev)
{
	int32_t lret = 0;
	struct fled_data *data =
		container_of(cdev, struct fled_data, cdev);

	KCLIGHT_D_LOG("[IN] name=%s", cdev->name);

	lret = data->cdev.brightness;

	KCLIGHT_D_LOG("[OUT] lret=0x%08x", lret);

	return lret;
}

static int kc_light_ktd2687_probe(struct i2c_client *client,
				     const struct i2c_device_id *id)
{
	
	
	u32 read_value;
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct device_node *node;
	struct fled_data *data;
	int ret = 0;

	if (!i2c_check_functionality(adapter, I2C_FUNC_I2C)) {
		ret = -EIO;
		KCLIGHT_E_LOG("fail i2c_check_functionality");
		dev_err(&client->dev, "i2c_check_functionality error\n");
		goto exit;
	}

	node = client->dev.of_node;
	if (node == NULL){
		KCLIGHT_E_LOG("client->dev.of_node == null");
		ret = -ENODEV;
		goto exit;
	}
	
	data = kzalloc(sizeof(struct fled_data), GFP_KERNEL);
	if (!data) {
		KCLIGHT_E_LOG("Failed kzalloc");
		ret = -ENOMEM;
		goto exit;
	}
	
	
	mutex_init(&data->fled_lock);
	
	data->client = client;
	i2c_set_clientdata(client, data);
	
	data->cdev.name			  = "mobilelightinfo";
	data->cdev.brightness_set = led_set;
	data->cdev.brightness_get = led_get;
	data->cdev.brightness     = 0;
	data->cdev.max_brightness = 0xffffffff;

	data->pinctrl = devm_pinctrl_get(&client->dev);
	
	if (IS_ERR_OR_NULL(data->pinctrl)) {
		pr_err("%s: failed to get pinctrl\n", __func__);
		return PTR_ERR(data->pinctrl);
	}

	ret = led_classdev_register(&client->dev, &data->cdev);
	if (ret < 0) {
		KCLIGHT_E_LOG("unable to register led %s", data->cdev.name);
		goto fail_led_classdev_register;
	}

	data->reset_gpio = of_get_named_gpio(client->dev.of_node, "kc,flash_reset", 0);
	if (!gpio_is_valid(data->reset_gpio)) {
		KCLIGHT_E_LOG("No valid RESET GPIO specified %d", data->reset_gpio);
		ret = -ENODEV;
		goto fail_fled_get_gpio;
	}
	KCLIGHT_E_LOG("reset_gpio = %d", data->reset_gpio);

	data->strb0_gpio = of_get_named_gpio(client->dev.of_node, "kc,flash_strb0", 0);
	if (!gpio_is_valid(data->strb0_gpio)) {
		KCLIGHT_E_LOG("No valid STRB0 GPIO specified %d", data->strb0_gpio);
		ret = -ENODEV;
		goto fail_fled_get_gpio;
	}
	KCLIGHT_E_LOG("strb0_gpio = %d", data->strb0_gpio);

	data->strb1_gpio = of_get_named_gpio(client->dev.of_node, "kc,flash_strb1", 0);
	if (!gpio_is_valid(data->strb1_gpio)) {
		KCLIGHT_E_LOG("No valid STRB1 GPIO specified %d", data->strb1_gpio);
		ret = -ENODEV;
		goto fail_fled_get_gpio;
	}
	KCLIGHT_E_LOG("strb1_gpio = %d", data->strb1_gpio);

	if (of_property_read_u32(client->dev.of_node, "torch_light_level", &read_value) == 0)
		data->torch_light_level = read_value;
	else
		data->torch_light_level = 0;

	data->torch_light_level_fixed = of_property_read_bool(client->dev.of_node, "torch_light_level_fixed");


	ret = gpio_request(data->reset_gpio, KCLIGHT_DRV_NAME);
	if (ret < 0) {
		KCLIGHT_E_LOG("failed to request GPIO=%d, ret=%d",
				data->reset_gpio, ret);
		goto fail_fled_req_reset_gpio;
	}

	ret = gpio_request(data->strb0_gpio, KCLIGHT_DRV_NAME);
	if (ret < 0) {
		KCLIGHT_E_LOG("failed to request GPIO=%d, ret=%d",
				data->strb0_gpio, ret);
		goto fail_fled_req_strb0_gpio;
	}

	ret = gpio_request(data->strb1_gpio, KCLIGHT_DRV_NAME);
	if (ret < 0) {
		KCLIGHT_E_LOG("failed to request GPIO=%d, ret=%d",
				data->strb1_gpio, ret);
		goto fail_fled_req_strb1_gpio;
	}

	if (!IS_ERR_OR_NULL(data->pinctrl)) {
		data->i2c_active = pinctrl_lookup_state(data->pinctrl,"i2c_active");

		if (IS_ERR_OR_NULL(data->i2c_active))
		{
			pr_err("%s: i2c_active can not get pinstate\n", __func__);
			goto fail_fled_req_strb1_gpio;
		}
	}
	
	if (!IS_ERR_OR_NULL(data->pinctrl)) {
//		data->i2c_suspend = pinctrl_lookup_state(data->pinctrl,"i2c_suspend");
		data->i2c_suspend = pinctrl_lookup_state(data->pinctrl,"i2c_active");

		if (IS_ERR_OR_NULL(data->i2c_suspend))
		{
			pr_err("%s: i2c_suspend can not get pinstate\n", __func__);
			goto fail_fled_req_strb1_gpio;
		}
	}


	kclight_gpio_control(data,data->reset_gpio, 0);
	kclight_gpio_control(data,data->strb0_gpio, 0);
	kclight_gpio_control(data,data->strb1_gpio, 0);

	data->state = 0;
	data->pincontrol_skip_flag = false;

	/* register flashlight operations */
	if (flashlight_dev_register("kc_light_ktd2687", &ktd2687_ops)) {
	     printk("Failed to add devices register.\n");
	     return -1;
	}
	KCLIGHT_N_LOG("[OUT]");
	return 0;

fail_fled_req_strb1_gpio:
	gpio_free(data->strb0_gpio);
fail_fled_req_strb0_gpio:
	gpio_free(data->reset_gpio);
fail_fled_req_reset_gpio:
fail_fled_get_gpio:
fail_led_classdev_register:
	mutex_destroy(&data->fled_lock);
	kfree(data);

exit:
	KCLIGHT_E_LOG("[OUT] ret=%d", ret);
	return ret;

}

static int kc_light_ktd2687_remove(struct i2c_client *client)
{
	struct fled_data *data = i2c_get_clientdata(client);

	KCLIGHT_D_LOG("[IN]");

	
	kclight_gpio_control(data,data->reset_gpio, 0);
	usleep_range(3000, 4000);
	
	gpio_free(data->reset_gpio);
	gpio_free(data->strb0_gpio);
	gpio_free(data->strb1_gpio);

	//mutex_unlock(&data->fled_lock);
	//mutex_destroy(&data->fled_lock);
	
	i2c_set_clientdata(client, NULL);

	/* unregister flashlight operations */
	flashlight_dev_unregister("kc_light_ktd2687");

	KCLIGHT_D_LOG("[OUT]");

	return 0;
}

static const struct i2c_device_id kclight_id[] = {
	{ KCLIGHT_DRV_NAME, 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, kclight_id);

static struct of_device_id kclight_match_table[] = {
	{ .compatible = "mediatek,kc_light_ktd2687",},
	{ },
};



static struct i2c_driver kc_light_ktd2687_driver = {
	.driver	= {
		.name	= KCLIGHT_DRV_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = kclight_match_table,
	},

	.probe		= kc_light_ktd2687_probe,
	.remove		= kc_light_ktd2687_remove,
	.id_table	=  kclight_id,
};

static int __init kc_light_ktd2687_init(void)
{
	return i2c_add_driver(&kc_light_ktd2687_driver);
}

static void __exit kc_light_ktd2687_exit(void)
{
	i2c_del_driver(&kc_light_ktd2687_driver);
}

module_init(kc_light_ktd2687_init);
module_exit(kc_light_ktd2687_exit);

MODULE_AUTHOR("Kyocera Inc.");
MODULE_DESCRIPTION("KC LED driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("leds-kinetic");


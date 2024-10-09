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

#include <linux/errno.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/leds.h>
#include <linux/mutex.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

#include <asm/uaccess.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/init.h>

#define KCKEYLIGHT_DRV_NAME "kc,kc_keylight"

#define LIGHT_OFF   0
#define TRI_LED      0x01
#define SUB_LCD     0x02
#define KEY_LED     0x04
#define LED_MASK    0x07
#define POWER_ON    true
#define POWER_OFF   false

/****************************************************************************s
 * DEBUG MACROS
 ***************************************************************************/
// debug
#define KCKEYLIGHT_DEBUG 1
#ifdef KCKEYLIGHT_DEBUG
#define KCKEYLIGHT_D_LOG(msg, ...)	\
	pr_err("[KEYLEDDRV][%s][D](%d): " msg "\n", __func__, __LINE__, ## __VA_ARGS__)
#else
#define KCKEYLIGHT_D_LOG(msg, ...)	\
	pr_debug("[KEYLEDDRV][%s][D](%d): " msg "\n", __func__, __LINE__, ## __VA_ARGS__)
#endif

#define KCKEYLIGHT_E_LOG(msg, ...)	\
	pr_err   ("[KEYLEDDRV][%s][E](%d): " msg "\n", __func__, __LINE__, ## __VA_ARGS__)
#define KCKEYLIGHT_N_LOG(msg, ...)	\
	pr_notice("[KEYLEDDRV][%s][N](%d): " msg "\n", __func__, __LINE__, ## __VA_ARGS__)


struct keylight_data {
    struct platform_device		*pdev;
    struct led_classdev			cdev;
    struct mutex				keylight_lock;
    struct mutex				power_lock;
    bool						keylight_state;
    int                         keyled_gpio;
    int                         power_gpio;
    int                         power_state;
};

static struct keylight_data *kdata;

void VLED_POWER(int led_type, bool on, struct keylight_data *data)
{
    KCKEYLIGHT_D_LOG("[IN]");
    mutex_lock(&data->power_lock);
    if (on){
        if (data->power_state == LIGHT_OFF) {
            gpio_set_value(data->power_gpio, 1);
        }
        data->power_state |= led_type;
    } else {
        data->power_state ^= led_type;
        if (data->power_state == LIGHT_OFF) {
            gpio_set_value(data->power_gpio, 0);
        }
    }
    mutex_unlock(&data->power_lock);
    KCKEYLIGHT_D_LOG("[OUT]");
}

static void keylight_ctrl(struct led_classdev *cdev, enum led_brightness on)
{
	struct keylight_data *data =
		container_of(cdev, struct keylight_data, cdev);
    KCKEYLIGHT_D_LOG("[IN]");
    mutex_lock(&data->keylight_lock);
    if (on){
        if (data->keylight_state == POWER_OFF) {
            VLED_POWER(KEY_LED, POWER_ON, data);
            gpio_set_value(data->keyled_gpio, 1);
            data->keylight_state = true;
        }
    } else {//off
        if (data->keylight_state == POWER_ON) {
            gpio_set_value(data->keyled_gpio, 0);
            data->keylight_state = false;
            VLED_POWER(KEY_LED, POWER_OFF, data);
        }
    }
    KCKEYLIGHT_D_LOG("keylight_state: %d power_state: %d",data->keylight_state, data->power_state);
    mutex_unlock(&data->keylight_lock);
    KCKEYLIGHT_D_LOG("[OUT]");
}

void VLED_POWER_CTRL(int led_type, bool on)
{
    if (on) {
        VLED_POWER(led_type, POWER_ON, kdata);
    } else {//off
        VLED_POWER(led_type, POWER_OFF, kdata);
    }
}    

static int kc_keylight_probe(struct platform_device *pdev)
{
    struct keylight_data *data;
	int ret = 0;
    int gpio_req = 0;
    KCKEYLIGHT_D_LOG("[IN]");

	data = kzalloc(sizeof(struct keylight_data), GFP_KERNEL);
	if (!data) {
		KCKEYLIGHT_E_LOG("Failed kzalloc");
		ret = -ENOMEM;
		goto exit;
	}
	kdata = data;

    mutex_init(&data->keylight_lock);
    mutex_init(&data->power_lock);
    data->pdev = pdev;
    platform_set_drvdata(pdev, data);
    data->cdev.name       = "button-backlight";
    data->cdev.brightness_set = keylight_ctrl;

    data->keylight_state = 0x00;

	ret = led_classdev_register(&pdev->dev, &data->cdev);
	if (ret < 0) {
		KCKEYLIGHT_E_LOG("unable to register led %s", data->cdev.name);
		goto fail_led_classdev_register;
    }

    data->power_gpio = of_get_named_gpio(pdev->dev.of_node, "kc,power_gpio", 0);
	if (!gpio_is_valid(data->power_gpio)) {
		KCKEYLIGHT_E_LOG("No valid POWER GPIO specified %d", data->power_gpio);
		ret = -ENODEV;
		goto fail_keylight_get_gpio;
	}
	KCKEYLIGHT_E_LOG("power_gpio = %d", data->power_gpio);

    data->keyled_gpio = of_get_named_gpio(pdev->dev.of_node, "kc,keyled_gpio", 0);
	if (!gpio_is_valid(data->keyled_gpio)) {
		KCKEYLIGHT_E_LOG("No valid KEYLED GPIO specified %d", data->keyled_gpio);
		ret = -ENODEV;
		goto fail_keylight_get_gpio;
	}
	KCKEYLIGHT_E_LOG("keyled_gpio = %d", data->keyled_gpio);

    gpio_req = gpio_request(data->power_gpio, KCKEYLIGHT_DRV_NAME);
    if(gpio_get_value(data->power_gpio)) {
        data->power_state |= TRI_LED;
    }
    KCKEYLIGHT_E_LOG("data->power_gpio gpio_req = %d", gpio_req);

    gpio_req = gpio_request(data->keyled_gpio, KCKEYLIGHT_DRV_NAME);
    KCKEYLIGHT_E_LOG("data->keyled_gpio gpio_req = %d", gpio_req);

fail_keylight_get_gpio:
fail_led_classdev_register:
	mutex_destroy(&data->keylight_lock);

exit:
	KCKEYLIGHT_E_LOG("[OUT] ret=%d", ret);
	return ret;
}

static int kc_keylight_remove(struct platform_device *pdev)
{
    // struct fled_data *data = platform_get_drvdata(pdev);
    // gpio_free(data->power_gpio);
    // gpio_free(data->keyled_gpio);
    return 0;
}

static struct of_device_id KCKEYLIGHT_match_table[] = {
    {.compatible = KCKEYLIGHT_DRV_NAME,},
    {}
};
MODULE_DEVICE_TABLE(of, KCKEYLIGHT_match_table);

static struct platform_driver kc_keylight_driver = {
    .driver = {
        .name = "kc_keylight",
        .owner = THIS_MODULE,
        .of_match_table = KCKEYLIGHT_match_table,
    },
    .probe = kc_keylight_probe,
    .remove = kc_keylight_remove,
};

static int __init kc_keylight_init(void)
{
	int32_t rc = 0;
	KCKEYLIGHT_D_LOG("[IN]");
	rc = platform_driver_register(&kc_keylight_driver);
	if (rc != 0) {
		KCKEYLIGHT_E_LOG("can't add platform driver");
		rc = -ENOTSUPP;
		return rc;
	}
	KCKEYLIGHT_D_LOG("[OUT]");
	return 0;
}

static void __exit kc_keylight_exit(void)
{
    KCKEYLIGHT_D_LOG("[IN]");
	platform_driver_unregister(&kc_keylight_driver);
	KCKEYLIGHT_D_LOG("[OUT]");
}

module_init(kc_keylight_init);
module_exit(kc_keylight_exit);
// MODULE_ALIAS("leds-kinetic");

MODULE_AUTHOR("Kyocera Inc.");
MODULE_DESCRIPTION("KC KEYLIGHT driver");
MODULE_LICENSE("GPL");

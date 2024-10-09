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
#include <linux/slab.h> //確認

#include <asm/uaccess.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/init.h>

#define KCSLCDBL_DRV_NAME "kc,kc_slcdbl"

#define LIGHT_OFF   0
#define TRI_LED      0x01
#define SUB_LCD     0x02
#define KEY_LED     0x04
#define LED_MASK    0x07
#define POWER_ON    true
#define POWER_OFF   false

//#define LED_ON		0x01
//#define LED_OFF		0x00

/****************************************************************************s
 * DEBUG MACROS
 ***************************************************************************/
// debug
#define KCSLCDBL_DEBUG 1
#ifdef KCSLCDBL_DEBUG
#define KCSLCDBL_D_LOG(msg, ...)	\
	pr_err("[KEYLEDDRV][%s][D](%d): " msg "\n", __func__, __LINE__, ## __VA_ARGS__)
#else
#define KCSLCDBL_D_LOG(msg, ...)	\
	pr_debug("[KEYLEDDRV][%s][D](%d): " msg "\n", __func__, __LINE__, ## __VA_ARGS__)
#endif

#define KCSLCDBL_E_LOG(msg, ...)	\
	pr_err   ("[KEYLEDDRV][%s][E](%d): " msg "\n", __func__, __LINE__, ## __VA_ARGS__)
#define KCSLCDBL_N_LOG(msg, ...)	\
	pr_notice("[KEYLEDDRV][%s][N](%d): " msg "\n", __func__, __LINE__, ## __VA_ARGS__)


struct slcdbl_data {
    struct platform_device		*pdev;
    struct led_classdev			cdev;
    struct mutex				slcdbl_lock;
//    struct mutex				power_lock;
    bool						slcdbl_state; //keyledのonoff
    int                         slcdbl_cnt_gpio;
//    int                         power_gpio;
//    int                         power_state; //led 
};

//static struct slcdbl_data *the_data = NULL;

//extern struct keylight_data *kdata;

//extern struct keylight_data *keylight_data_get(void);

extern void VLED_POWER_CTRL(int led_type, bool on);

static void slcdbl_ctrl(struct led_classdev *cdev, enum led_brightness on)
{
    bool ON = true;
    bool OFF = false;
//	struct keylight_data *kdata;
//    struct keylight_data *kdata =
//		container_of(cdev, struct keylight_data, cdev);
    struct slcdbl_data *data =
		container_of(cdev, struct slcdbl_data, cdev);
	
    KCSLCDBL_D_LOG("[IN]");
//    kdata = keylight_data_get();
//    mutex_lock(&data->slcdbl_lock);
    if (on){
        if (data->slcdbl_state == POWER_OFF) {
            VLED_POWER_CTRL(SUB_LCD, ON);
//            VLED_POWER(SUB_LCD, ON, kdata);
            gpio_set_value(data->slcdbl_cnt_gpio, 1);
            data->slcdbl_state = true;
        }
    } else {//off
        if (data->slcdbl_state == POWER_ON) {
            gpio_set_value(data->slcdbl_cnt_gpio, 0);
            data->slcdbl_state = false;
            VLED_POWER_CTRL(SUB_LCD, OFF);
//            VLED_POWER(SUB_LCD, OFF, kdata);
        }
    }
    KCSLCDBL_D_LOG("slcdbl_state: %d",data->slcdbl_state);
//    KCSLCDBL_D_LOG("slcdbl_state: %d power_state: %d",data->slcdbl_state, kdata->power_state);
//    mutex_unlock(&data->slcdbl_lock);
    KCSLCDBL_D_LOG("[OUT]");
}

static int kc_slcdbl_probe(struct platform_device *pdev)
{
    struct slcdbl_data *data;
	int ret = 0;
    int gpio_req = 0;
    KCSLCDBL_D_LOG("[IN]");

	data = kzalloc(sizeof(struct slcdbl_data), GFP_KERNEL);
	if (!data) {
		KCSLCDBL_E_LOG("Failed kzalloc");
		ret = -ENOMEM;
		goto exit;
	}

    mutex_init(&data->slcdbl_lock);
//    mutex_init(&data->power_lock);
    data->pdev = pdev;
    platform_set_drvdata(pdev, data);
    data->cdev.name       = "subbacklightinfo";
    data->cdev.brightness_set = slcdbl_ctrl;

    data->slcdbl_state = 0x00;

	ret = led_classdev_register(&pdev->dev, &data->cdev);
	if (ret < 0) {
		KCSLCDBL_E_LOG("unable to register led %s", data->cdev.name);
		goto fail_led_classdev_register;
    }

    // 関数にまとめてOK
    // dtsの名前と構造体の名前を統一する
//    data->power_gpio = of_get_named_gpio(pdev->dev.of_node, "kc,power_gpio", 0);
//	if (!gpio_is_valid(data->power_gpio)) {
//		KCSLCDBL_E_LOG("No valid POWER GPIO specified %d", data->power_gpio);
//		ret = -ENODEV;
//		goto fail_slcdbl_get_gpio;
//	}
//	KCSLCDBL_E_LOG("power_gpio = %d", data->power_gpio);

    // dtsの名前と構造体の名前を統一する
    data->slcdbl_cnt_gpio = of_get_named_gpio(pdev->dev.of_node, "kc,slcdbl_cnt_gpio", 0);
	if (!gpio_is_valid(data->slcdbl_cnt_gpio)) {
		KCSLCDBL_E_LOG("No valid KEYLED GPIO specified %d", data->slcdbl_cnt_gpio);
		ret = -ENODEV;
		goto fail_slcdbl_get_gpio;
	}
	KCSLCDBL_E_LOG("slcdbl_cnt_gpio = %d", data->slcdbl_cnt_gpio);

//    gpio_req = gpio_request(data->power_gpio, KCSLCDBL_DRV_NAME);
//    if(gpio_get_value(data->power_gpio)) {
//        data->power_state |= TRI_LED;
//    }
//    KCSLCDBL_E_LOG("data->power_gpio gpio_req = %d", gpio_req);

    gpio_req = gpio_request(data->slcdbl_cnt_gpio, KCSLCDBL_DRV_NAME);
    KCSLCDBL_E_LOG("data->slcdbl_cnt_gpio gpio_req = %d", gpio_req);

//    the_data = data;

fail_slcdbl_get_gpio:
fail_led_classdev_register:
	mutex_destroy(&data->slcdbl_lock);

exit:
	KCSLCDBL_E_LOG("[OUT] ret=%d", ret);
	return ret;
}

static int kc_slcdbl_remove(struct platform_device *pdev)
{
    // gpio freeあった方が良い
    return 0;
}

static struct of_device_id KCSLCDBL_match_table[] = {
    {.compatible = KCSLCDBL_DRV_NAME,},
    {}
};
MODULE_DEVICE_TABLE(of, KCSLCDBL_match_table);

static struct platform_driver kc_slcdbl_driver = {
    .driver = {
        .name = "kc_slcdbl",
        .owner = THIS_MODULE,
        .of_match_table = KCSLCDBL_match_table,
    },
    .probe = kc_slcdbl_probe,
    .remove = kc_slcdbl_remove,
};

static int __init kc_slcdbl_init(void)
{
	int32_t rc = 0;
	KCSLCDBL_D_LOG("[IN]");
	rc = platform_driver_register(&kc_slcdbl_driver);
	if (rc != 0) {
		KCSLCDBL_E_LOG("can't add platform driver");
		rc = -ENOTSUPP;
		return rc;
	}
	KCSLCDBL_D_LOG("[OUT]");
	return 0;
}

static void __exit kc_slcdbl_exit(void)
{
    KCSLCDBL_D_LOG("[IN]");
	platform_driver_unregister(&kc_slcdbl_driver);
	KCSLCDBL_D_LOG("[OUT]");
}

module_init(kc_slcdbl_init);
module_exit(kc_slcdbl_exit);
// MODULE_ALIAS("leds-kinetic");

MODULE_AUTHOR("Kyocera Inc.");
MODULE_DESCRIPTION("KC SLCDBL driver");
MODULE_LICENSE("GPL");
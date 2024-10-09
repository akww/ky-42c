/*
2022-5-20 camera flashlight add
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
#include <linux/i2c.h>
#include "../../flashlight/flashlight-core.h"
#include "../../flashlight/flashlight-dt.h"

/* define mutex, work queue and timer */
static DEFINE_MUTEX(ktd2687_mutex);

/* define usage count */
static int use_count;

/* flashlight disable function */
static int ktd2687_disable(void)
{
	return 0;
}

/* flashlight enable function */
static int ktd2687_enable(void)
{
	return 0;
}

/* flashlight init */
int ktd2687_init(void)
{
	return 0;
}

/* flashlight uninit */
int ktd2687_uninit(void)
{
	ktd2687_disable();

	return 0;
}

/* set flashlight level */
static int ktd2687_set_level(void)
{
	return 0;
}

 /******************************************************************************
 * Flashlight operations
 *****************************************************************************/
extern int ktd2687_ioctl(unsigned int cmd, unsigned long arg)
{
	switch (cmd) {
	case FLASH_IOC_SET_TIME_OUT_TIME_MS:
		break;

	case FLASH_IOC_SET_DUTY:
		break;

	case FLASH_IOC_SET_ONOFF:
		break;

	case FLASH_IOC_GET_DUTY_NUMBER:
		break;

	case FLASH_IOC_GET_MAX_TORCH_DUTY:
		break;

	case FLASH_IOC_GET_DUTY_CURRENT:
		break;

	case FLASH_IOC_GET_HW_TIMEOUT:
		break;

	case FLASH_IOC_GET_HW_FAULT:
		break;

	default:
		break;
	}
	return 0;
}

extern int ktd2687_open(void)
{
	return 0;
}

extern int ktd2687_release(void)
{
	return 0;
}

extern int ktd2687_set_driver(int set)
{
	int ret = 0;

	/* set chip and usage count */
	mutex_lock(&ktd2687_mutex);
	if (set) {
		if (!use_count)
			ret = ktd2687_init();
		use_count++;
		pr_debug("Set driver: %d\n", use_count);
	} else {
		use_count--;
		if (!use_count)
			ret = ktd2687_uninit();
		if (use_count < 0)
			use_count = 0;
		pr_debug("Unset driver: %d\n", use_count);
	}
	mutex_unlock(&ktd2687_mutex);

	return ret;
}

extern ssize_t ktd2687_strobe_store(struct flashlight_arg arg)
{
	ktd2687_set_driver(1);
	ktd2687_set_level();
	ktd2687_enable();
	msleep(arg.dur);
	ktd2687_disable();
	ktd2687_release();

	return 0;
}
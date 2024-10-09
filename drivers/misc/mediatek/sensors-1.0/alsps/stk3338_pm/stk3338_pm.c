/*
 * This software is contributed or developed by KYOCERA Corporation.
 * (C) 2013 KYOCERA Corporation
 * (C) 2015 KYOCERA Corporation
 * (C) 2017 KYOCERA Corporation
 * (C) 2018 KYOCERA Corporation
 * (C) 2019 KYOCERA Corporation
 * (C) 2020 KYOCERA Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/sched.h>
#include <linux/fs.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/pm.h>
#include <linux/gpio.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/cdev.h>
#include <linux/interrupt.h>

#include <linux/pinctrl/machine.h>
#include "stk3338_pm.h"

/* delay time (msec) */
#define STK3338_DEVICE_PON_WAITTIME1                 (1)
#define STK3338_DEVICE_PON_WAITTIME2                 (30)
#define IRQ_DETECT 1
#define IRQ_CLEAR 0

int gpio_of_irq = -1;
int irq_flag = -1;

struct stk3338_pm_private_data {
	struct cdev cdev;
	struct device *dd;
	struct stk3338_pm_drvdata *drvdata;
//	struct pinctrl_state *vprox18_active;
	struct pinctrl_state *vprox32_active;
//	struct pinctrl_state *vprox18_sleep;
	struct pinctrl_state *vprox32_sleep;
};

struct stk3338_pm_drvdata {
       struct platform_device *pdev;
       struct stk3338_pm_private_data private_data;
};

static struct class *stk3338_pm_class;
static dev_t stk3338_pm_minor;

/* forward declarations */
static int stk3338_pm_open(struct inode *inode, struct file *filp);
static ssize_t stk3338_pm_write(struct file *, const char __user *, size_t, loff_t *);


/* file operations */
static const struct file_operations stk3338_pm_fops = {
       .owner          = THIS_MODULE,
       .open           = stk3338_pm_open,
       .write          = stk3338_pm_write,
};

static ssize_t irq_flag_show(struct device *dev,struct device_attribute *attr,char *buf)
{
       dev_info(dev, "%s\n", __func__);
	return scnprintf(buf, PAGE_SIZE, "%i\n", irq_flag);
}

static ssize_t irq_flag_store(struct device *dev,struct device_attribute *attr,const char *buf, size_t count)
{
       sscanf(buf, "%x", &irq_flag );
       dev_info(dev, "%s store value = %d \n", __func__,irq_flag);

	return count;
}
static DEVICE_ATTR(irq_flag, S_IRUSR | S_IWUSR, irq_flag_show, irq_flag_store);

static struct attribute *attributes[] = {
	&dev_attr_irq_flag.attr,
	NULL
};
static const struct attribute_group attribute_group = {
	.attrs = attributes,
};

/**
 * stk3338_pm_dev_init() - Device initialization.
 * @drvdata:   [IN]    driver data
 *
 * Return codes
 *   0 - Success
 *   -EIO - Fail of initialization
 */
static int stk3338_pm_dev_init(struct stk3338_pm_drvdata *drvdata)
{
       int ret;
       struct stk3338_pm_private_data *private_data = &drvdata->private_data;

       /* Power Ctrl init */
       private_data->cdev.owner = THIS_MODULE;
       private_data->drvdata = drvdata;

       pr_info("stk3338_pm_dev_init: cdev_init() \n");
       cdev_init(&private_data->cdev, &stk3338_pm_fops);
       if (cdev_add(&private_data->cdev, stk3338_pm_minor, 1) != 0) {
               pr_err("stk3338_pm: cdev_add failed");
               goto err_request_gpio_rst;
       }

       pr_err("stk3338_pm_dev_init: device_create()\n");
       private_data->dd = device_create(stk3338_pm_class, NULL,
                                                private_data->cdev.dev, private_data,
                                                STK3338_PM_DRIVER_NAME"%d", drvdata->pdev->id);
       if (IS_ERR(private_data->dd)) {
               pr_err("stk3338_pm: device_create failed: %i",
            (int)PTR_ERR(private_data->dd));
               goto err_device_create;
       }

       /* pinctrl operations */
       pr_err("stk3338_pm_dev_init: kzalloc()\n");
       drvdata->pdev->dev.pins = kzalloc(sizeof(*(drvdata->pdev->dev.pins)), GFP_KERNEL);

       if (!drvdata->pdev->dev.pins)
              return -ENOMEM;

       pr_err("stk3338_pm_dev_init: devm_pinctrl_get()\n");
       drvdata->pdev->dev.pins->p = devm_pinctrl_get(&drvdata->pdev->dev);

       if (IS_ERR(drvdata->pdev->dev.pins->p)) {
              dev_err(&drvdata->pdev->dev, "no pinctrl handle\n");
              ret = PTR_ERR(drvdata->pdev->dev.pins->p);
              goto cleanup_alloc;
       }

//	pr_err("stk3338_pm_dev_init: active=pinctrl_lookup_state(vprox18_active)\n");
//	private_data->vprox18_active  = pinctrl_lookup_state(drvdata->pdev->dev.pins->p, "vprox18_active");
//	if (IS_ERR(private_data->vprox18_active)) {
//		dev_err(&drvdata->pdev->dev, "vprox18_active pinctrl state\n");
//		ret = 0;
//		goto cleanup_get_init;
//	}

	pr_err("stk3338_pm_dev_init: active=pinctrl_lookup_state(vprox32_active)\n");
	private_data->vprox32_active   = pinctrl_lookup_state(drvdata->pdev->dev.pins->p, "vprox32_active");
	if (IS_ERR(private_data->vprox32_active)) {
		dev_err(&drvdata->pdev->dev, "vprox32_active pinctrl state\n");
		ret = 0;
		goto cleanup_get_init;
	}

//	pr_err("stk3338_pm_dev_init: sleep=pinctrl_lookup_state(vprox18_sleep)\n");
//	private_data->vprox18_sleep  = pinctrl_lookup_state(drvdata->pdev->dev.pins->p, "vprox18_sleep");
//	if (IS_ERR(private_data->vprox18_sleep)) {
//		dev_err(&drvdata->pdev->dev, "vprox18_sleep pinctrl state\n");
//		ret = 0;
//		goto cleanup_get_init;
//	}

	pr_err("stk3338_pm_dev_init: sleep=pinctrl_lookup_state(vprox32_sleep)\n");
	private_data->vprox32_sleep   = pinctrl_lookup_state(drvdata->pdev->dev.pins->p, "vprox32_sleep");
	if (IS_ERR(private_data->vprox32_sleep)) {
		dev_err(&drvdata->pdev->dev, "vprox32_sleep pinctrl state\n");
		ret = 0;
		goto cleanup_get_init;
	}

	return 0;

err_device_create:
       cdev_del(&private_data->cdev);

err_request_gpio_rst:

       return -EIO;

cleanup_get_init:
       devm_pinctrl_put(drvdata->pdev->dev.pins->p);

cleanup_alloc:
       devm_kfree(&drvdata->pdev->dev, drvdata->pdev->dev.pins);
       drvdata->pdev->dev.pins = NULL;

       /* Only return deferrals */
       if (ret != -EPROBE_DEFER)
               ret = 0;

       return ret;
}

/**
 * stk3338_pm_dev_power_on() - power on stk3338 pm device.
 * @drvdata:   [IN]    driver data
 *
 * Return codes
 *   0 - Success
 */

static int stk3338_pm_dev_power_on(struct stk3338_pm_drvdata *drvdata)
{
       int ret;
       struct stk3338_pm_private_data *private_data = &drvdata->private_data;

       // power on
//       pr_err("stk3338_pm_dev_power_on: pinctrl_select_state(vprox18_active)\n");
//       ret = pinctrl_select_state(drvdata->pdev->dev.pins->p, private_data->vprox18_active);
//	if (ret) {
//          dev_dbg(&drvdata->pdev->dev, "failed to activate pinctrl state: vprox18_active\n");
//          goto cleanup_get_pon;
//	}

//	dev_info(&drvdata->pdev->dev, "vprox18_active\n");
	msleep(STK3338_DEVICE_PON_WAITTIME1);

       // power on
       pr_err("stk3338_pm_dev_power_on: pinctrl_select_state(vprox32_active)\n");
       ret = pinctrl_select_state(drvdata->pdev->dev.pins->p, private_data->vprox32_active);
    	if (ret) {
           dev_dbg(&drvdata->pdev->dev, "failed to activate pinctrl state: vprox32_active\n");
           goto cleanup_get_pon;
    	}

	dev_info(&drvdata->pdev->dev, "vprox32_active\n");
	msleep(STK3338_DEVICE_PON_WAITTIME2);

       return 0;

cleanup_get_pon:
       devm_pinctrl_put(drvdata->pdev->dev.pins->p);

       /* Only return deferrals */
       if (ret != -EPROBE_DEFER)
               ret = 0;

       return ret;

}

static irqreturn_t irq_handler(int irq, void *handle)
{
	pr_info("%s \n", __func__);
       irq_flag = IRQ_DETECT;

	return IRQ_HANDLED;
}

/**
 * stk3338_irq_init() - enable irq handler.
 * @drvdata:   [IN]    driver data
 *
 * Return codes
 *   0 - Success
 */
static int stk3338_irq_init(struct stk3338_pm_drvdata *drvdata)
{
       int ret;
       struct stk3338_pm_private_data *private_data = &drvdata->private_data;

       ret = sysfs_create_group(&private_data->dd->kobj, &attribute_group);
	if (ret) {
		dev_err(private_data->dd, "could not create sysfs\n");
		goto exit;
	}

       gpio_of_irq = of_get_named_gpio(drvdata->pdev->dev.of_node, "stk3338_pm_irq", 0);

	if (gpio_of_irq < 0) {
		dev_err(private_data->dd, "failed to get '%d'\n",gpio_of_irq);
		goto exit;
	}

       ret = request_irq(gpio_to_irq(gpio_of_irq),irq_handler,IRQF_TRIGGER_FALLING | IRQF_ONESHOT,"stk3338_irq",private_data);
       if (ret) {
		dev_err(private_data->dd, "Fail to request_irq\n");
		goto exit;
	}

       irq_flag =IRQ_CLEAR;

       return ret;
exit:
       disable_irq_nosync(gpio_to_irq(gpio_of_irq));
       return -1;
}

/**
 * stk3338_pm_dev_power_off() - power off stk3338 pm device.
 * @drvdata:   [IN]    driver data
 *
 * Return codes
 *   0 - Success
 */
static int stk3338_pm_dev_power_off(struct stk3338_pm_drvdata *drvdata)
{
       int ret;
       struct stk3338_pm_private_data *private_data = &drvdata->private_data;

       // power off
       pr_err("stk3338_pm_dev_power_off: pinctrl_select_state(vprox32_sleep)\n");
       ret = pinctrl_select_state(drvdata->pdev->dev.pins->p, private_data->vprox32_sleep);
       if (ret) {
              dev_dbg(&drvdata->pdev->dev, "failed to activate pinctrl state: vprox32_sleep\n");
              goto cleanup_get_poff;
       }

	msleep(STK3338_DEVICE_PON_WAITTIME1);

	// power off
//       pr_err("stk3338_pm_dev_power_off: pinctrl_select_state(vprox18_sleep)\n");
//       ret = pinctrl_select_state(drvdata->pdev->dev.pins->p, private_data->vprox18_sleep);
//       if (ret) {
//              dev_dbg(&drvdata->pdev->dev, "failed to activate pinctrl state: vprox18_sleep\n");
//              goto cleanup_get_poff;
//       }

//	dev_info(&drvdata->pdev->dev, "vprox18_sleep\n");

       return 0;

cleanup_get_poff:
       devm_pinctrl_put(drvdata->pdev->dev.pins->p);

       /* Only return deferrals */
       if (ret != -EPROBE_DEFER)
               ret = 0;

       return ret;

}

/**
 * stk3338_pm_probe() - Prove stk3338 pm driver.
 * @pdev:      [IN]    platform device data
 *
 * Return codes
 *   0 - Success
 */
static int stk3338_pm_probe(struct platform_device *pdev)
{
       int ret = -ENODEV;

       struct stk3338_pm_drvdata *drvdata;

       dev_info(&pdev->dev, "stk3338_pm_probe in\n");

       drvdata = kzalloc(sizeof(struct stk3338_pm_drvdata), GFP_KERNEL);
       if (!drvdata) {
               dev_err(&pdev->dev, "No enough memory for stk3338_pm\n");
               ret = -ENOMEM;
               goto exit;
       }

       drvdata->pdev = pdev;
       platform_set_drvdata(pdev, drvdata);

       ret = stk3338_pm_dev_init(drvdata);
       if (ret) {
               dev_err(&pdev->dev, "Fail to initialize\n");
               goto exit;
       }

       ret = stk3338_pm_dev_power_on(drvdata);
		if (ret) {
               dev_err(&pdev->dev, "Fail to Power on\n");
               goto exit;
       }

	ret = stk3338_irq_init(drvdata);
	if (ret) {
		dev_err(&pdev->dev, "Fail to irq init\n");
		goto exit;
	}

       return 0;
exit:
       kfree(drvdata);
       platform_set_drvdata(pdev, NULL);
       dev_info(&pdev->dev, "%s: ng\n", __func__);

       return ret;
}

static ssize_t stk3338_pm_write(struct file *filp,const char __user *userbuf,size_t count, loff_t *f_pos)
{
       unsigned long value;
       char s[2];
       struct stk3338_pm_private_data *private_data;
       struct stk3338_pm_drvdata *drvdata;
       int len = min(sizeof(s) - 1, count);

       private_data = filp->private_data;
       drvdata = private_data->drvdata;

       if (copy_from_user(s, userbuf, len)) {
               return -EFAULT;
       }

       s[len] = '\0';

       if (kstrtoul((const char*)s, 0, &value)) {
              pr_err("Invalid value for power_ctrl\n");
              return -EFAULT;
       }

       if (value == STK3338_POWER_ON)
              stk3338_pm_dev_power_on(drvdata);
       else if (value == STK3338_POWER_OFF)
              stk3338_pm_dev_power_off(drvdata);

       return 0;
}

static int stk3338_pm_open(struct inode *inode, struct file *filp)
{
       struct stk3338_pm_private_data *private_data;

       private_data = container_of(inode->i_cdev, struct stk3338_pm_private_data, cdev);
       filp->private_data = private_data;

       return 0;
}

/**
 * stk3338_pm_remove() - Remove stk3338 pm driver.
 * @pdev:      [IN]    platform device data
 */
static int stk3338_pm_remove(struct platform_device *pdev)
{
       struct stk3338_pm_drvdata *drvdata = dev_get_drvdata(&pdev->dev);
       struct stk3338_pm_private_data *private_data = &drvdata->private_data;

       device_destroy(stk3338_pm_class, private_data->cdev.dev);
       cdev_del(&private_data->cdev);
       kfree(drvdata);

       return 0;
}

static struct of_device_id stk3338_pm_match_table[] = {
    {.compatible = STK3338_PM_DRIVER_NAME},
    {}
};

/**
 * brief Platform driver data structure of stk3338 driver
 */
static struct platform_driver stk3338_pm_driver = {
       .probe          = stk3338_pm_probe,
       .remove         = stk3338_pm_remove,
       .driver         = {
       .name           = STK3338_PM_DRIVER_NAME,
       .of_match_table = stk3338_pm_match_table,
       },
};

/**
 * stk3338_pm_driver_init() - The module init handler.
 *
 * Return codes
 *   0 - Success
 *   -errno - Failure
 */
static int __init stk3338_pm_driver_init(void)
{
       int ret;

       ret = alloc_chrdev_region(&stk3338_pm_minor, 0, 1, STK3338_PM_DRIVER_NAME);
       if (ret) {
        pr_err("stk3338_pm: alloc_chrdev_region failed: %d", ret);
               goto err_devrgn;
       }

       stk3338_pm_class = class_create(THIS_MODULE, STK3338_PM_DRIVER_NAME);
       if (IS_ERR(stk3338_pm_class)) {
               ret = PTR_ERR(stk3338_pm_class);
        pr_err("stk3338_pm: Error creating class: %d", ret);
        goto err_class;
       }

       ret = platform_driver_register(&stk3338_pm_driver);
       if (ret) {
        pr_err("stk3338_pm: platform_driver_register failed: %d", ret);
        goto err_register;
       }

       return 0;

err_register:
    class_destroy(stk3338_pm_class);
err_class:
    unregister_chrdev_region(0, 1);
err_devrgn:
    return ret;
}

/**
 * stk3338_pm_driver_exit() - The module exit handler.
 *
 * Return codes
 *   0 - Success
 *   -errno - Failure
 */
static void __exit stk3338_pm_driver_exit(void)
{
       platform_driver_unregister(&stk3338_pm_driver);
       class_destroy(stk3338_pm_class);
       unregister_chrdev_region(0, 1);
       disable_irq_nosync(gpio_to_irq(gpio_of_irq));
}

module_init(stk3338_pm_driver_init);
module_exit(stk3338_pm_driver_exit);

MODULE_DESCRIPTION("stk3338_pm driver");
MODULE_LICENSE("GPL v2");

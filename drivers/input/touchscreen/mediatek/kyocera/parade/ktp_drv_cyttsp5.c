/*
 * This software is contributed or developed by KYOCERA Corporation.
 * (C) 2013 KYOCERA Corporation
 * (C) 2014 KYOCERA Corporation
 * (C) 2015 KYOCERA Corporation
 * (C) 2016 KYOCERA Corporation
 * (C) 2022 KYOCERA Corporation
 *
 * drivers/input/touchscreen/mediatek/kyocera/parade/ktp_drv_cyttsp5.c
 *
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <asm/uaccess.h>
#include <linux/module.h>
#include <linux/input/mt.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/namei.h>
#include <linux/mutex.h>
#include <linux/cdev.h>

#include "ktp_drv_cyttsp5.h"

struct sysfs_data_{
	char command;
	u16 start_addr;
	u16 size;
} sdata;

/* sysfs ctrl command list */
#define KC_SYSFS_KDBGLEVEL	'x'
#define KC_SYSFS_POLLING	'e'
#define KC_SYSFS_CONFIG		'c'
#define KC_SYSFS_STATUS		's'
#define KC_SYSFS_IRQ		'i'
#define KC_SYSFS_NOTICE		'n'
#define KC_SYSFS_FACTORY_LOCK	'l'
/* pixart command list */

/* Global Variables */
unsigned int ts_event_control = 0;
EXPORT_SYMBOL_GPL(ts_event_control);
unsigned int ts_log_level = 0;
EXPORT_SYMBOL_GPL(ts_log_level);
unsigned int ts_log_file_enable = 0;
EXPORT_SYMBOL_GPL(ts_log_file_enable);
unsigned int ts_esd_recovery = 0;
EXPORT_SYMBOL_GPL(ts_esd_recovery);
unsigned int ts_config_switching = 1;
EXPORT_SYMBOL_GPL(ts_config_switching);
unsigned int ts_irq_enable = 0;
EXPORT_SYMBOL_GPL(ts_irq_enable);
unsigned int ts_error_status = 0;
EXPORT_SYMBOL_GPL(ts_error_status);
unsigned int ts_factory_lock = 0;
EXPORT_SYMBOL_GPL(ts_factory_lock);
int ts_panel_id = 0;

static int kc_ts_open(struct inode *inode, struct file *file)
{
	struct kc_ts_data *ts =
		container_of(inode->i_cdev, struct kc_ts_data, device_cdev);
	KC_TS_DEV_DBG("%s() is called.\n", __func__);

	file->private_data = ts;
	return 0;
};

static int kc_ts_release(struct inode *inode, struct file *file)
{
	KC_TS_DEV_DBG("%s() is called.\n", __func__);
	file->private_data = NULL;
	return 0;
};

static long kc_ts_diag_data_start(struct kc_ts_data *ts)
{
	KC_TS_DEV_DBG("%s is called.\n", __func__);
	mutex_lock(&ts->lock);
	if (ts->diag_data == NULL) {
		ts->diag_data = kzalloc(sizeof(struct ts_diag_type), GFP_KERNEL);
		if (!ts->diag_data) {
			mutex_unlock(&ts->lock);
			dev_err(ts->dev, "Failed to allocate memory!\n");
			return -ENOMEM;
		}
	}
	mutex_unlock(&ts->lock);

	return 0;
}

static long kc_ts_diag_data_end(struct kc_ts_data *ts)
{
	KC_TS_DEV_DBG("%s is called.\n", __func__);

	mutex_lock(&ts->lock);
	if (ts->diag_data != NULL) {
		kfree(ts->diag_data);
		ts->diag_data = NULL;
		KC_TS_DEV_DBG("%s ts->diag_data = NULL\n", __func__);
	}
	mutex_unlock(&ts->lock);
	return 0;
}

#ifdef CONFIG_TOUCH_ESD_RECOVERY
static void kc_ts_esd_work(struct work_struct *work)
{
	struct kc_ts_data *ts = container_of(work, struct kc_ts_data, esdwork.work);
	int err = 0;

	KC_TS_DEV_DBG("%s is called.\n", __func__);

	if (ts_esd_recovery)
		err = ts->tops->esd_proc(ts);
	else
		KC_TS_DEV_DBG("%s: Skip. Disabled ESD recovery\n", __func__);

	if (ts->wq && !err)
		queue_delayed_work(ts->wq, &ts->esdwork,
					msecs_to_jiffies(ESD_POLLING_TIME));
	else
		pr_err("%s: Failed to ESD work! err = %d\n", __func__, err);

	KC_TS_DEV_DBG("%s is completed.\n", __func__);
}
#endif /* CONFIG_TOUCH_ESD_RECOVERY */

static long kc_ts_ioctl(struct file *file, unsigned int cmd,
						unsigned long arg)
{
	struct kc_ts_data *ts = (struct kc_ts_data *)file->private_data;
	struct device *dev = ts->dev;
	long err = 0;

	KC_TS_DEV_DBG("%s() is called.\n", __func__);

	if (!ts) {
		dev_err(dev, "%s: kc_ts data is not set.\n", __func__);
		return -EINVAL;
	}

	switch (cmd) {
	case IOCTL_SET_CONF_STAT:
		KC_TS_DEV_DBG("%s: IOCTL_SET_CONF_STAT\n", __func__);
		if (!access_ok(VERIFY_READ, (void __user *)arg,
						_IOC_SIZE(cmd))) {
			err = -EFAULT;
			dev_err(dev, "%s: invalid access\n", __func__);
			goto done;
		}
		if (copy_from_user(&ts->config_status,
			(void __user *)arg, sizeof(ts->config_status))) {
			err = -EFAULT;
			pr_err("%s: copy_from_user error\n", __func__);
			goto done;
		}

		mutex_lock(&ts->lock);
		err = ts->tops->set_conf(ts);
		mutex_unlock(&ts->lock);
		break;
	case IOCTL_DIAG_START:
		KC_TS_DEV_DBG("%s: IOCTL_DIAG_START\n", __func__);
		err = kc_ts_diag_data_start(ts);
		break;

	case IOCTL_MULTI_GET:
	case IOCTL_COODINATE_GET:
		KC_TS_DEV_DBG("%s: IOCTL_MULTI_GET\n", __func__);
		KC_TS_DEV_DBG("%s: IOCTL_COODINATE_GET\n", __func__);
		if (!access_ok(VERIFY_WRITE, (void __user *)arg,
						_IOC_SIZE(cmd))) {
			err = -EFAULT;
			dev_err(dev, "%s: invalid access\n", __func__);
			goto done;
		}
		mutex_lock(&ts->lock);
 		if (ts->diag_data) {
			if (ts->tops->get_touch_info) {
				ts->tops->get_touch_info(ts);
			}
			err = copy_to_user((void __user *)arg, ts->diag_data,
						sizeof(struct ts_diag_type));
			if (err)
				dev_err(dev, "%s: copy_to_user error\n", __func__);
		} else {
			dev_err(dev, "Touchscreen Diag not active!\n");
		}
		mutex_unlock(&ts->lock);
		break;

	case IOCTL_DIAG_END:
		KC_TS_DEV_DBG("%s: IOCTL_DIAG_END\n", __func__);
		err = kc_ts_diag_data_end(ts);
		break;
	default:
		dev_err(dev, "%s: cmd error[%X]\n", __func__, cmd);
		return -EINVAL;
		break;
	}
done:
	return err;
}

int ts_ctrl_init(struct kc_ts_data *ts, const struct file_operations *fops)
{
	struct cdev *device_cdev = &ts->device_cdev;

	dev_t device_t = MKDEV(0, 0);
	struct device *class_dev_t = NULL;
	int ret;

	ret = alloc_chrdev_region(&device_t, 0, 1, "ts_ctrl");
	if (ret)
		goto error;

	ts->device_major = MAJOR(device_t);

	cdev_init(device_cdev, fops);
	device_cdev->owner = THIS_MODULE;
	device_cdev->ops = fops;
	ret = cdev_add(device_cdev, MKDEV(ts->device_major, 0), 1);
	if (ret)
		goto err_unregister_chrdev;

	ts->device_class = class_create(THIS_MODULE, "ts_ctrl");
	if (IS_ERR(ts->device_class)) {
		ret = -1;
		goto err_cleanup_cdev;
	};

	class_dev_t = device_create(ts->device_class, NULL,
		MKDEV(ts->device_major, 0), NULL, "ts_ctrl");
	if (IS_ERR(class_dev_t)) {
		ret = -1;
		goto err_destroy_class;
	}

	return 0;

err_destroy_class:
	class_destroy(ts->device_class);
err_cleanup_cdev:
	cdev_del(device_cdev);
err_unregister_chrdev:
	unregister_chrdev_region(device_t, 1);
error:
	return ret;
}
EXPORT_SYMBOL_GPL(ts_ctrl_init);

/***********************************************************/

#define KTP_DEV_NAME              "cyttsp5_core"
#define KTP_DRIVER_NAME           "kc_touch"
#define KTP_CLASS_NAME            "ktp"

#define KTP_MAX_PRBUF_SIZE       PAGE_SIZE		// 1024*4

static ssize_t ktp_drv_name_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	ssize_t size = 0;

	size = snprintf(buf, KTP_MAX_PRBUF_SIZE,
		"%s\n",
		KTP_DEV_NAME);

	pr_err("[KCTP]%s: Device Name = [%s]\n", __func__, KTP_DEV_NAME);

	return size;
}

static ssize_t ktp_drv_event_ctrl_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	size_t ret = 0;

	pr_err("%s: START\n", __func__);
	pr_err("%s: END\n", __func__);

	return sprintf(buf, "%d\n", ret);
}

static ssize_t ktp_drv_event_ctrl_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	pr_err("%s: START\n", __func__);
	pr_err("%s: END\n", __func__);

	return size;
}

static ssize_t ktp_drv_glove_mode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	size_t ret = 0;
	pr_err("%s: START\n", __func__);
	pr_err("%s: END\n", __func__);

	return sprintf(buf, "%d\n", ret);
}

static ssize_t ktp_drv_glove_mode_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	pr_err("%s: START\n", __func__);
	pr_err("%s: END\n", __func__);

	return size;
}

static ssize_t ktp_drv_touch_mode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	size_t ret = 0;
	pr_err("%s: START\n", __func__);
	pr_err("%s: END\n", __func__);

	return sprintf(buf, "%d\n", ret);
}

static ssize_t ktp_drv_touch_mode_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	pr_err("%s: START\n", __func__);
	pr_err("%s: END\n", __func__);

	return size;
}

static ssize_t ktp_drv_easywake_mode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	size_t ret = 0;
	pr_err("%s: START\n", __func__);
	pr_err("%s: END\n", __func__);

	return sprintf(buf, "%d\n", ret);
}

static ssize_t ktp_drv_easywake_mode_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	pr_err("%s: START\n", __func__);
	pr_err("%s: END\n", __func__);

	return size;
}

static DEVICE_ATTR(ktp_drv_name, 0644, ktp_drv_name_show, NULL);
static DEVICE_ATTR(ktp_drv_event_ctrl, 0644, ktp_drv_event_ctrl_show, ktp_drv_event_ctrl_store);
static DEVICE_ATTR(ktp_drv_glove_mode, 0644, ktp_drv_glove_mode_show, ktp_drv_glove_mode_store);
static DEVICE_ATTR(ktp_drv_touch_mode, 0644, ktp_drv_touch_mode_show, ktp_drv_touch_mode_store);
static DEVICE_ATTR(ktp_drv_easywake_mode, 0644, ktp_drv_easywake_mode_show, ktp_drv_easywake_mode_store);

static struct attribute *ktp_attrs[] = {
		&dev_attr_ktp_drv_name.attr,
		&dev_attr_ktp_drv_event_ctrl.attr,
		&dev_attr_ktp_drv_glove_mode.attr,
		&dev_attr_ktp_drv_touch_mode.attr,
		&dev_attr_ktp_drv_easywake_mode.attr,
		NULL
};

static const struct attribute_group ktp_attr_group = {
		.attrs = ktp_attrs,
};

static int ktp_drv_create_sysfs(struct kc_ts_data *ktp)
{
	int ret = 0;

	ret = alloc_chrdev_region(&ktp->device_major, 0, 1, KTP_DRIVER_NAME);
	if (ret  < 0) {
		pr_err("%s: alloc_chrdev_region failed. ret=%d\n", __func__, ret);
		goto exit;
	}

	ktp->device_class = class_create(THIS_MODULE, KTP_CLASS_NAME);
	if (IS_ERR(ktp->device_class)) {
		pr_err("%s: Fail to create class.\n", __func__);
		goto exit;
	}

	ktp->dev = device_create(ktp->device_class, NULL,
		ktp->device_major, NULL, KTP_DRIVER_NAME);
	if (IS_ERR(ktp->dev)) {
		ret = PTR_ERR(ktp->dev);
		pr_err("%s: Fail to create device. ret=%d\n", __func__, ret);
		goto exit;
	}

	dev_set_drvdata(ktp->dev, ktp);

	ret = sysfs_create_group(&ktp->dev->kobj, &ktp_attr_group);
	if (ret)
		pr_err("%s: Fail to create sysfs.\n", __func__);

exit:
	return ret;
}

/***********************************************************/

int ts_ctrl_exit(struct kc_ts_data *ts)
{
	struct cdev *device_cdev = &ts->device_cdev;
	struct class *device_class = ts->device_class;
	dev_t device_t = MKDEV(ts->device_major, 0);

	if (device_class) {
		device_destroy(device_class, MKDEV(ts->device_major, 0));
		class_destroy(device_class);
	}
	cdev_del(device_cdev);
	unregister_chrdev_region(device_t, 1);
	return 0;
}
EXPORT_SYMBOL_GPL(ts_ctrl_exit);

const struct file_operations kc_ts_fops = {
	.owner = THIS_MODULE,
	.open = kc_ts_open,
	.unlocked_ioctl = kc_ts_ioctl,
	.release = kc_ts_release,
};

static ssize_t kc_ctrl_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct input_dev *input_dev = container_of(dev, struct input_dev, dev);
	struct kc_ts_data *ts = input_get_drvdata(input_dev);
	bool enable = false;

	switch (buf[0]) {
	case KC_SYSFS_KDBGLEVEL:
		KC_TS_DEV_DBG("%s: KC_SYSFS_KDBGLEVEL\n", __func__);
		sscanf(buf, "%c %x", &sdata.command,
					(unsigned int *) &ts_log_level);
		KC_TS_DEV_DBG("%s: ts_log_level = %x\n", __func__, ts_log_level);
		break;
	case KC_SYSFS_POLLING:
		KC_TS_DEV_DBG("%s: KC_SYSFS_POLLING\n", __func__);
		sscanf(buf, "%c %x", &sdata.command,
					(unsigned int *) &ts_esd_recovery);
		KC_TS_DEV_DBG("ts_esd_recovery is set to %d\n",
							ts_esd_recovery);
		break;
	case KC_SYSFS_CONFIG:
		KC_TS_DEV_DBG("%s: KC_SYSFS_CONFIG\n", __func__);
		sscanf(buf, "%c %x", &sdata.command,
					(unsigned int *) &ts_config_switching);
		KC_TS_DEV_DBG("%s: ts_config_switching is set to %d\n", __func__,
							ts_config_switching);
		break;
	case KC_SYSFS_IRQ:
		KC_TS_DEV_DBG("%s: KC_SYSFS_IRQ\n", __func__);
		sscanf(buf, "%c %x", &sdata.command,
					(unsigned int *) &ts_irq_enable);
		if(ts_irq_enable != 0)
			enable = true;
		ts->tops->irq_proc(ts, enable);
		break;
	case KC_SYSFS_STATUS:
		KC_TS_DEV_DBG("%s: KC_SYSFS_STATUS\n", __func__);
		sscanf(buf, "%c", &sdata.command);
		break;
	case KC_SYSFS_NOTICE:
		KC_TS_DEV_DBG("%s: KC_SYSFS_NOTICE\n", __func__);

		mutex_lock(&ts->lock);
		sscanf(buf, "%c %x", &sdata.command,
					(unsigned int *) &ts_event_control);
		if (ts_event_control && (ts->tops->free_fingers))
			ts->tops->free_fingers(ts);
		mutex_unlock(&ts->lock);

		KC_TS_DEV_DBG("%s: ts_event_control is set to %x\n",
						__func__, ts_event_control);
		break;
	case KC_SYSFS_FACTORY_LOCK:
		KC_TS_DEV_DBG("%s: KC_SYSFS_FACTORY_LOCK\n", __func__);
		sscanf(buf, "%c %x", &sdata.command,
					(unsigned int *) &ts_factory_lock);
		if(ts_factory_lock != 0)
			enable = true;
		ts->tops->factory_lock(ts, enable);
		break;
	default:
		break;
	}
	return count;
}

static ssize_t kc_ctrl_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	int count = 0;
	struct input_dev *input_dev = container_of(dev, struct input_dev, dev);
	struct kc_ts_data *ts = input_get_drvdata(input_dev);

	switch (sdata.command) {
	case KC_SYSFS_KDBGLEVEL:
		KC_TS_DEV_DBG("%s: KC_SYSFS_KDBGLEVEL\n", __func__);
		count += scnprintf(buf, PAGE_SIZE - count,
				   "ts_log_level is [%d]\n",
				   ts_log_level);
		break;
	case KC_SYSFS_POLLING:
		KC_TS_DEV_DBG("%s: KC_SYSFS_POLLING\n", __func__);
		count += scnprintf(buf, PAGE_SIZE - count, "ts_esd_recovery is "
							"[%d]\n", ts_esd_recovery);
		break;
	case KC_SYSFS_CONFIG:
		KC_TS_DEV_DBG("%s: KC_SYSFS_CONFIG\n", __func__);
		count += scnprintf(buf, PAGE_SIZE - count,
				   "config switching is [%d]\n",
				   ts_config_switching);
		break;
	case KC_SYSFS_STATUS:
		KC_TS_DEV_DBG("%s: KC_SYSFS_STATUS\n", __func__);
		if (ts->tops->get_status)
			count = ts->tops->get_status(ts, buf);
		break;
	case KC_SYSFS_IRQ:
		KC_TS_DEV_DBG("%s: KC_SYSFS_IRQ\n", __func__);
		count += scnprintf(buf, PAGE_SIZE - count,
				   "irq enable is [%d]\n",
				   ts_irq_enable);
		break;
	case KC_SYSFS_NOTICE:
		KC_TS_DEV_DBG("%s: KC_SYSFS_NOTICE\n", __func__);
		count += scnprintf(buf, PAGE_SIZE - count,
				   "event control is [%d]\n",
				   ts_event_control);
		break;
	case KC_SYSFS_FACTORY_LOCK:
		KC_TS_DEV_DBG("%s: KC_SYSFS_FACTORY_LOCK\n", __func__);
		count += scnprintf(buf, PAGE_SIZE - count,
				   "factory lock  is [%d]\n",
				   ts_factory_lock);
		break;
	default:
		break;
	}
	return count;
}

static DEVICE_ATTR(ctrl, S_IRUGO|S_IWUSR|S_IWGRP, kc_ctrl_show, kc_ctrl_store);

static struct attribute *kc_attrs[] = {
	&dev_attr_ctrl.attr,
	NULL
};

static const struct attribute_group kc_attr_group = {
	.attrs = kc_attrs,
};

int kc_ts_probe(struct kc_ts_data *ts)
{
	int ret = 0;
	
	pr_err("%s: is enter.\n", __func__);

	KC_TS_DEV_DBG("%s is called.\n", __func__);
	if (!ts) {
		pr_err("%s: kc_ts data is not set.\n", __func__);
		return -EINVAL;
	}

	if (!ts->dev) {
		pr_err("%s: dev is not set.\n", __func__);
		return -EINVAL;
	}

	if (!ts->tops) {
		pr_err("%s: tops is not set.\n", __func__);
		return -EINVAL;
	}

	mutex_init(&ts->lock);

	/* Create cdev file ts_ctrl */
	ret = ts_ctrl_init(ts, &kc_ts_fops);
	if (ret) {
		pr_err("%s: Fail to create cdev.\n", __func__);
		goto err_cdev;
	}

	/* Create sysfs */
	ret = sysfs_create_group(&ts->dev->kobj, &kc_attr_group);
	if (ret) {
		pr_err("%s: Fail to create sysfs.\n", __func__);
		goto err_sysfs;
	}

	/* Create sysfs */
	ret = ktp_drv_create_sysfs(ts);
	if (ret) {
		pr_err("%s: Fail to create sysfs.\n", __func__);
		goto err_sysfs;
	}

	ts->wq = alloc_workqueue("kc_ts_wq", WQ_MEM_RECLAIM, 1);
	if (!ts->wq) {
		pr_err("%s: Fail to allocate workqueue!\n", __func__);
		ret = -ENOMEM;
		goto err_sysfs;
	}
#ifdef CONFIG_TOUCH_ESD_RECOVERY
	INIT_DELAYED_WORK(&ts->esdwork, kc_ts_esd_work);
	queue_delayed_work(ts->wq, &ts->esdwork,
				   msecs_to_jiffies(10000));
#endif /* CONFIG_TOUCH_ESD_RECOVERY */

	KC_TS_DEV_DBG("%s is completed.\n", __func__);
	pr_err("%s: is exit.\n", __func__);


	return ret;

err_sysfs:
	ts_ctrl_exit(ts);
err_cdev:
	mutex_destroy(&ts->lock);

	return ret;
}
EXPORT_SYMBOL(kc_ts_probe);

void kc_ts_remove(struct kc_ts_data *ts)
{
	KC_TS_DEV_DBG("%s is called.\n", __func__);

	if (!ts || !ts->dev) {
		pr_err("%s: kc_ts data is not set.\n", __func__);
		return;
	}

	if(ts->wq){
		cancel_delayed_work_sync(&ts->esdwork);
		flush_workqueue(ts->wq);
	}
	destroy_workqueue(ts->wq);

	mutex_destroy(&ts->lock);
	sysfs_remove_group(&ts->dev->kobj, &kc_attr_group);
	ts_ctrl_exit(ts);

	KC_TS_DEV_DBG("%s is completed.\n", __func__);

	return;

}
EXPORT_SYMBOL(kc_ts_remove);

MODULE_AUTHOR("KYOCERA Corporation");
MODULE_DESCRIPTION("kc touchscreen driver");
MODULE_LICENSE("GPL");


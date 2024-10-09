/*
 * This software is contributed or developed by KYOCERA Corporation.
 * (C) 2022 KYOCERA Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>
#include <linux/pm_wakeup.h>
#include <linux/sysfs.h>
#include <linux/oem_chg_dnand.h>

static spinlock_t oem_chg_dnand_lock;
struct mutex oem_chg_dnand_mutex;
static int oem_chg_dnand_property_values[MAX_OEM_CHG_DNAND_PROP];
static int oem_chg_dnand_property_values_changed = 0;
static struct platform_device *chg_dnand_pdev = NULL;
static struct wakeup_source* chg_dnand_set_wake_src;
static struct wakeup_source* chg_dnand_sync_wake_src;
static struct work_struct set_property_work;
static struct completion chg_dnand_sync_completion;
static int chg_dnand_sync_sync_count = 0;

#define OEM_CHG_DNAND_ATTR(_name) \
{ \
	.attr = { \
		.name = #_name, \
		.mode = 0644, \
	}, \
	.show = oem_chg_dnand_show_property, \
	.store = oem_chg_dnand_store_property, \
}
static struct device_attribute oem_chg_dnand_property_attrs[];

/****************************************************************************
 * INTERNAL API
****************************************************************************/
static void oem_chg_dnand_send_uvent_change(void)
{
	kobject_uevent(&chg_dnand_pdev->dev.kobj, KOBJ_CHANGE);
}

static void oem_chg_dnand_send_uevent_sync(void)
{
	char event_string[64];
	char *envp[] = { event_string, NULL };

	snprintf(event_string, sizeof(event_string), "OEM_CHG_DNAND_UEVENT_SYNC=%d", chg_dnand_sync_sync_count);
	kobject_uevent_env(&chg_dnand_pdev->dev.kobj, KOBJ_CHANGE, envp);
}

static void oem_chg_dnand_set_property_work(struct work_struct *work)
{
	unsigned long flags;

	spin_lock_irqsave(&oem_chg_dnand_lock, flags);
	if(oem_chg_dnand_property_values_changed) {
		oem_chg_dnand_property_values_changed = 0;
		spin_unlock_irqrestore(&oem_chg_dnand_lock, flags);

		oem_chg_dnand_send_uvent_change();
		spin_lock_irqsave(&oem_chg_dnand_lock, flags);
	}

	if(!oem_chg_dnand_property_values_changed) {
		__pm_relax(chg_dnand_set_wake_src);
	}
	spin_unlock_irqrestore(&oem_chg_dnand_lock, flags);
}

/****************************************************************************
 * EXTERNAL API
****************************************************************************/

/**
 * oem_chg_dnand_set_property: - write the property
 * @ocdp: id of property
 * @val: value of write to the property
 *
 * Return: 0 if set is succeeded
 * otherwise it returns negative error value.
 */
int oem_chg_dnand_set_property(enum oem_chg_dnand_property ocdp, int *val)
{
	unsigned long flags;

	if(chg_dnand_pdev == NULL) return -EPROBE_DEFER;

	spin_lock_irqsave(&oem_chg_dnand_lock, flags);
	oem_chg_dnand_property_values_changed = 1;
	__pm_stay_awake(chg_dnand_set_wake_src);
	oem_chg_dnand_property_values[ocdp] = *val;
	spin_unlock_irqrestore(&oem_chg_dnand_lock, flags);
	schedule_work(&set_property_work);
	return 0;
}
EXPORT_SYMBOL(oem_chg_dnand_set_property);

/**
 * oem_chg_dnand_get_property: - read the property
 * @ocdp: id of property
 * @val: value of read from the property
 *
 * Return: 0 if get is succeeded
 * otherwise it returns negative error value.
 */
int oem_chg_dnand_get_property(enum oem_chg_dnand_property ocdp, int *val)
{
	unsigned long flags;

	if(chg_dnand_pdev == NULL) {
		*val = INT_MIN;
		return -EPROBE_DEFER;
	}

	spin_lock_irqsave(&oem_chg_dnand_lock, flags);
	*val = oem_chg_dnand_property_values[ocdp];
	spin_unlock_irqrestore(&oem_chg_dnand_lock, flags);

	return 0;
}
EXPORT_SYMBOL(oem_chg_dnand_get_property);

/**
 * oem_chg_dnand_sync_property: - sync all property to the dnand
 *
 * This function sent uevent to chg_dnand process, and wait it done writing dnand.
 * So, This function cannot be used within a function that stops the process, such as probe , irq and atomic context etc.
 * If it cannot synchronize with chg_dnand process in 20 seconds, Returns an error and gives up synchronization.
 *
 * Return: 0 if sync is succeeded
 * otherwise it returns negative error value.
 */
int oem_chg_dnand_sync_property()
{
	int ret = 0;

	if(chg_dnand_pdev == NULL) return -EPROBE_DEFER;

	pr_notice("%s : start\n", __func__);

	mutex_lock(&oem_chg_dnand_mutex);
	__pm_stay_awake(chg_dnand_sync_wake_src);
	reinit_completion(&chg_dnand_sync_completion);
	chg_dnand_sync_sync_count++;
	oem_chg_dnand_send_uevent_sync();
	if(wait_for_completion_io_timeout(&chg_dnand_sync_completion , msecs_to_jiffies(20 * 1000)) == 0) {
		ret = -ETIMEDOUT;
	}
	__pm_relax(chg_dnand_sync_wake_src);
	mutex_unlock(&oem_chg_dnand_mutex);

	pr_notice("%s : end, ret = %d\n", __func__, ret);
	return ret;
}
EXPORT_SYMBOL(oem_chg_dnand_sync_property);

/****************************************************************************
 * USERLAND API
****************************************************************************/
static ssize_t oem_chg_dnand_store_property(struct device *dev, struct device_attribute *attr, const char *buffer, size_t size)
{
	ssize_t ret;
	const ptrdiff_t off = attr - oem_chg_dnand_property_attrs;
	int value;

	ret = kstrtoint(buffer, 0, &value);
	if (ret < 0)
		return ret;

	ret = oem_chg_dnand_set_property(off, &value);
	if (ret < 0)
		return ret;

	return size;
}

static ssize_t oem_chg_dnand_show_property(struct device *dev, struct device_attribute *attr, char *buffer) {
	ssize_t ret;
	const ptrdiff_t off = attr - oem_chg_dnand_property_attrs;
	int value;

	ret = oem_chg_dnand_get_property(off, &value); 
	if (ret < 0)
		return ret;

	return sprintf(buffer, "%d\n", value);
}

static struct device_attribute oem_chg_dnand_property_attrs[] = {
	OEM_CHG_DNAND_ATTR(oem_cycle_count),
	OEM_CHG_DNAND_ATTR(oem_cycle_increase),
	OEM_CHG_DNAND_ATTR(oem_online_time),
	OEM_CHG_DNAND_ATTR(oem_battery_care_mode),
	OEM_CHG_DNAND_ATTR(oem_battery_care_notification),
	OEM_CHG_DNAND_ATTR(oem_auto_on_enable),
};

static ssize_t oem_chg_dnand_sync(struct device *dev, struct device_attribute *attr, const char *buffer, size_t size)
{
	oem_chg_dnand_sync_property();

	return size;
}

static ssize_t oem_chg_dnand_sync_finish(struct device *dev, struct device_attribute *attr,	const char *buffer, size_t size)
{
	int ret;
	int val;

	ret = kstrtoint(buffer, 0, &val);
	if(ret != 0) return ret;

	if(val == chg_dnand_sync_sync_count) {
		complete(&chg_dnand_sync_completion);
	}

	return size;
}

static DEVICE_ATTR(sync, 0644, NULL, oem_chg_dnand_sync);
static DEVICE_ATTR(sync_finish, 0644, NULL, oem_chg_dnand_sync_finish);

static struct attribute *oem_chg_dnand_control_attrs[] = {
	&dev_attr_sync.attr,
	&dev_attr_sync_finish.attr,
	NULL
};

static const struct attribute_group oem_chg_dnand_control_attr_group = {
	.attrs = oem_chg_dnand_control_attrs,
};

/****************************************************************************
 * INITIAL PROCESS
****************************************************************************/
static int oem_chg_dnand_parse_dt(struct oem_chg_dnand_chip *chip)
{
	struct device_node *np;
	int *oem_chg_dnand_values = NULL;
	int i;

	np = of_find_node_by_name(NULL, "chosen");
	if (np) {
		oem_chg_dnand_values = (int*)of_get_property(np, "atag,oem_chg_dnand_values", NULL);
		if(!oem_chg_dnand_values) {
			pr_err("%s : can not find atag,oem_chg_dnand_values property\n", __func__);
		}
	}else{
		pr_err("%s : can not find chosen node\n", __func__);
	}

	if(oem_chg_dnand_values) {
		for(i = 0; i < MAX_OEM_CHG_DNAND_PROP; i++) {
			oem_chg_dnand_property_values[i] = oem_chg_dnand_values[i];
		}
	}else{
		for(i = 0; i < MAX_OEM_CHG_DNAND_PROP; i++) {
			oem_chg_dnand_property_values[i] = INT_MIN;
		}
	}

	return 0;
}

static int oem_chg_dnand_probe(struct platform_device *pdev)
{
	int rc = 0;
	struct oem_chg_dnand_chip *chip;
	int dcf_idx = 0;

	dev_notice(&pdev->dev, "%s : start\n", __func__);

	chip = devm_kzalloc(&pdev->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip) {
		dev_err(&pdev->dev, "Unable to allocate memory\n");
		return -ENOMEM;
	}

	chip->dev = &pdev->dev;
	platform_set_drvdata(pdev, chip);

	rc = oem_chg_dnand_parse_dt(chip);
	if (rc < 0) {
		dev_err(&pdev->dev, "Unable to parse DT nodes\n");
		goto err_parse_dt;
	}

	spin_lock_init(&oem_chg_dnand_lock);
	mutex_init(&oem_chg_dnand_mutex);
	chg_dnand_set_wake_src =  wakeup_source_register("chg_dnand_set_wake_src");
	chg_dnand_sync_wake_src =  wakeup_source_register("chg_dnand_sync_wake_src");
	INIT_WORK(&set_property_work, oem_chg_dnand_set_property_work);
	init_completion(&chg_dnand_sync_completion);

	rc = sysfs_create_group(&pdev->dev.kobj, &oem_chg_dnand_control_attr_group);
	if (rc < 0) {
		dev_err(&pdev->dev, "Unable to sysfs_create_group oem_chg_dnand_control_attr_group\n");
		goto err_create_control_sysfs;
	}

	for(dcf_idx = 0; dcf_idx < ARRAY_SIZE(oem_chg_dnand_property_attrs); dcf_idx++) {
		rc = device_create_file(&pdev->dev, &oem_chg_dnand_property_attrs[dcf_idx]);
		if (rc < 0) {
			dev_err(&pdev->dev, "Unable to device_create_file, dcif = %d\n", dcf_idx);
			for (dcf_idx--; dcf_idx >= 0; dcf_idx--) {
				device_remove_file(&pdev->dev, &oem_chg_dnand_property_attrs[dcf_idx]);
			}
			goto err_create_property_sysfs;
		}
	}

	chg_dnand_pdev = pdev;
	dev_notice(&pdev->dev, "%s : end\n", __func__);
	return 0;

err_create_property_sysfs:
	sysfs_remove_group(&pdev->dev.kobj, &oem_chg_dnand_control_attr_group);
err_create_control_sysfs:
	wakeup_source_unregister(chg_dnand_set_wake_src);
	wakeup_source_unregister(chg_dnand_sync_wake_src);
	mutex_destroy(&oem_chg_dnand_mutex);
err_parse_dt:
	platform_set_drvdata(pdev, NULL);
	return rc;
}

static int oem_chg_dnand_remove(struct platform_device *pdev)
{
	return 0;
}

static const struct of_device_id oem_chg_dnand_of_match[] = {
	{ .compatible = "oem_chg_dnand_driver", },
	{},
};

static struct platform_driver oem_chg_dnand_driver = {
	.probe = oem_chg_dnand_probe,
	.remove = oem_chg_dnand_remove,
	.driver = {
		.name = "oem_chg_dnand_driver",
		.of_match_table = oem_chg_dnand_of_match,
	},
};

static int __init oem_chg_dnand_init(void)
{
	return platform_driver_register(&oem_chg_dnand_driver);
}
module_init(oem_chg_dnand_init);

static void __exit oem_chg_dnand_exit(void)
{
	return platform_driver_unregister(&oem_chg_dnand_driver);
}
module_exit(oem_chg_dnand_exit);

MODULE_DESCRIPTION("oem_chg_dnand_driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("oem_chgdnand");

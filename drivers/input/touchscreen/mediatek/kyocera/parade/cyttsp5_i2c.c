/*
 * This software is contributed or developed by KYOCERA Corporation.
 * (C) 2016 KYOCERA Corporation
 * (C) 2017 KYOCERA Corporation
 * (C) 2019 KYOCERA Corporation
 */
/*
 * cyttsp5_i2c.c
 * Cypress TrueTouch(TM) Standard Product V5 I2C Module.
 * For use with Cypress touchscreen controllers.
 * Supported parts include:
 * CYTMA5XX
 * CYTMA448
 * CYTMA445A
 * CYTT21XXX
 * CYTT31XXX
 *
 * Copyright (C) 2012-2015 Cypress Semiconductor
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2, and only version 2, as published by the
 * Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * Contact Cypress Semiconductor at www.cypress.com <ttdrivers@cypress.com>
 *
 */

#include "cyttsp5_regs.h"

#include <linux/i2c.h>
#include <linux/version.h>

#define CY_I2C_DATA_SIZE  (2 * 256)

static int cyttsp5_i2c_read_default(struct device *dev, void *buf, int size)
{
	struct i2c_client *client = to_i2c_client(dev);
	int rc;

	if (!buf || !size || size > CY_I2C_DATA_SIZE)
		return -EINVAL;

	rc = i2c_master_recv(client, buf, size);

	return (rc < 0) ? rc : rc != size ? -EIO : 0;
}

static int cyttsp5_i2c_read_default_nosize(struct device *dev, u8 *buf, u32 max)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct i2c_msg msgs[2];
	u8 msg_count = 1;
	int rc;
	u32 size;

	if (!buf)
		return -EINVAL;

	msgs[0].addr = client->addr;
	msgs[0].flags = (client->flags & I2C_M_TEN) | I2C_M_RD;
	msgs[0].len = 2;
	msgs[0].buf = buf;
	rc = i2c_transfer(client->adapter, msgs, msg_count);
	if (rc < 0 || rc != msg_count)
		return (rc < 0) ? rc : -EIO;

	size = get_unaligned_le16(&buf[0]);
	if (!size || size == 2)
		return 0;

	if (size > max)
		return -EINVAL;

	rc = i2c_master_recv(client, buf, size);

	return (rc < 0) ? rc : rc != (int)size ? -EIO : 0;
}

static int cyttsp5_i2c_write_read_specific(struct device *dev, u8 write_len,
		u8 *write_buf, u8 *read_buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct i2c_msg msgs[2];
	u8 msg_count = 1;
	int rc;

	if (!write_buf || !write_len)
		return -EINVAL;

	msgs[0].addr = client->addr;
	msgs[0].flags = client->flags & I2C_M_TEN;
	msgs[0].len = write_len;
	msgs[0].buf = write_buf;
	rc = i2c_transfer(client->adapter, msgs, msg_count);

	if (rc < 0 || rc != msg_count)
		return (rc < 0) ? rc : -EIO;

	rc = 0;

	if (read_buf)
		rc = cyttsp5_i2c_read_default_nosize(dev, read_buf,
				CY_I2C_DATA_SIZE);

	return rc;
}

int cyttsp5_tp_i2c_pinctrl_select(struct device *dev, int state)
{
#if 0
	struct i2c_client *client = to_i2c_client(dev);
	int err;

	switch(state){
		case 0:
			pr_debug("%s: pinctrl default\n",__func__);
			err = i2c_pinctrl_set_default(client->adapter);
			break;
		case 1:
			pr_debug("%s: pinctrl active\n",__func__);
			err = i2c_pinctrl_set_active(client->adapter);
			break;
		case 2:
			pr_debug("%s: pinctrl sleep\n",__func__);
			err = i2c_pinctrl_set_sleep(client->adapter);
			break;
		default:
			pr_err("%s: state is error\n",__func__);
			break;
	}
	return err;
#else
	return 0;
#endif
}

static struct cyttsp5_bus_ops cyttsp5_i2c_bus_ops = {
	.bustype = BUS_I2C,
	.read_default = cyttsp5_i2c_read_default,
	.read_default_nosize = cyttsp5_i2c_read_default_nosize,
	.write_read_specific = cyttsp5_i2c_write_read_specific,
};

static const struct of_device_id cyttsp5_i2c_of_match[] = {
	{ .compatible = "mediatek,cap_touch",},
	{ }
};
MODULE_DEVICE_TABLE(of, cyttsp5_i2c_of_match);

static int cyttsp5_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *i2c_id)
{
	struct device *dev = &client->dev;
#ifdef CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP5_DEVICETREE_SUPPORT
	const struct of_device_id *match;
#endif
	int rc;

	pr_err("[cyttsp5]%s is called\n",__func__);
	
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(dev, "I2C functionality not Supported\n");
		pr_err("I2C functionality not Supported\n");
		return -EIO;
	}

#ifdef CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP5_DEVICETREE_SUPPORT
	match = of_match_device(of_match_ptr(cyttsp5_i2c_of_match), dev);
	if (match) {
		rc = cyttsp5_devtree_create_and_get_pdata(dev);
		if (rc < 0)
		{
			pr_err("cyttsp5_devtree_create_and_get_pdata failed\n");
			return rc;
		}
	}
#endif

	rc = cyttsp5_probe(&cyttsp5_i2c_bus_ops, &client->dev, client->irq,
			  CY_I2C_DATA_SIZE);

#ifdef CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP5_DEVICETREE_SUPPORT
	if (rc && match)
		cyttsp5_devtree_clean_pdata(dev);
#endif

	pr_err("[cyttsp5]%s end\n",__func__);
	return rc;
}

static int cyttsp5_i2c_remove(struct i2c_client *client)
{
#ifdef CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP5_DEVICETREE_SUPPORT
	struct device *dev = &client->dev;
	const struct of_device_id *match;
#endif
	struct cyttsp5_core_data *cd = i2c_get_clientdata(client);

	cyttsp5_release(cd);

#ifdef CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP5_DEVICETREE_SUPPORT
	match = of_match_device(of_match_ptr(cyttsp5_i2c_of_match), dev);
	if (match)
		cyttsp5_devtree_clean_pdata(dev);
#endif

	return 0;
}

static const struct i2c_device_id cyttsp5_i2c_id[] = {
	{ CYTTSP5_I2C_NAME, 0, },
	{ }
};
MODULE_DEVICE_TABLE(i2c, cyttsp5_i2c_id);

void cyttsp5_i2c_shutdown(struct i2c_client *client)
{
	struct cyttsp5_core_data *cd = i2c_get_clientdata(client);

	cyttsp5_shutdown(cd);
}

static struct i2c_driver cyttsp5_i2c_driver = {
	.driver = {
		.name = CYTTSP5_I2C_NAME,
		.owner = THIS_MODULE,
#ifdef CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP5_DEVICETREE_SUPPORT
		.of_match_table = cyttsp5_i2c_of_match,
#endif
	},
	.probe = cyttsp5_i2c_probe,
	.remove = cyttsp5_i2c_remove,
	.id_table = cyttsp5_i2c_id,
	.shutdown = cyttsp5_i2c_shutdown,
};

static int tpd_local_init(void)
{
	pr_err("[cyttsp5]%s is called\n",__func__);

	if (i2c_add_driver(&cyttsp5_i2c_driver) != 0) {
		pr_err("unable to add i2c driver.");
		return -1;
	}


	/*disable auto load touch driver for linux3.0 porting*/
	if (tpd_load_status == 0) {
		pr_err("[cyttsp5]add error touch panel driver.");
		i2c_del_driver(&cyttsp5_i2c_driver);
		return -1;
	}

	pr_err("end [cyttsp5]%s, %d\n", __func__, __LINE__);
	tpd_type_cap = 1;
	return 0;

}

static void tpd_resume(struct device *h)
{
	pr_err("%s is called\n",__func__);

	pr_err("cyttsp5 TPD wake up done\n");
}

static void tpd_suspend(struct device *h) 
{
    pr_err("%s is called\n",__func__);

    pr_err("cyttsp5 TPD enter sleep done\n");
}

static struct tpd_driver_t tpd_device_driver = {
	.tpd_device_name = CYTTSP5_I2C_NAME,
	.tpd_local_init = tpd_local_init,
	.suspend = tpd_suspend,
	.resume = tpd_resume,
#ifdef TPD_HAVE_BUTTON
	.tpd_have_button = 1,
#else
	.tpd_have_button = 0,
#endif
};

static int __init cyttsp5_i2c_init(void)
{
	pr_err("%s is called\n",__func__);
	
	tpd_get_dts_info();
	if(tpd_driver_add(&tpd_device_driver) < 0)
		pr_err("add Parade driver failed\n");
	return 0;
}
static void __exit cyttsp5_i2c_exit(void)
{
	pr_err("%s is called\n",__func__);
	tpd_driver_remove(&tpd_device_driver);
}
module_init(cyttsp5_i2c_init);
module_exit(cyttsp5_i2c_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Cypress TrueTouch(R) Standard Product I2C driver");
MODULE_AUTHOR("Cypress Semiconductor <ttdrivers@cypress.com>");

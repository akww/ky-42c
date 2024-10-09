/*
 * This software is contributed or developed by KYOCERA Corporation.
 * (C) 2022 KYOCERA Corporation
 *
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 *
 * History: V1.0 --- [2013.03.14]Driver creation
 *          V1.1 --- [2013.07.03]Re-write I2C function to fix the bug that
 *                               i2c access error on MT6589 platform.
 *          V1.2 --- [2013.07.04]Add self test function.
 *          V1.3 --- [2013.07.04]Support new chip id 0x57 and 0x58.
 */

#define pr_fmt(fmt) "[barometer] " fmt

#include "barohub.h"
#include <barometer.h>
#include <hwmsensor.h>
#include <SCP_sensorHub.h>
#include "SCP_power_monitor.h"

/* trace */
enum BAR_TRC {
	BAR_TRC_READ = 0x01,
	BAR_TRC_RAWDATA = 0x02,
	BAR_TRC_IOCTL = 0x04,
	BAR_TRC_FILTER = 0x08,
};

/* barohub i2c client data */
struct barohub_ipi_data {
	/* sensor info */
	atomic_t trace;
	atomic_t suspend;
	struct work_struct init_done_work;
	atomic_t scp_init_done;
	atomic_t first_ready_after_boot;
	bool factory_enable;
	bool android_enable;
	bool temp_android_enable;
	struct sensorInfo_t baro_info;
	int32_t config_data[2];
};

static struct barohub_ipi_data *obj_ipi_data;
static int barohub_local_init(void);
static int barohub_local_remove(void);
static int barohub_init_flag = -1;
static DEFINE_SPINLOCK(calibration_lock);
static struct baro_init_info barohub_init_info = {
	.name = "barohub",
	.init = barohub_local_init,
	.uninit = barohub_local_remove,
};

static int barohub_set_powermode(bool enable)
{
	int err = 0;

	err = sensor_enable_to_hub(ID_PRESSURE, enable);
	if (err < 0)
		pr_err("SCP_sensorHub_req_send fail!\n");

	return err;
}

static int barohub_temp_set_powermode(bool enable)
{
	int err = 0;

	err = sensor_enable_to_hub(ID_AMBIENT_TEMPERATURE, enable);
	if (err < 0)
		pr_err("SCP_sensorHub_req_send fail!\n");

	return err;
}

/*
 *get compensated pressure
 *unit: hectopascal(hPa)
 */
static int barohub_get_pressure(char *buf, int bufsize)
{
	struct barohub_ipi_data *obj = obj_ipi_data;
	struct data_unit_t data;
	uint64_t time_stamp = 0;
	int pressure;
	int err = 0;

	if (atomic_read(&obj->suspend))
		return -3;

	if (buf == NULL)
		return -1;
	err = sensor_get_data_from_hub(ID_PRESSURE, &data);
	if (err < 0) {
		pr_err("sensor_get_data_from_hub fail!\n");
		return err;
	}

	time_stamp		= data.time_stamp;
	pressure		= data.pressure_t.pressure;
	sprintf(buf, "%08x", pressure);
	if (atomic_read(&obj->trace) & BAR_TRC_IOCTL)
		pr_debug("compensated pressure value: %s\n", buf);

	return err;
}
static ssize_t show_sensordata_value(struct device_driver *ddri, char *buf)
{
	char strbuf[BAROHUB_BUFSIZE] = {0};
	int err = 0;

	err = barohub_set_powermode(true);
	if (err < 0) {
		pr_err("barohub_set_powermode fail!!\n");
		return 0;
	}
	err = barohub_get_pressure(strbuf, BAROHUB_BUFSIZE);
	if (err < 0) {
		pr_err("barohub_set_powermode fail!!\n");
		return 0;
	}
	err = barohub_set_powermode(false);
	if (err < 0) {
		pr_err("barohub_set_powermode(off) fail!!\n");
		return 0;
	}

	return snprintf(buf, PAGE_SIZE, "%s\n", strbuf);
}

static int barohub_get_temperature(char *buf, int bufsize)
{
	struct barohub_ipi_data *obj = obj_ipi_data;
	struct data_unit_t data;
	uint64_t time_stamp = 0;
	int temperature;
	int err = 0;

	if (atomic_read(&obj->suspend))
		return -3;

	if (buf == NULL)
		return -1;
	err = sensor_get_data_from_hub(ID_AMBIENT_TEMPERATURE, &data);
	if (err < 0) {
		pr_err("sensor_get_data_from_hub fail!\n");
		return err;
	}

	time_stamp		= data.time_stamp;
	temperature		= data.temperature;
	sprintf(buf, "%08x", temperature);
	if (atomic_read(&obj->trace) & BAR_TRC_IOCTL)
		pr_debug("compensated temperature value: %s\n", buf);

	return err;
}
static ssize_t show_temp_sensordata_value(struct device_driver *ddri, char *buf)
{
	char strbuf[BAROHUB_BUFSIZE] = {0};
	int err = 0;

	err = barohub_temp_set_powermode(true);
	if (err < 0) {
		pr_err("barohub_temp_set_powermode fail!!\n");
		return 0;
	}
	err = barohub_get_temperature(strbuf, BAROHUB_BUFSIZE);
	if (err < 0) {
		pr_err("barohub_get_temperature fail!!\n");
		return 0;
	}
	err = barohub_temp_set_powermode(false);
	if (err < 0) {
		pr_err("barohub_temp_set_powermode(off) fail!!\n");
		return 0;
	}

	return snprintf(buf, PAGE_SIZE, "%s\n", strbuf);
}

static ssize_t show_trace_value(struct device_driver *ddri, char *buf)
{
	ssize_t res = 0;
	struct barohub_ipi_data *obj = obj_ipi_data;

	if (obj == NULL) {
		pr_err("pointer is null\n");
		return 0;
	}

	res = snprintf(buf, PAGE_SIZE, "0x%04X\n", atomic_read(&obj->trace));
	return res;
}

static ssize_t store_trace_value(struct device_driver *ddri,
			const char *buf, size_t count)
{
	struct barohub_ipi_data *obj = obj_ipi_data;
	int trace = 0, res = 0;

	if (obj == NULL) {
		pr_err("obj is null\n");
		return 0;
	}
	res = kstrtoint(buf, 10, &trace);
	if (res != 0) {
		pr_err("invalid content: '%s', length = %d\n",
							buf, (int)count);
		return count;
	}
	atomic_set(&obj->trace, trace);
	res = sensor_set_cmd_to_hub(ID_PRESSURE, CUST_ACTION_SET_TRACE, &trace);
	if (res < 0) {
		pr_err("sensor_set_cmd_to_hub fail,(ID: %d),(action: %d)\n",
			ID_PRESSURE, CUST_ACTION_SET_TRACE);
		return 0;
	}
	return count;
}

static int barohub_ReadDeviceId(char *buf, int bufsize)
{
	u8 databuf[10];
	struct barohub_ipi_data *obj = obj_ipi_data;
	int err = 0;

	memset(databuf, 0, sizeof(u8) * 10);

	if ((buf == NULL) || (bufsize <= 30))
		return -1;

	err = sensor_set_cmd_to_hub(ID_PRESSURE, CUST_ACTION_GET_SENSOR_INFO, &obj->baro_info);
	if (err < 0) {
		pr_err("set_cmd_to_hub fail, (ID: %d),(action: %d)\n",
			ID_PRESSURE, CUST_ACTION_GET_SENSOR_INFO);
		return -1;
	}

	sprintf(buf, "0x%x", obj->baro_info.deviceId);
	return 0;
}

static ssize_t show_deviceid(struct device_driver *ddri, char *buf)
{
	char strbuf[BAROHUB_BUFSIZE];

	barohub_ReadDeviceId(strbuf, BAROHUB_BUFSIZE);
	return snprintf(buf, PAGE_SIZE, "%s\n", strbuf);
}

static DRIVER_ATTR(sensordata, 0444, show_sensordata_value, NULL);
static DRIVER_ATTR(temp_sensordata, 0444, show_temp_sensordata_value, NULL);
static DRIVER_ATTR(trace, 0644, show_trace_value, store_trace_value);
static DRIVER_ATTR(deviceid, 0444, show_deviceid, NULL);

static struct driver_attribute *barohub_attr_list[] = {
	&driver_attr_sensordata,	/* dump sensor data */
	&driver_attr_temp_sensordata,	/* dump temp sensor data */
	&driver_attr_trace,	/* trace log */
	&driver_attr_deviceid,
};

static int barohub_create_attr(struct device_driver *driver)
{
	int idx = 0, err = 0;
	int num = (int)(ARRAY_SIZE(barohub_attr_list));

	if (driver == NULL)
		return -EINVAL;

	for (idx = 0; idx < num; idx++) {
		err = driver_create_file(driver, barohub_attr_list[idx]);
		if (err) {
			pr_err("driver_create_file (%s) = %d\n",
				barohub_attr_list[idx]->attr.name, err);
			break;
		}
	}
	return err;
}

static int barohub_delete_attr(struct device_driver *driver)
{
	int idx = 0, err = 0;
	int num = (int)(ARRAY_SIZE(barohub_attr_list));

	if (driver == NULL)
		return -EINVAL;

	for (idx = 0; idx < num; idx++)
		driver_remove_file(driver, barohub_attr_list[idx]);

	return err;
}

static void scp_init_work_done(struct work_struct *work)
{
	int err = 0;
	int32_t cfg_data[2] = {0};
	struct barohub_ipi_data *obj = obj_ipi_data;

	if (atomic_read(&obj->scp_init_done) == 0) {
		pr_debug("scp is not ready to send cmd\n");
		return;
	}
	if (atomic_xchg(&obj->first_ready_after_boot, 1) == 0)
		return;
	spin_lock(&calibration_lock);
	cfg_data[0] = obj->config_data[0];
	cfg_data[1] = obj->config_data[1];
	spin_unlock(&calibration_lock);
	err = sensor_cfg_to_hub(ID_PRESSURE, (uint8_t *)cfg_data,
				sizeof(cfg_data));
	if (err < 0)
		pr_err("sensor_cfg_to_hub fail\n");
}

static int baro_recv_data(struct data_unit_t *event, void *reserved)
{
	int err = 0;
	struct barohub_ipi_data *obj = obj_ipi_data;
	int32_t cali_data[2] = {0};

	if (event->flush_action == FLUSH_ACTION)
		err = baro_flush_report();
	else if (event->flush_action == DATA_ACTION &&
			READ_ONCE(obj->android_enable) == true)
		err = baro_data_report(event->pressure_t.pressure, 2,
			(int64_t)event->time_stamp);
    else if (event->flush_action == CALI_ACTION) {
        cali_data[0] = event->data[0];
        cali_data[1] = event->data[1];
        err = baro_cali_report(cali_data);
        spin_lock(&calibration_lock);
        obj->config_data[0] = event->data[0];
        obj->config_data[1] = event->data[1];
        spin_unlock(&calibration_lock);
    }
	return err;
}

static int temp_recv_data(struct data_unit_t *event, void *reserved)
{
	int err = 0;
	struct barohub_ipi_data *obj = obj_ipi_data;
	pr_debug("temp_recv_data,(en:%d) value(%d)\n", (READ_ONCE(obj->temp_android_enable)?1:0), event->temperature);

	if (event->flush_action == FLUSH_ACTION)
		err = temp_flush_report();
	else if (event->flush_action == DATA_ACTION &&
			READ_ONCE(obj->temp_android_enable) == true)
		err = temp_data_report(event->temperature, 2,
			(int64_t)event->time_stamp);
	return err;
}



static int barohub_factory_enable_sensor(bool enabledisable,
				int64_t sample_periods_ms)
{
	int err = 0;
	struct barohub_ipi_data *obj = obj_ipi_data;

	if (enabledisable == true)
		WRITE_ONCE(obj->factory_enable, true);
	else
		WRITE_ONCE(obj->factory_enable, false);

	if (enabledisable == true) {
		err = sensor_set_delay_to_hub(ID_PRESSURE, sample_periods_ms);
		if (err) {
			pr_err("sensor_set_delay_to_hub failed!\n");
			return -1;
		}
	}
	err = sensor_enable_to_hub(ID_PRESSURE, enabledisable);
	if (err) {
		pr_err("sensor_enable_to_hub failed!\n");
		return -1;
	}
	return 0;
}
static int barohub_factory_get_data(int32_t *data)
{
	int err = 0;
	char strbuf[BAROHUB_BUFSIZE];

	err = barohub_get_pressure(strbuf, BAROHUB_BUFSIZE);
	if (err < 0) {
		pr_err("barohub_get_pressure fail\n");
		return -1;
	}
	err = kstrtoint(strbuf, 16, data);
	if (err != 0)
		pr_debug("kstrtoint fail\n");

	return 0;
}
static int barohub_factory_get_raw_data(int32_t *data)
{
	return 0;
}
static int barohub_factory_enable_calibration(void)
{
	return sensor_calibration_to_hub(ID_PRESSURE);
}
static int barohub_factory_clear_cali(void)
{
	return 0;
}
static int barohub_factory_set_cali(int32_t offset)
{
	return 0;
}
static int barohub_factory_get_cali(int32_t *offset)
{
	return 0;
}
static int barohub_factory_do_self_test(void)
{
	return 0;
}
static int barohub_factory_temp_get_data(int32_t *data)
{
	int err = 0;
	char strbuf[BAROHUB_BUFSIZE];

	err = barohub_get_temperature(strbuf, BAROHUB_BUFSIZE);
	if (err < 0) {
		pr_err("barohub_get_temperature fail\n");
		return -1;
	}
	err = kstrtoint(strbuf, 16, data);
	if (err != 0)
		pr_debug("kstrtoint fail\n");

	return 0;
}

static struct baro_factory_fops barohub_factory_fops = {
	.enable_sensor = barohub_factory_enable_sensor,
	.get_data = barohub_factory_get_data,
	.get_raw_data = barohub_factory_get_raw_data,
	.enable_calibration = barohub_factory_enable_calibration,
	.clear_cali = barohub_factory_clear_cali,
	.set_cali = barohub_factory_set_cali,
	.get_cali = barohub_factory_get_cali,
	.do_self_test = barohub_factory_do_self_test,
	.temp_get_data = barohub_factory_temp_get_data,
};

static struct baro_factory_public barohub_factory_device = {
	.gain = 1,
	.sensitivity = 1,
	.fops = &barohub_factory_fops,
};

static int barohub_open_report_data(int open)
{
	return 0;
}

static int barohub_temp_open_report_data(int open)
{
	return 0;
}

static int barohub_enable_nodata(int en)
{
	int res = 0;
	bool power = false;
	struct barohub_ipi_data *obj = obj_ipi_data;

	if (en == true)
		WRITE_ONCE(obj->android_enable, true);
	else
		WRITE_ONCE(obj->android_enable, false);

	if (en == 1)
		power = true;
	if (en == 0)
		power = false;

	res = barohub_set_powermode(power);
	if (res < 0) {
		pr_debug("barohub_set_powermode fail\n");
		return res;
	}
	pr_debug("barohub_set_powermode OK!\n");
	return res;
}
static int barohub_temp_enable_nodata(int en)
{
	int res = 0;
	bool power = false;
	struct barohub_ipi_data *obj = obj_ipi_data;

	if (en == true)
		WRITE_ONCE(obj->temp_android_enable, true);
	else
		WRITE_ONCE(obj->temp_android_enable, false);

	if (en == 1)
		power = true;
	if (en == 0)
		power = false;

	res = barohub_temp_set_powermode(power);
	if (res < 0) {
		pr_debug("barohub_temp_set_powermode fail\n");
		return res;
	}
	pr_debug("barohub_temp_set_powermode OK!\n");
	return res;
}

static int barohub_set_delay(u64 ns)
{
#if defined CONFIG_MTK_SCP_SENSORHUB_V1
	int err = 0;
	unsigned int delayms = 0;
	struct barohub_ipi_data *obj = obj_ipi_data;

	delayms = (unsigned int)ns / 1000 / 1000;
	err = sensor_set_delay_to_hub(ID_PRESSURE, delayms);
	if (err < 0) {
		pr_err("als_set_delay fail!\n");
		return err;
	}
	return 0;
#elif defined CONFIG_NANOHUB
	return 0;
#else
	return 0;
#endif
}

static int barohub_temp_set_delay(u64 ns)
{
#if defined CONFIG_MTK_SCP_SENSORHUB_V1
	int err = 0;
	unsigned int delayms = 0;
	struct barohub_ipi_data *obj = obj_ipi_data;

	delayms = (unsigned int)ns / 1000 / 1000;
	err = sensor_set_delay_to_hub(ID_AMBIENT_TEMPERATURE, delayms);
	if (err < 0) {
		pr_err("als_set_delay fail!\n");
		return err;
	}
	return 0;
#elif defined CONFIG_NANOHUB
	return 0;
#else
	return 0;
#endif
}

static int barohub_batch(int flag,
	int64_t samplingPeriodNs, int64_t maxBatchReportLatencyNs)
{
#if defined CONFIG_MTK_SCP_SENSORHUB_V1
	barohub_set_delay(samplingPeriodNs);
#endif
	return sensor_batch_to_hub(ID_PRESSURE,
		flag, samplingPeriodNs, maxBatchReportLatencyNs);
}

static int barohub_temp_batch(int flag,
	int64_t samplingPeriodNs, int64_t maxBatchReportLatencyNs)
{
#if defined CONFIG_MTK_SCP_SENSORHUB_V1
	barohub_temp_set_delay(samplingPeriodNs);
#endif

	return sensor_batch_to_hub(ID_AMBIENT_TEMPERATURE,
		flag, samplingPeriodNs, maxBatchReportLatencyNs);
}

static int barohub_flush(void)
{
	return sensor_flush_to_hub(ID_PRESSURE);
}

static int barohub_temp_flush(void)
{
	return sensor_flush_to_hub(ID_AMBIENT_TEMPERATURE);
}

static int barohub_set_cali(uint8_t *data, uint8_t count)
{
	struct barohub_ipi_data *obj = obj_ipi_data;

	spin_lock(&calibration_lock);
	obj->config_data[0] = data[0];
	obj->config_data[1] = data[1];
	spin_unlock(&calibration_lock);

	return sensor_cfg_to_hub(ID_PRESSURE, data, count);
}

static int barohub_get_data(int *value, int *status)
{
	char buff[BAROHUB_BUFSIZE] = {0};
	int err = 0;

	err = barohub_get_pressure(buff, BAROHUB_BUFSIZE);

	if (err) {
		pr_err("get compensated pressure value failed, err = %d\n",
			err);
		return -1;
	}

	err = kstrtoint(buff, 16, value);
	if (err == 0)
		*status = SENSOR_STATUS_ACCURACY_MEDIUM;

	return 0;
}

static int barohub_temp_get_data(int *value, int *status)
{
	char buff[BAROHUB_BUFSIZE] = {0};
	int err = 0;

	err = barohub_get_temperature(buff, BAROHUB_BUFSIZE);

	if (err) {
		pr_err("get compensated pressure value failed, err = %d\n",
			err);
		return -1;
	}

	err = kstrtoint(buff, 16, value);
	if (err == 0)
		*status = SENSOR_STATUS_ACCURACY_MEDIUM;

	return 0;
}

static int scp_ready_event(uint8_t event, void *ptr)
{
	struct barohub_ipi_data *obj = obj_ipi_data;

	switch (event) {
	case SENSOR_POWER_UP:
	    atomic_set(&obj->scp_init_done, 1);
	    schedule_work(&obj->init_done_work);
		break;
	case SENSOR_POWER_DOWN:
	    atomic_set(&obj->scp_init_done, 0);
		break;
	}
	return 0;
}

static struct scp_power_monitor scp_ready_notifier = {
	.name = "baro",
	.notifier_call = scp_ready_event,
};

static int barohub_probe(struct platform_device *pdev)
{
	struct barohub_ipi_data *obj;
	struct baro_control_path ctl = { 0 };
	struct temp_control_path temp_ctl = { 0 };
	struct baro_data_path data = { 0 };
	struct temp_data_path temp_data = { 0 };
	int err = 0;
	struct platform_driver *paddr =
				barohub_init_info.platform_diver_addr;

	pr_debug("%s\n", __func__);

	obj = kzalloc(sizeof(*obj), GFP_KERNEL);
	if (!obj) {
		err = -ENOMEM;
		goto exit;
	}

	INIT_WORK(&obj->init_done_work, scp_init_work_done);
	obj_ipi_data = obj;
	platform_set_drvdata(pdev, obj);

	atomic_set(&obj->trace, 0);
	atomic_set(&obj->suspend, 0);
	WRITE_ONCE(obj->factory_enable, false);
	WRITE_ONCE(obj->android_enable, false);
	WRITE_ONCE(obj->temp_android_enable, false);

	atomic_set(&obj->scp_init_done, 0);
	atomic_set(&obj->first_ready_after_boot, 0);
	scp_power_monitor_register(&scp_ready_notifier);
	err = scp_sensorHub_data_registration(ID_PRESSURE, baro_recv_data);
	if (err < 0) {
		pr_err("scp_sensorHub_data_registration failed\n");
		goto exit_kfree;
	}
	err = scp_sensorHub_data_registration(ID_AMBIENT_TEMPERATURE, temp_recv_data);
	if (err < 0) {
		pr_err("scp_sensorHub_data_registration [temp] failed\n");
		goto exit_kfree;
	}

	err = baro_factory_device_register(&barohub_factory_device);
	if (err) {
		pr_err("baro_factory_device_register failed, err = %d\n",
			err);
		goto exit_kfree;
	}

	ctl.is_use_common_factory = false;
	err = barohub_create_attr(&paddr->driver);
	if (err) {
		pr_err("create attribute failed, err = %d\n", err);
		goto exit_create_attr_failed;
	}

	ctl.open_report_data = barohub_open_report_data;
	ctl.enable_nodata = barohub_enable_nodata;
	ctl.set_delay = barohub_set_delay;
	ctl.batch = barohub_batch;
	ctl.flush = barohub_flush;
	ctl.set_cali = barohub_set_cali;
#if defined CONFIG_MTK_SCP_SENSORHUB_V1
	ctl.is_report_input_direct = false;
	ctl.is_support_batch = false;
#elif defined CONFIG_NANOHUB
	ctl.is_report_input_direct = true;
	ctl.is_support_batch = true;
#else
#endif
	err = baro_register_control_path(&ctl);
	if (err) {
		pr_err("register baro control path err\n");
		goto exit_create_attr_failed;
	}

	data.get_data = barohub_get_data;
	data.vender_div = 100;
	err = baro_register_data_path(&data);
	if (err) {
		pr_err("baro_register_data_path failed, err = %d\n", err);
		goto exit_create_attr_failed;
	}

	temp_ctl.is_use_common_factory = false;
	temp_ctl.open_report_data = barohub_temp_open_report_data;
	temp_ctl.enable_nodata = barohub_temp_enable_nodata;
	temp_ctl.set_delay = barohub_temp_set_delay;
	temp_ctl.batch = barohub_temp_batch;
	temp_ctl.flush = barohub_temp_flush;
#if defined CONFIG_MTK_SCP_SENSORHUB_V1
	temp_ctl.is_report_input_direct = false;
	temp_ctl.is_support_batch = false;
#elif defined CONFIG_NANOHUB
	temp_ctl.is_report_input_direct = true;
	temp_ctl.is_support_batch = true;
#else
#endif

	err = temp_register_control_path(&temp_ctl);
	if (err) {
		pr_err("register baro control path err\n");
		goto exit_create_attr_failed;
	}

	temp_data.get_data = barohub_temp_get_data;
	temp_data.vender_div = 100; //Must match the definition "AMBIENT_TEMP_INCREASE_NUM_AP" on the SCP side.
	err = temp_register_data_path(&temp_data);
	if (err) {
		pr_err("baro_register_data_path failed, err = %d\n", err);
		goto exit_create_attr_failed;
	}


	barohub_init_flag = 0;
	pr_debug("%s: OK\n", __func__);
	return 0;


exit_create_attr_failed:
	barohub_delete_attr(&(barohub_init_info.platform_diver_addr->driver));
exit_kfree:
	kfree(obj);
	obj_ipi_data = NULL;
exit:
	pr_err("err = %d\n", err);
	barohub_init_flag = -1;
	return err;
}

static int barohub_remove(struct platform_device *pdev)
{
	int err = 0;
	struct platform_driver *paddr =
				barohub_init_info.platform_diver_addr;

	err = barohub_delete_attr(&paddr->driver);
	if (err)
		pr_err("barohub_delete_attr failed, err = %d\n", err);

	baro_factory_device_deregister(&barohub_factory_device);

	obj_ipi_data = NULL;
	kfree(platform_get_drvdata(pdev));

	return 0;
}
static int barohub_suspend(struct platform_device *pdev, pm_message_t msg)
{
	return 0;
}

static int barohub_resume(struct platform_device *pdev)
{
	return 0;
}
static struct platform_device barohub_device = {
	.name = BAROHUB_DEV_NAME,
	.id = -1,
};

static struct platform_driver barohub_driver = {
	.driver = {
		.name = BAROHUB_DEV_NAME,
	},
	.probe = barohub_probe,
	.remove = barohub_remove,
	.suspend = barohub_suspend,
	.resume = barohub_resume,
};

static int barohub_local_remove(void)
{
	pr_debug("%s\n", __func__);
	platform_driver_unregister(&barohub_driver);
	return 0;
}

static int barohub_local_init(void)
{
	if (platform_driver_register(&barohub_driver)) {
		pr_err("add driver error\n");
		return -1;
	}
	if (-1 == barohub_init_flag)
		return -1;
	return 0;
}

static int __init barohub_init(void)
{
	pr_debug("%s\n", __func__);
	if (platform_device_register(&barohub_device)) {
		pr_err("baro platform device error\n");
		return -1;
	}
	baro_driver_add(&barohub_init_info);
	return 0;
}

static void __exit barohub_exit(void)
{
	pr_debug("%s\n", __func__);
	platform_driver_unregister(&barohub_driver);
}

module_init(barohub_init);
module_exit(barohub_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("BAROHUB Driver");
MODULE_AUTHOR("hongxu.zhao@mediatek.com");
MODULE_VERSION(BAROHUB_DRIVER_VERSION);

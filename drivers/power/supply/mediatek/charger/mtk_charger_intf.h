/*
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
 */
/*
 * This software is contributed or developed by KYOCERA Corporation.
 * (C) 2020 KYOCERA Corporation
 */

#ifndef __MTK_CHARGER_INTF_H__
#define __MTK_CHARGER_INTF_H__

#include <linux/device.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/mutex.h>
#include <linux/notifier.h>
#include <linux/spinlock.h>
#include <linux/timer.h>
#include <linux/wait.h>
#include <linux/alarmtimer.h>
#include <mt-plat/charger_type.h>
#include <mt-plat/mtk_charger.h>
#include <mt-plat/mtk_battery.h>

#include <mtk_gauge_time_service.h>

#include <mt-plat/charger_class.h>

#include <tcpm.h>
#include <tcpci.h>

struct charger_manager;
#include "mtk_pe_intf.h"
#include "mtk_pe20_intf.h"
#include "mtk_pe40_intf.h"
#include "mtk_pdc_intf.h"
#include "adapter_class.h"

#define CHARGING_INTERVAL 10
#define CHARGING_FULL_INTERVAL 20

#define CHRLOG_ERROR_LEVEL   1
#define CHRLOG_DEBUG_LEVEL   2

extern int chr_get_debug_level(void);

#define chr_err(fmt, args...)					\
do {								\
	if (chr_get_debug_level() >= CHRLOG_ERROR_LEVEL) {	\
		pr_notice(fmt, ##args);				\
	}							\
} while (0)

#define chr_info(fmt, args...)					\
do {								\
	if (chr_get_debug_level() >= CHRLOG_ERROR_LEVEL) {	\
		pr_notice_ratelimited(fmt, ##args);		\
	}							\
} while (0)

#define chr_debug(fmt, args...)					\
do {								\
	if (chr_get_debug_level() >= CHRLOG_DEBUG_LEVEL) {	\
		pr_notice(fmt, ##args);				\
	}							\
} while (0)

#define CHR_CC		(0x0001)
#define CHR_TOPOFF	(0x0002)
#define CHR_TUNING	(0x0003)
#define CHR_POSTCC	(0x0004)
#define CHR_BATFULL	(0x0005)
#define CHR_ERROR	(0x0006)
#define	CHR_PE40_INIT	(0x0007)
#define	CHR_PE40_CC	(0x0008)
#define	CHR_PE40_TUNING	(0x0009)
#define	CHR_PE40_POSTCC	(0x000A)
#define CHR_PE30	(0x000B)

/* charging abnormal status */
#define CHG_VBUS_OV_STATUS	(1 << 0)
#define CHG_BAT_OT_STATUS	(1 << 1)
#define CHG_OC_STATUS		(1 << 2)
#define CHG_BAT_OV_STATUS	(1 << 3)
#define CHG_ST_TMO_STATUS	(1 << 4)
#define CHG_BAT_LT_STATUS	(1 << 5)
#define CHG_TYPEC_WD_STATUS	(1 << 6)

/* oem charging log */
#define OEM_CHGLOG_WARM_LIMIT_BIT	(1 << 0)
#define OEM_CHGLOG_WARM_STOP_BIT	(1 << 1)
#define OEM_CHGLOG_COOL_LIMIT_BIT	(1 << 2)
#define OEM_CHGLOG_COOL_STOP_BIT	(1 << 3)
#define OEM_CHGLOG_CONN_LIMIT_BIT	(1 << 4)
#define OEM_CHGLOG_CONN_STOP_BIT	(1 << 5)
#define OEM_CHGLOG_CHARGER_NG_BIT	(1 << 6)
#define OEM_CHGLOG_CABLE_NG_BIT		(1 << 7)
#define OEM_CHGLOG_CHARGER_BAD_BIT	(1 << 8)
#define OEM_CHGLOG_CABLE_BAD_BIT	(1 << 9)
#define OEM_CHGLOG_WIRELESS_CHG_BIT	(1 << 10)
#define OEM_CHGLOG_POOR_CHARGER_BIT (1 << 11)
#define OEM_CHGLOG_TEMP_MASK		GENMASK(3, 0)
#define OEM_CHGLOG_MONITOR_MASK		GENMASK(9, 6)
#define OEM_CHGLOG_MASK				GENMASK(11, 0)
#define is_oem_chglog_temp_bit(bit) (bit & OEM_CHGLOG_TEMP_MASK)
#define is_oem_chglog_bit(bit) (bit & OEM_CHGLOG_MASK)

/* charger_algorithm notify charger_dev */
enum {
	EVENT_EOC,
	EVENT_RECHARGE,
};

/* charger_dev notify charger_manager */
enum {
	CHARGER_DEV_NOTIFY_VBUS_OVP,
	CHARGER_DEV_NOTIFY_BAT_OVP,
	CHARGER_DEV_NOTIFY_EOC,
	CHARGER_DEV_NOTIFY_RECHG,
	CHARGER_DEV_NOTIFY_SAFETY_TIMEOUT,
};

/*
 * Software JEITA
 * T0: -10 degree Celsius
 * T1: 0 degree Celsius
 * T2: 10 degree Celsius
 * T3: 45 degree Celsius
 * T4: 50 degree Celsius
 */
enum sw_jeita_state_enum {
	TEMP_BELOW_T0 = 0,
	TEMP_T0_TO_T1,
	TEMP_T1_TO_T2,
	TEMP_T2_TO_T3,
	TEMP_T3_TO_T4,
	TEMP_ABOVE_T4
};

struct sw_jeita_data {
	int sm;
	int pre_sm;
	int cv;
	bool charging;
	bool error_recovery_flag;
};

/* battery thermal protection */
enum bat_temp_state_enum {
	BAT_TEMP_LOW = 0,
	BAT_TEMP_NORMAL,
	BAT_TEMP_HIGH
};

/*-------*/
enum {
	OEM_USBIN_INIT = -1,
	OEM_USBIN_LOW = 0,
	OEM_USBIN_HIGH = 1,
};

enum {
	OEM_CHG_PAD_INIT = -1,
	OEM_CHG_PAD_DET = 0,
	OEM_CHG_PAD_REMOVE = 1,
};

enum {
	OEM_ACC_PAD_INIT = -1,
	OEM_ACC_DET = 0,
	OEM_ACC_REMOVE = 1,
};
/*-------*/

struct battery_thermal_protection_data {
	int sm;
	bool enable_min_charge_temp;
	int min_charge_temp;
	int min_charge_temp_plus_x_degree;
	int max_charge_temp;
	int max_charge_temp_minus_x_degree;
};

struct charger_custom_data {
	int battery_cv;	/* uv */
	int max_charger_voltage;
	int max_charger_voltage_setting;
	int min_charger_voltage;

	int usb_charger_current_suspend;
	int usb_charger_current_unconfigured;
	int usb_charger_current_configured;
	int usb_charger_current;
	int ac_charger_current;
	int ac_charger_input_current;
	int non_std_ac_charger_current;
	int charging_host_charger_current;
	int apple_1_0a_charger_current;
	int apple_2_1a_charger_current;
	int ta_ac_charger_current;
	int pd_charger_current;
	int oem_input_current_limit_cc_30;
	int oem_charging_current_limit_cc_30;
	int oem_input_current_limit_cc_15;
	int oem_charging_current_limit_cc_15;
	int default_input_current_lim;
	int default_charging_cur_lim;

	/* dynamic mivr */
	int min_charger_voltage_1;
	int min_charger_voltage_2;
	int max_dmivr_charger_current;

	/* sw jeita */
	int jeita_temp_above_t4_cv;
	int jeita_temp_t3_to_t4_cv;
	int jeita_temp_t2_to_t3_cv;
	int jeita_temp_t1_to_t2_cv;
	int jeita_temp_t0_to_t1_cv;
	int jeita_temp_below_t0_cv;
	int temp_t4_thres;
	int temp_t4_thres_minus_x_degree;
	int temp_t3_thres;
	int temp_t3_thres_minus_x_degree;
	int temp_t2_thres;
	int temp_t2_thres_plus_x_degree;
	int temp_t1_thres;
	int temp_t1_thres_plus_x_degree;
	int temp_t0_thres;
	int temp_t0_thres_plus_x_degree;
	int temp_neg_10_thres;
	int oem_jeita_temp_t3_to_t4_cc;
	int oem_jeita_temp_t2_to_t3_cc;
	int oem_jeita_temp_t1_to_t2_cc;
	int oem_jeita_temp_t0_to_t1_cc;

	/* battery temperature protection */
	int mtk_temperature_recharge_support;
	int max_charge_temp;
	int max_charge_temp_minus_x_degree;
	int min_charge_temp;
	int min_charge_temp_plus_x_degree;

	/* pe */
	int pe_ichg_level_threshold;	/* ma */
	int ta_ac_12v_input_current;
	int ta_ac_9v_input_current;
	int ta_ac_7v_input_current;
	bool ta_12v_support;
	bool ta_9v_support;

	/* pe2.0 */
	int pe20_ichg_level_threshold;	/* ma */
	int ta_start_battery_soc;
	int ta_stop_battery_soc;

	/* pe4.0 */
	int pe40_single_charger_input_current;	/* ma */
	int pe40_single_charger_current;
	int pe40_dual_charger_input_current;
	int pe40_dual_charger_chg1_current;
	int pe40_dual_charger_chg2_current;
	int pe40_stop_battery_soc;
	int pe40_max_vbus;
	int pe40_max_ibus;
	int high_temp_to_leave_pe40;
	int high_temp_to_enter_pe40;
	int low_temp_to_leave_pe40;
	int low_temp_to_enter_pe40;

	/* pe4.0 cable impedance threshold (mohm) */
	u32 pe40_r_cable_1a_lower;
	u32 pe40_r_cable_2a_lower;
	u32 pe40_r_cable_3a_lower;

	/* dual charger */
	u32 chg1_ta_ac_charger_current;
	u32 chg2_ta_ac_charger_current;
	int slave_mivr_diff;
	u32 dual_polling_ieoc;

	/* slave charger */
	int chg2_eff;
	bool parallel_vbus;

	/* cable measurement impedance */
	int cable_imp_threshold;
	int vbat_cable_imp_threshold;

	/* bif */
	int bif_threshold1;	/* uv */
	int bif_threshold2;	/* uv */
	int bif_cv_under_threshold2;	/* uv */

	/* power path */
	bool power_path_support;

	int max_charging_time; /* second */

	int bc12_charger;

	/* pd */
	int pd_vbus_upper_bound;
	int pd_vbus_low_bound;
	int pd_ichg_level_threshold;
	int pd_stop_battery_soc;

	int vsys_watt;
	int ibus_err;

	/* oem add */
	bool	oem_gpio_init;
	bool	 oem_chattering_flag;
	int		*oem_cycle_count_thresh;
	int		*oem_cycle_count_fv_comp_mv;
	int		oem_cycle_count_levels;
	int		*oem_cont_chg_thresh;
	int		*oem_cont_chg_fv_comp_mv;
	int		oem_cont_chg_levels;
	int		*oem_batt_temp_thresh;
	int		*oem_cont_chg_factor;
	int		oem_cont_chg_factor_levels;
	int		oem_batt_care_uv;
	int		oem_usb_gpio;
	int		oem_usb_gpio_val;
	int		oem_chg_pad_gpio;
	int		oem_chg_pad_gpio_val;
	int		oem_acc_det_gpio;
	int		oem_acc_det_gpio_val;
	int		oem_input_current_limit_chgpad;
	int		oem_charging_current_limit_chgpad;
	int		oem_input_current_limit_kcacc;
	int		oem_charging_current_limit_kcacc;
	struct	delayed_work	oem_chg_pad_chattering_work;
};

struct charger_data {
	int force_charging_current;
	int thermal_input_current_limit;
	int thermal_charging_current_limit;
	int input_current_limit;
	int charging_current_limit;
	int disable_charging_count;
	int input_current_limit_by_aicl;
	int junction_temp_min;
	int junction_temp_max;
	int full_charging_capacity;
	int vbat_limitation;
	int fact_chg_time;
	int sdp_charging_current;
	bool is_factory_use;
	int step_chg_cnt;
};

struct charger_manager {
	bool init_done;
	const char *algorithm_name;
	struct platform_device *pdev;
	void	*algorithm_data;
	int usb_state;
	bool usb_unlimited;
	bool disable_charger;

	struct charger_device *chg1_dev;
	struct notifier_block chg1_nb;
	struct charger_data chg1_data;
	struct charger_consumer *chg1_consumer;

	struct charger_device *chg2_dev;
	struct notifier_block chg2_nb;
	struct charger_data chg2_data;

	struct adapter_device *pd_adapter;


	enum charger_type chr_type;
	bool can_charging;
	int cable_out_cnt;

	int (*do_algorithm)(struct charger_manager *cm);
	int (*plug_in)(struct charger_manager *cm);
	int (*plug_out)(struct charger_manager *cm);
	int (*do_charging)(struct charger_manager *cm, bool en);
	int (*do_event)(struct notifier_block *nb, unsigned long ev, void *v);
	int (*change_current_setting)(struct charger_manager *cm);

	/* notify charger user */
	struct srcu_notifier_head evt_nh;
	/* receive from battery */
	struct notifier_block psy_nb;

	/* common info */
	int battery_temp;

	/* sw jeita */
	bool enable_sw_jeita;
	struct sw_jeita_data sw_jeita;

	/* dynamic_cv */
	bool enable_dynamic_cv;

	bool cmd_discharging;
	bool safety_timeout;
	bool vbusov_stat;

	/* battery warning */
	unsigned int notify_code;
	unsigned int notify_test_mode;

	/* battery thermal protection */
	struct battery_thermal_protection_data thermal;

	/* dtsi custom data */
	struct charger_custom_data data;

	bool enable_sw_safety_timer;
	bool sw_safety_timer_setting;

	/* High voltage charging */
	bool enable_hv_charging;

	/* pe */
	bool enable_pe_plus;
	struct mtk_pe pe;

	/* pe 2.0 */
	bool enable_pe_2;
	struct mtk_pe20 pe2;

	/* pe 4.0 */
	bool enable_pe_4;
	struct mtk_pe40 pe4;

	/* type-C*/
	bool enable_type_c;

	/* water detection */
	bool water_detected;

	/* pd */
	struct mtk_pdc pdc;
	bool disable_pd_dual;

	int pd_type;
	//struct tcpc_device *tcpc;
	bool pd_reset;

	/* thread related */
	struct hrtimer charger_kthread_timer;

	/* alarm timer */
	struct alarm charger_timer;
	struct timespec endtime;
	bool is_suspend;

	struct wakeup_source charger_wakelock;
	struct mutex charger_lock;
	struct mutex charger_pd_lock;
	struct mutex cable_out_lock;
	spinlock_t slock;
	unsigned int polling_interval;
	bool charger_thread_timeout;
	wait_queue_head_t  wait_que;
	bool charger_thread_polling;

	/* kpoc */
	atomic_t enable_kpoc_shdn;

	/* ATM */
	bool atm_enabled;

	/* dynamic mivr */
	bool enable_dynamic_mivr;

	/* oem add */
	struct power_supply		*oem_batt_psy;
	struct power_supply		*oem_chg_psy;
	bool					oem_charging_initialized;

	bool oem_auto_on_detect;
	bool oem_chgpad_detect;
	bool oem_acc_detect;
};

/* charger related module interface */
extern int charger_manager_notifier(struct charger_manager *info, int event);
extern int mtk_switch_charging_init(struct charger_manager *info);
extern int mtk_dual_switch_charging_init(struct charger_manager *info);
extern int mtk_linear_charging_init(struct charger_manager *info);
extern void _wake_up_charger(struct charger_manager *info);
extern int mtk_get_dynamic_cv(struct charger_manager *info, unsigned int *cv);
extern bool is_dual_charger_supported(struct charger_manager *info);
extern int charger_enable_vbus_ovp(struct charger_manager *pinfo, bool enable);
extern bool is_typec_adapter(struct charger_manager *info);

/* pmic API */
extern unsigned int upmu_get_rgs_chrdet(void);
extern int pmic_get_vbus(void);
extern int pmic_get_charging_current(void);
extern int pmic_get_battery_voltage(void);
extern int pmic_get_bif_battery_voltage(int *vbat);
extern int pmic_is_bif_exist(void);
extern int pmic_enable_hw_vbus_ovp(bool enable);
extern bool pmic_is_battery_exist(void);

extern void check_dock(void);
extern void check_dock_hole(void);


extern void notify_adapter_event(enum adapter_type type, enum adapter_event evt,
	void *val);

extern int mtk_charger_usb_therm_det(void);
extern int oem_chglog_change(int bit, bool detect);
extern int oem_chglog_thermal_check(int current_limit);
extern int get_prop_batt_status(void);

/* FIXME */
enum usb_state_enum {
	USB_SUSPEND = 0,
	USB_UNCONFIGURED,
	USB_CONFIGURED
};

bool __attribute__((weak)) is_usb_rdy(void)
{
	pr_info("%s is not defined\n", __func__);
	return false;
}

/* procfs */
#define PROC_FOPS_RW(name)						\
static int mtk_chg_##name##_open(struct inode *node, struct file *file)	\
{									\
	return single_open(file, mtk_chg_##name##_show, PDE_DATA(node));\
}									\
static const struct file_operations mtk_chg_##name##_fops = {		\
	.owner = THIS_MODULE,						\
	.open = mtk_chg_##name##_open,					\
	.read = seq_read,						\
	.llseek = seq_lseek,						\
	.release = single_release,					\
	.write = mtk_chg_##name##_write,				\
}

#define MAX_STEP_CHG_ENTRIES	32
#define RANGE_DATA_ENTRIES	3

struct range_data {
	u32 low_threshold;
	u32 high_threshold;
	u32 value;
};

struct step_chg_cfg {
	int			entries;
	int			hysteresis;
	struct range_data	fcc_cfg[MAX_STEP_CHG_ENTRIES];
	struct range_data	fcc_cfg_low[MAX_STEP_CHG_ENTRIES];
};

#endif /* __MTK_CHARGER_INTF_H__ */

/*
 *  GPIO driven matrix keyboard driver
 *
 *  Copyright (c) 2008 Marek Vasut <marek.vasut@gmail.com>
 *
 *  Based on corgikbd.c
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 */
/*
 * This software is contributed or developed by KYOCERA Corporation.
 * (C) 2022 KYOCERA Corporation
 */

#include <linux/types.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/jiffies.h>
#include <linux/module.h>
#include <linux/gpio.h>
#include <linux/input/matrix_keypad.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_platform.h>

// KCTP_CUST +
#include <uapi/linux/kctouch_uapi.h>
// KCTP_CUST -

#ifndef QUALCOMM_ORIGINAL_FEATURE
#include <linux/gpio_keys.h>
#ifdef KCTP_CUST
#include "kckeyptr.h"
#endif /* KCTP_CUST */

#define IRQS_PENDING 0x00000200
#define istate core_internal_state__do_not_mess_with_it
struct wakeup_source matrix_keypad_wake_src;
struct wakeup_source matrix_keypad_scan_wake_src;
#endif /* Not QUALCOMM_ORIGINAL_FEATURE */

struct matrix_keypad {
	const struct matrix_keypad_platform_data *pdata;
	struct input_dev *input_dev;
	unsigned int row_shift;

	DECLARE_BITMAP(disabled_gpios, MATRIX_MAX_ROWS);

	uint32_t last_key_state[MATRIX_MAX_COLS];
	struct delayed_work work;
	spinlock_t lock;
	bool scan_pending;
	bool stopped;
	bool gpio_all_disabled;
#ifndef QUALCOMM_ORIGINAL_FEATURE
	struct pinctrl *matrix_keypad_pinctrl;
#endif /* Not QUALCOMM_ORIGINAL_FEATURE */
};

#ifndef QUALCOMM_ORIGINAL_FEATURE
#define KEYPAD_GROUP_NUM 5

static int matrix_keypad_key_group[MATRIX_MAX_ROWS][MATRIX_MAX_COLS] =
{
	{ 4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4, },
	{ 4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4, },
	{ 4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4, },
	{ 4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4, },
	{ 4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4, },
	{ 4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4, },
	{ 4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4, },
	{ 4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4, },
	{ 4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4, },
	{ 4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4, },
	{ 4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4, },
	{ 4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4, },
	{ 4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4, },
	{ 4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4, },
 };

/* Coordinate of COLUMN and ROW for new_state */
#define KPD_UP_KEY_ROW      2
#define KPD_UP_KEY_COL      5
#define KPD_RI_KEY_ROW      2
#define KPD_RI_KEY_COL      6
#define KPD_DO_KEY_ROW      1
#define KPD_DO_KEY_COL      5
#define KPD_LE_KEY_ROW      1
#define KPD_LE_KEY_COL      6

#define KPD_LE_UP_KEY_ROW   5
#define KPD_LE_UP_KEY_COL   0
#define KPD_RI_UP_KEY_ROW   6
#define KPD_RI_UP_KEY_COL   0
#define KPD_LE_DO_KEY_ROW   7
#define KPD_LE_DO_KEY_COL   0
#define KPD_RI_DO_KEY_ROW   8
#define KPD_RI_DO_KEY_COL   0

struct keypad_8way_to_4way {
	unsigned char diagnal_row;
	unsigned char diagnal_col;
	unsigned char up_do_row;
	unsigned char up_do_col;
	unsigned char le_ri_row;
	unsigned char le_ri_col;
};

#endif /* Not QUALCOMM_ORIGINAL_FEATURE */

#ifdef QUALCOMM_ORIGINAL_FEATURE
#define MATRIX_KEYPAD_ERR_LOG_PRINT(fmt, ...)
#define MATRIX_KEYPAD_DEBUG_LOG_PRINT(fmt, ...)
#define MATRIX_KEYPAD_PR_LOG_PRINT(fmt, args...)
#else
static bool matrix_keypad_debug;
module_param(matrix_keypad_debug, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(matrix_keypad_debug, "matrix_keypad debug messages");

#define MATRIX_KEYPAD_ERR_LOG_PRINT(fmt, ...) printk(KERN_ERR "%s: " fmt, __func__, ##__VA_ARGS__)
#define MATRIX_KEYPAD_DEBUG_LOG_PRINT(fmt, ...) printk(KERN_NOTICE "%s: " fmt, __func__, ##__VA_ARGS__)
#define MATRIX_KEYPAD_PR_LOG_PRINT(fmt, args...)    do { if (matrix_keypad_debug)	     \
                                                         printk(KERN_DEBUG "%s: " fmt, __func__, ##args); \
                                                   } while(0)
#endif

#ifdef KCTP_CUST
static bool g_kdm_matrixkey_check = false;

static void *gkeyptr = NULL;

void matrix_keyptr_set_info(void *keyptr)
{
	gkeyptr = keyptr;
}
#endif /* KCTP_CUST */

#ifndef QUALCOMM_ORIGINAL_FEATURE
static int matrix_keypad_pinctrl_configure(struct matrix_keypad *keypad,
							bool active)
{
	struct pinctrl_state *set_state;
	int retval;

	if (active) {
		set_state =
			pinctrl_lookup_state(keypad->matrix_keypad_pinctrl,
						"matrix_keypad_active");
		if (IS_ERR(set_state)) {
			MATRIX_KEYPAD_ERR_LOG_PRINT("cannot get ts pinctrl active state\n");
			return PTR_ERR(set_state);
		}
	} else {
		set_state =
			pinctrl_lookup_state(keypad->matrix_keypad_pinctrl,
						"matrix_keypad_prescan");
		if (IS_ERR(set_state)) {
			MATRIX_KEYPAD_ERR_LOG_PRINT("cannot get gpiokey pinctrl prescan state\n");

			printk(KERN_ERR "%s: cannot get gpiokey pinctrl prescan state\n", __func__);
			return PTR_ERR(set_state);
		}
	}
	retval = pinctrl_select_state(keypad->matrix_keypad_pinctrl, set_state);
	if (retval) {
		MATRIX_KEYPAD_ERR_LOG_PRINT("cannot set ts pinctrl state\n");
		return retval;
	}

	return 0;
}
/*
 * NOTE: chnge from 8way to 4way
 *
*   exsample : change in the LeftUp
 *
 *		Push				->	send
 *		-----------------------------------
 *		LeftUp, Left, Up	->	Left, Up
 *		LeftUp, Left		->	Left
 *		LeftUp, Up			->	Up
 *		Left, Up			->	Left, Up
 *		Left				->	Left
 *		Up					->	Up
 */
static void matrix_keypad_8way_to_4way( u_int32_t *new_state, int *push_count )
{
	const struct keypad_8way_to_4way chg_tbl[4] = {
		/*	diagnal_row			, diagnal_col		, up_down_row		, up_down_col		, left_right_row	, left_right_col	*/
		{	KPD_LE_UP_KEY_ROW	, KPD_LE_UP_KEY_COL	, KPD_UP_KEY_ROW	, KPD_UP_KEY_COL	, KPD_LE_KEY_ROW	, KPD_LE_KEY_COL }, 
		{	KPD_LE_DO_KEY_ROW	, KPD_LE_DO_KEY_COL	, KPD_DO_KEY_ROW	, KPD_DO_KEY_COL	, KPD_LE_KEY_ROW	, KPD_LE_KEY_COL }, 
		{	KPD_RI_UP_KEY_ROW	, KPD_RI_UP_KEY_COL	, KPD_UP_KEY_ROW	, KPD_UP_KEY_COL	, KPD_RI_KEY_ROW	, KPD_RI_KEY_COL }, 
		{	KPD_RI_DO_KEY_ROW	, KPD_RI_DO_KEY_COL	, KPD_DO_KEY_ROW	, KPD_DO_KEY_COL	, KPD_RI_KEY_ROW	, KPD_RI_KEY_COL } 
	};
	unsigned char i = 0;

	for( i =0; i<4; i++ ) {
		
		if( new_state[chg_tbl[i].diagnal_col] &   ( 1 << chg_tbl[i].diagnal_row )) {
			new_state[chg_tbl[i].diagnal_col] &= ~( 1 << chg_tbl[i].diagnal_row );
			*push_count -= 1;
			if((( new_state[chg_tbl[i].up_do_col]	&  ( 1 << chg_tbl[i].up_do_row	)) == 0 ) &&
			   (( new_state[chg_tbl[i].le_ri_col]	&  ( 1 << chg_tbl[i].le_ri_row	)) == 0 )) {
			   	new_state[chg_tbl[i].up_do_col]	|= ( 1 << chg_tbl[i].up_do_row );
			   	new_state[chg_tbl[i].le_ri_col]	|= ( 1 << chg_tbl[i].le_ri_row );
				*push_count += 2;
			}
		}
	}
}

#endif /* Not QUALCOMM_ORIGINAL_FEATURE */

/*
 * NOTE: normally the GPIO has to be put into HiZ when de-activated to cause
 * minmal side effect when scanning other columns, here it is configured to
 * be input, and it should work on most platforms.
 */
static void __activate_col(const struct matrix_keypad_platform_data *pdata,
			   int col, bool on)
{
	bool level_on = !pdata->active_low;

	if (on) {
		gpio_direction_output(pdata->col_gpios[col], level_on);
	} else {
#ifdef QUALCOMM_ORIGINAL_FEATURE
		gpio_set_value_cansleep(pdata->col_gpios[col], !level_on);
#else
		gpio_direction_input(pdata->col_gpios[col]);
		gpio_direction_output(pdata->col_gpios[col], !level_on);
    	if (pdata->col_scan_delay_us)
    		udelay(pdata->col_scan_delay_us);
#endif
		gpio_direction_input(pdata->col_gpios[col]);
	}
}

static void activate_col(const struct matrix_keypad_platform_data *pdata,
			 int col, bool on)
{
	__activate_col(pdata, col, on);

	if (on && pdata->col_scan_delay_us)
		udelay(pdata->col_scan_delay_us);
}

static void activate_all_cols(const struct matrix_keypad_platform_data *pdata,
			      bool on)
{
	int col;

	for (col = 0; col < pdata->num_col_gpios; col++)
		__activate_col(pdata, col, on);
}

static bool row_asserted(const struct matrix_keypad_platform_data *pdata,
			 int row)
{
	return gpio_get_value_cansleep(pdata->row_gpios[row]) ?
			!pdata->active_low : pdata->active_low;
}

static void enable_row_irqs(struct matrix_keypad *keypad)
{
	const struct matrix_keypad_platform_data *pdata = keypad->pdata;
	int i;

	if (pdata->clustered_irq > 0)
		enable_irq(pdata->clustered_irq);
	else {
		for (i = 0; i < pdata->num_row_gpios; i++)
			enable_irq(gpio_to_irq(pdata->row_gpios[i]));
	}
}

static void disable_row_irqs(struct matrix_keypad *keypad)
{
	const struct matrix_keypad_platform_data *pdata = keypad->pdata;
	int i;

	if (pdata->clustered_irq > 0)
		disable_irq_nosync(pdata->clustered_irq);
	else {
		for (i = 0; i < pdata->num_row_gpios; i++)
			disable_irq_nosync(gpio_to_irq(pdata->row_gpios[i]));
	}
}

/*
 * This gets the keys from keyboard and reports it to input subsystem
 */
static int g_key_test = 0;
module_param_named(key_test, g_key_test, uint, 0644);

static void matrix_keypad_scan(struct work_struct *work)
{
	struct matrix_keypad *keypad =
		container_of(work, struct matrix_keypad, work.work);
	struct input_dev *input_dev = keypad->input_dev;
	const unsigned short *keycodes = input_dev->keycode;
	const struct matrix_keypad_platform_data *pdata = keypad->pdata;
	uint32_t new_state[MATRIX_MAX_COLS];
#ifdef QUALCOMM_ORIGINAL_FEATURE
	int row, col, code, count_state = 0;
#else
	int row, col, code = 0;
	int count = 0;
    struct irq_desc *desc;
    int gcnt[KEYPAD_GROUP_NUM], i;
    bool folder_state;
#endif /* Not QUALCOMM_ORIGINAL_FEATURE */

#ifdef KCTP_CUST
	bool fake_touch = false;
#endif /* KCTP_CUST */

    MATRIX_KEYPAD_PR_LOG_PRINT("start \n");

#ifndef QUALCOMM_ORIGINAL_FEATURE
    if(!keypad->scan_pending){
         keypad->scan_pending = true;
    }

    folder_state = gpio_keys_is_stateon(SW_LID);

    if(!folder_state){
    if(keypad->matrix_keypad_pinctrl){
        matrix_keypad_pinctrl_configure(keypad, false);
    }
    else{
        MATRIX_KEYPAD_ERR_LOG_PRINT("matrix_keypad_pinctrl 1st err %p\n", keypad->matrix_keypad_pinctrl);
    }

    for(i=0;i<KEYPAD_GROUP_NUM;i++)
        gcnt[i]=0;
#endif /* Not QUALCOMM_ORIGINAL_FEATURE */

	/* de-activate all columns for scanning */
	activate_all_cols(pdata, false);

	memset(new_state, 0, sizeof(new_state));

	/* assert each column and read the row status out */
	for (col = 0; col < pdata->num_col_gpios; col++) {
#ifdef QUALCOMM_ORIGINAL_FEATURE
		for (row = 0; row < pdata->num_row_gpios; row++) {
			activate_col(pdata, col, true);
			new_state[col] |=
				row_asserted(pdata, row) ? (1 << row) : 0;
			gpio_direction_output(pdata->col_gpios[col], 0);
			new_state[col] &=
				row_asserted(pdata, row) ? ~(1 << row) : ~(0);
		}
		if (new_state[col])
			count_state++;
		activate_col(pdata, col, false);
		for (row = 0; row < pdata->num_row_gpios; row++) {
			gpio_direction_output(pdata->row_gpios[row], 0);
			gpio_direction_input(pdata->row_gpios[row]);
		}
	}

	if (count_state == 5)
		goto out;
#else
		activate_col(pdata, col, true);

		for (row = 0; row < pdata->num_row_gpios; row++) {
			/* if registered with kecodes */
			if( keycodes[MATRIX_SCAN_CODE(row,col,keypad->row_shift)] != 0 ) {
				if( row_asserted(pdata, row) ){
					count++;
					gcnt[matrix_keypad_key_group[row][col]]++;
					new_state[col] |= (1 << row);
				}
			}
		}

		activate_col(pdata, col, false);
	}

	if( g_key_test != 1 )
	{
		matrix_keypad_8way_to_4way( new_state, &gcnt[4] );
	}
	if( (count != 0) && !(keypad->stopped) && !gpio_keys_is_stateon(SW_LID))
		__pm_stay_awake(&matrix_keypad_scan_wake_src);

	for (col = 0; col < pdata->num_col_gpios; col++) {
		for (row = 0; row < pdata->num_row_gpios; row++) {
			switch (matrix_keypad_key_group[row][col]) {
				case 0:
					if(gcnt[0] > 2) {
						if( keypad->last_key_state[col] & (1 << row) )
							new_state[col] |= (1 << row);
						else
							new_state[col] &= ~(1 << row);

						continue;
					}
					break;

				case 1:
					break;

				case 2:
					if(gcnt[2] > 2) {
						if( keypad->last_key_state[col] & (1 << row) )
							new_state[col] |= (1 << row);
						else
							new_state[col] &= ~(1 << row);

						continue;
					}
					break;

				case 3:
					break;

				case 4:
					if(gcnt[4] > 3) {
						if( keypad->last_key_state[col] & (1 << row) )
							new_state[col] |= (1 << row);
						else
							new_state[col] &= ~(1 << row);

						continue;
					}
					break;

				default:
					break;
			}
		}
	}
#endif /* Not QUALCOMM_ORIGINAL_FEATURE */

#ifdef KCTP_CUST
	keyptr_matrix_prepar(gkeyptr);
#endif /* KCTP_CUST */

	for (col = 0; col < pdata->num_col_gpios; col++) {
		uint32_t bits_changed;

		bits_changed = keypad->last_key_state[col] ^ new_state[col];
		if (bits_changed == 0)
			continue;

		for (row = 0; row < pdata->num_row_gpios; row++) {
			if ((bits_changed & (1 << row)) == 0)
				continue;

			code = MATRIX_SCAN_CODE(row, col, keypad->row_shift);
#ifdef QUALCOMM_ORIGINAL_FEATURE
			input_event(input_dev, EV_MSC, MSC_SCAN, code);
#endif /* QUALCOMM_ORIGINAL_FEATURE */
#ifndef QUALCOMM_ORIGINAL_FEATURE
            __pm_wakeup_event(&matrix_keypad_wake_src, MSEC_PER_SEC);

#ifdef KCTP_CUST
			fake_touch = keyptr_matrix_scan(gkeyptr, keycodes[code], new_state[col] & (1 << row));
#endif /* KCTP_CUST */

#endif /* Not QUALCOMM_ORIGINAL_FEATURE */
#ifdef KCTP_CUST
			if (!fake_touch) {
#endif /* KCTP_CUST */
			    input_report_key(input_dev,
						 keycodes[code],
						 new_state[col] & (1 << row));
				MATRIX_KEYPAD_DEBUG_LOG_PRINT("input_report_key code %d %d  \n", keycodes[code], new_state[col] & (1 << row));
#ifdef KCTP_CUST
	        }
#endif /* KCTP_CUST */
		}
	}

#ifdef KCTP_CUST
	if (fake_touch) {
		keyptr_input_report(gkeyptr);
	} else {
		if (!g_kdm_matrixkey_check) {
			input_sync(input_dev);
		}
	}
#else
	input_sync(input_dev);
#endif /* KCTP_CUST */

	memcpy(keypad->last_key_state, new_state, sizeof(new_state));

#ifdef QUALCOMM_ORIGINAL_FEATURE
out:
	activate_all_cols(pdata, true);
#else
    if(keypad->matrix_keypad_pinctrl){
        matrix_keypad_pinctrl_configure(keypad, true);
    }
    else{
        MATRIX_KEYPAD_ERR_LOG_PRINT("matrix_keypad_pinctrl 2st err %p\n", keypad->matrix_keypad_pinctrl);
    }
#endif
#ifndef QUALCOMM_ORIGINAL_FEATURE
	for (row = 0; row < pdata->num_row_gpios; row++) {
		for (col = 0; col < pdata->num_col_gpios; col++) {
			if ((keypad->last_key_state[col] & (1 << row))){
        		desc = irq_to_desc(gpio_to_irq(pdata->row_gpios[row]));
                desc->istate &= ~IRQS_PENDING;
                break;
            }
		}
	}
	}
#endif /* Not QUALCOMM_ORIGINAL_FEATURE */
	/* Enable IRQs again */
	spin_lock_irq(&keypad->lock);
#ifndef QUALCOMM_ORIGINAL_FEATURE
	if(count==0 || keypad->stopped || gpio_keys_is_stateon(SW_LID)){
#endif /* Not QUALCOMM_ORIGINAL_FEATURE */
	keypad->scan_pending = false;
	enable_row_irqs(keypad);
	__pm_relax(&matrix_keypad_scan_wake_src);
    MATRIX_KEYPAD_PR_LOG_PRINT("end cnt = %d stop = %d fd = %d \n", count, keypad->stopped, gpio_keys_is_stateon(SW_LID));
#ifndef QUALCOMM_ORIGINAL_FEATURE
	}else {
		schedule_delayed_work(&keypad->work,
			msecs_to_jiffies(keypad->pdata->debounce_ms));
        MATRIX_KEYPAD_PR_LOG_PRINT("polling  cnt = %d stop = %d fd = %d \n", count, keypad->stopped, gpio_keys_is_stateon(SW_LID));
	}
#endif /* Not QUALCOMM_ORIGINAL_FEATURE */
	spin_unlock_irq(&keypad->lock);
}

static irqreturn_t matrix_keypad_interrupt(int irq, void *id)
{
	struct matrix_keypad *keypad = id;
	unsigned long flags;

    MATRIX_KEYPAD_PR_LOG_PRINT(" %d\n", irq);
	spin_lock_irqsave(&keypad->lock, flags);

	/*
	 * See if another IRQ beaten us to it and scheduled the
	 * scan already. In that case we should not try to
	 * disable IRQs again.
	 */
#ifdef QUALCOMM_ORIGINAL_FEATURE
	if (unlikely(keypad->scan_pending || keypad->stopped))
		goto out;
#else
	if (unlikely(keypad->scan_pending || keypad->stopped)){
        MATRIX_KEYPAD_PR_LOG_PRINT(" %d pending \n", irq);
		goto out;
    }
#endif

	disable_row_irqs(keypad);
	keypad->scan_pending = true;
	schedule_delayed_work(&keypad->work,
		msecs_to_jiffies(keypad->pdata->debounce_ms));

out:
	spin_unlock_irqrestore(&keypad->lock, flags);
	return IRQ_HANDLED;
}

static int matrix_keypad_start(struct input_dev *dev)
{
	struct matrix_keypad *keypad = input_get_drvdata(dev);

	keypad->stopped = false;
	mb();

	/*
	 * Schedule an immediate key scan to capture current key state;
	 * columns will be activated and IRQs be enabled after the scan.
	 */
	schedule_delayed_work(&keypad->work, 0);

	return 0;
}

static void matrix_keypad_stop(struct input_dev *dev)
{
	struct matrix_keypad *keypad = input_get_drvdata(dev);

	spin_lock_irq(&keypad->lock);
	keypad->stopped = true;
	spin_unlock_irq(&keypad->lock);
#ifdef QUALCOMM_ORIGINAL_FEATURE
	flush_delayed_work(&keypad->work);
#else
	flush_delayed_work(&keypad->work);
	msleep(2 * keypad->pdata->debounce_ms);
#endif
	/*
	 * matrix_keypad_scan() will leave IRQs enabled;
	 * we should disable them now.
	 */
	disable_row_irqs(keypad);
}

#ifdef CONFIG_PM_SLEEP
static void matrix_keypad_enable_wakeup(struct matrix_keypad *keypad)
{
	const struct matrix_keypad_platform_data *pdata = keypad->pdata;
	unsigned int gpio;
	int i;

	if (pdata->clustered_irq > 0) {
		if (enable_irq_wake(pdata->clustered_irq) == 0)
			keypad->gpio_all_disabled = true;
	} else {

		for (i = 0; i < pdata->num_row_gpios; i++) {
			if (!test_bit(i, keypad->disabled_gpios)) {
				gpio = pdata->row_gpios[i];

				if (enable_irq_wake(gpio_to_irq(gpio)) == 0)
					__set_bit(i, keypad->disabled_gpios);
			}
		}
	}
}

static void matrix_keypad_disable_wakeup(struct matrix_keypad *keypad)
{
	const struct matrix_keypad_platform_data *pdata = keypad->pdata;
	unsigned int gpio;
	int i;

	if (pdata->clustered_irq > 0) {
		if (keypad->gpio_all_disabled) {
			disable_irq_wake(pdata->clustered_irq);
			keypad->gpio_all_disabled = false;
		}
	} else {
		for (i = 0; i < pdata->num_row_gpios; i++) {
			if (test_and_clear_bit(i, keypad->disabled_gpios)) {
				gpio = pdata->row_gpios[i];
				disable_irq_wake(gpio_to_irq(gpio));
			}
		}
	}
}

static int matrix_keypad_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct matrix_keypad *keypad = platform_get_drvdata(pdev);
#ifndef QUALCOMM_ORIGINAL_FEATURE
	int folder_status;
#endif /* Not QUALCOMM_ORIGINAL_FEATURE */


    MATRIX_KEYPAD_PR_LOG_PRINT("\n");

	matrix_keypad_stop(keypad->input_dev);

#ifdef QUALCOMM_ORIGINAL_FEATURE
	if (device_may_wakeup(&pdev->dev))
		matrix_keypad_enable_wakeup(keypad);
#else
	folder_status = gpio_keys_is_stateon(SW_LID);
    if (device_may_wakeup(&pdev->dev) && !folder_status){
		MATRIX_KEYPAD_DEBUG_LOG_PRINT("enable_wakeup %d\n", folder_status);

		matrix_keypad_enable_wakeup(keypad);
    }
    else{
		MATRIX_KEYPAD_DEBUG_LOG_PRINT(" Not enable_wakeup %d\n", folder_status);
    }
#endif

	return 0;
}

static int matrix_keypad_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct matrix_keypad *keypad = platform_get_drvdata(pdev);

    MATRIX_KEYPAD_PR_LOG_PRINT("\n");

	if (device_may_wakeup(&pdev->dev))
		matrix_keypad_disable_wakeup(keypad);

	matrix_keypad_start(keypad->input_dev);

	return 0;
}
#endif

static SIMPLE_DEV_PM_OPS(matrix_keypad_pm_ops,
			 matrix_keypad_suspend, matrix_keypad_resume);

static int matrix_keypad_init_gpio(struct platform_device *pdev,
				   struct matrix_keypad *keypad)
{
	const struct matrix_keypad_platform_data *pdata = keypad->pdata;
	int i, err;

	/* initialized strobe lines as outputs, activated */
	for (i = 0; i < pdata->num_col_gpios; i++) {
		err = gpio_request(pdata->col_gpios[i], "matrix_kbd_col");
		if (err) {
			dev_err(&pdev->dev,
				"failed to request GPIO%d for COL%d\n",
				pdata->col_gpios[i], i);
			goto err_free_cols;
		}

		gpio_direction_output(pdata->col_gpios[i], !pdata->active_low);
	}

	for (i = 0; i < pdata->num_row_gpios; i++) {
		err = gpio_request(pdata->row_gpios[i], "matrix_kbd_row");
		if (err) {
			dev_err(&pdev->dev,
				"failed to request GPIO%d for ROW%d\n",
				pdata->row_gpios[i], i);
			goto err_free_rows;
		}

		gpio_direction_input(pdata->row_gpios[i]);
	}

	if (pdata->clustered_irq > 0) {
		err = request_any_context_irq(pdata->clustered_irq,
				matrix_keypad_interrupt,
				pdata->clustered_irq_flags,
				"matrix-keypad", keypad);
		if (err < 0) {
			dev_err(&pdev->dev,
				"Unable to acquire clustered interrupt\n");
			goto err_free_rows;
		}
	} else {
		for (i = 0; i < pdata->num_row_gpios; i++) {
			err = request_any_context_irq(
					gpio_to_irq(pdata->row_gpios[i]),
					matrix_keypad_interrupt,
					IRQF_TRIGGER_RISING |
					IRQF_TRIGGER_FALLING,
					"matrix-keypad", keypad);
			if (err < 0) {
				dev_err(&pdev->dev,
					"Unable to acquire interrupt for GPIO line %i\n",
					pdata->row_gpios[i]);
				goto err_free_irqs;
			}
		}
	}

	/* initialized as disabled - enabled by input->open */
	disable_row_irqs(keypad);
	return 0;

err_free_irqs:
	while (--i >= 0)
		free_irq(gpio_to_irq(pdata->row_gpios[i]), keypad);
	i = pdata->num_row_gpios;
err_free_rows:
	while (--i >= 0)
		gpio_free(pdata->row_gpios[i]);
	i = pdata->num_col_gpios;
err_free_cols:
	while (--i >= 0)
		gpio_free(pdata->col_gpios[i]);

	return err;
}

static void matrix_keypad_free_gpio(struct matrix_keypad *keypad)
{
	const struct matrix_keypad_platform_data *pdata = keypad->pdata;
	int i;

	if (pdata->clustered_irq > 0) {
		free_irq(pdata->clustered_irq, keypad);
	} else {
		for (i = 0; i < pdata->num_row_gpios; i++)
			free_irq(gpio_to_irq(pdata->row_gpios[i]), keypad);
	}

	for (i = 0; i < pdata->num_row_gpios; i++)
		gpio_free(pdata->row_gpios[i]);

	for (i = 0; i < pdata->num_col_gpios; i++)
		gpio_free(pdata->col_gpios[i]);
}

#ifdef CONFIG_OF
static struct matrix_keypad_platform_data *
matrix_keypad_parse_dt(struct device *dev)
{
	struct matrix_keypad_platform_data *pdata;
	struct device_node *np = dev->of_node;
	unsigned int *gpios;
	int ret, i, nrow, ncol;

	if (!np) {
		dev_err(dev, "device lacks DT data\n");
		return ERR_PTR(-ENODEV);
	}

	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata) {
		dev_err(dev, "could not allocate memory for platform data\n");
		return ERR_PTR(-ENOMEM);
	}

	pdata->num_row_gpios = nrow = of_gpio_named_count(np, "row-gpios");
	pdata->num_col_gpios = ncol = of_gpio_named_count(np, "col-gpios");
	if (nrow <= 0 || ncol <= 0) {
		dev_err(dev, "number of keypad rows/columns not specified\n");
		return ERR_PTR(-EINVAL);
	}

	if (of_get_property(np, "linux,no-autorepeat", NULL))
		pdata->no_autorepeat = true;

	pdata->wakeup = of_property_read_bool(np, "wakeup-source") ||
			of_property_read_bool(np, "linux,wakeup"); /* legacy */

	if (of_get_property(np, "gpio-activelow", NULL))
		pdata->active_low = true;

	of_property_read_u32(np, "debounce-delay-ms", &pdata->debounce_ms);
	of_property_read_u32(np, "col-scan-delay-us",
						&pdata->col_scan_delay_us);

	gpios = devm_kzalloc(dev,
			     sizeof(unsigned int) *
				(pdata->num_row_gpios + pdata->num_col_gpios),
			     GFP_KERNEL);
	if (!gpios) {
		dev_err(dev, "could not allocate memory for gpios\n");
		return ERR_PTR(-ENOMEM);
	}

	for (i = 0; i < nrow; i++) {
		ret = of_get_named_gpio(np, "row-gpios", i);
		if (ret < 0)
			return ERR_PTR(ret);
		gpios[i] = ret;
	}

	for (i = 0; i < ncol; i++) {
		ret = of_get_named_gpio(np, "col-gpios", i);
		if (ret < 0)
			return ERR_PTR(ret);
		gpios[nrow + i] = ret;
	}

	pdata->row_gpios = gpios;
	pdata->col_gpios = &gpios[pdata->num_row_gpios];

	return pdata;
}
#else
static inline struct matrix_keypad_platform_data *
matrix_keypad_parse_dt(struct device *dev)
{
	dev_err(dev, "no platform data defined\n");

	return ERR_PTR(-EINVAL);
}
#endif

static int matrix_keypad_probe(struct platform_device *pdev)
{
	const struct matrix_keypad_platform_data *pdata;
	struct matrix_keypad *keypad;
	struct input_dev *input_dev;
	int err;

    MATRIX_KEYPAD_PR_LOG_PRINT("start\n");

#ifndef QUALCOMM_ORIGINAL_FEATURE
    wakeup_source_init(&matrix_keypad_wake_src, "matrix_keypad_wake_src");
    wakeup_source_init(&matrix_keypad_scan_wake_src, "matrix_keypad_scan_wake_src");
#endif /* Not QUALCOMM_ORIGINAL_FEATURE */

	pdata = dev_get_platdata(&pdev->dev);
	if (!pdata) {
		pdata = matrix_keypad_parse_dt(&pdev->dev);
		if (IS_ERR(pdata)) {
			dev_err(&pdev->dev, "no platform data defined\n");
			return PTR_ERR(pdata);
		}
	} else if (!pdata->keymap_data) {
		dev_err(&pdev->dev, "no keymap data defined\n");
		return -EINVAL;
	}

	keypad = kzalloc(sizeof(struct matrix_keypad), GFP_KERNEL);
	input_dev = input_allocate_device();
	if (!keypad || !input_dev) {
		err = -ENOMEM;
		goto err_free_mem;
	}


#ifndef QUALCOMM_ORIGINAL_FEATURE
	/* Get pinctrl if target uses pinctrl */
	keypad->matrix_keypad_pinctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(keypad->matrix_keypad_pinctrl)) {
		if (PTR_ERR(keypad->matrix_keypad_pinctrl) == -EPROBE_DEFER)
			return -EPROBE_DEFER;

		MATRIX_KEYPAD_ERR_LOG_PRINT("Target does not use pinctrl\n");
		keypad->matrix_keypad_pinctrl = NULL;
	}

	if (keypad->matrix_keypad_pinctrl) {
		err = matrix_keypad_pinctrl_configure(keypad, true);
		if (err) {
			MATRIX_KEYPAD_ERR_LOG_PRINT("cannot set ts pinctrl active state\n");
			goto err_free_gpio;
		}
	}
#endif /* Not QUALCOMM_ORIGINAL_FEATURE */

	keypad->input_dev = input_dev;
	keypad->pdata = pdata;
	keypad->row_shift = get_count_order(pdata->num_col_gpios);
	keypad->stopped = true;
	INIT_DELAYED_WORK(&keypad->work, matrix_keypad_scan);
	spin_lock_init(&keypad->lock);

#ifdef QUALCOMM_ORIGINAL_FEATURE
	input_dev->name		= pdev->name;
#else
	input_dev->name		= "matrix_keypad";
#endif

	input_dev->id.bustype	= BUS_HOST;
	input_dev->dev.parent	= &pdev->dev;
	input_dev->open		= matrix_keypad_start;
	input_dev->close	= matrix_keypad_stop;

	err = matrix_keypad_build_keymap(pdata->keymap_data, NULL,
					 pdata->num_row_gpios,
					 pdata->num_col_gpios,
					 NULL, input_dev);
	if (err) {
		dev_err(&pdev->dev, "failed to build keymap\n");
		goto err_free_mem;
	}

	if (!pdata->no_autorepeat)
		__set_bit(EV_REP, input_dev->evbit);
#ifdef QUALCOMM_ORIGINAL_FEATURE
	input_set_capability(input_dev, EV_MSC, MSC_SCAN);
#endif /* QUALCOMM_ORIGINAL_FEATURE */
	input_set_drvdata(input_dev, keypad);

	err = matrix_keypad_init_gpio(pdev, keypad);
	if (err)
		goto err_free_mem;

	err = input_register_device(keypad->input_dev);
	if (err)
		goto err_free_gpio;

	device_init_wakeup(&pdev->dev, pdata->wakeup);
	platform_set_drvdata(pdev, keypad);

    MATRIX_KEYPAD_PR_LOG_PRINT("end\n");
	return 0;

err_free_gpio:
	matrix_keypad_free_gpio(keypad);
err_free_mem:
	input_free_device(input_dev);
	kfree(keypad);
	return err;
}

static int matrix_keypad_remove(struct platform_device *pdev)
{
	struct matrix_keypad *keypad = platform_get_drvdata(pdev);

	device_init_wakeup(&pdev->dev, 0);

#ifndef QUALCOMM_ORIGINAL_FEATURE
    wakeup_source_trash(&matrix_keypad_wake_src);
    wakeup_source_trash(&matrix_keypad_scan_wake_src);
#endif /* Not QUALCOMM_ORIGINAL_FEATURE */

	matrix_keypad_free_gpio(keypad);
	input_unregister_device(keypad->input_dev);
	kfree(keypad);

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id matrix_keypad_dt_match[] = {
	{ .compatible = "gpio-matrix-keypad" },
	{ }
};
MODULE_DEVICE_TABLE(of, matrix_keypad_dt_match);
#endif

static struct platform_driver matrix_keypad_driver = {
	.probe		= matrix_keypad_probe,
	.remove		= matrix_keypad_remove,
	.driver		= {
		.name	= "matrix-keypad",
		.pm	= &matrix_keypad_pm_ops,
		.of_match_table = of_match_ptr(matrix_keypad_dt_match),
	},
};
module_platform_driver(matrix_keypad_driver);

MODULE_AUTHOR("Marek Vasut <marek.vasut@gmail.com>");
MODULE_DESCRIPTION("GPIO Driven Matrix Keypad Driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:matrix-keypad");

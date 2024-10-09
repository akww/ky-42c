/*
 * This software is contributed or developed by KYOCERA Corporation.
 * (C) 2013 KYOCERA Corporation
 * (C) 2015 KYOCERA Corporation
 * (C) 2017 KYOCERA Corporation
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

#ifndef _STK3338_PM_H_
#define _STK3338_PM_H_

/* module name of tuner pm driver */
#define STK3338_PM_DRIVER_NAME      "stk3338_pm"

/* stk3338 : power on */
#define STK3338_POWER_ON       1

/* stk3338 : power off */
#define STK3338_POWER_OFF      0

struct stk3338_pm_platform_data {
       int gpio_pwr;
};

#endif /* _STK3338_PM_H_ */

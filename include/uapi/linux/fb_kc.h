/*
 * This software is contributed or developed by KYOCERA Corporation.
 * (C) 2022 KYOCERA Corporation
*/
#ifndef _UAPI_LINUX_FB_KC_H
#define _UAPI_LINUX_FB_KC_H

//#include <linux/types.h>
//#include <linux/i2c.h>
/* sub display info */

#define FBIOPUT_DIAGVALID	0x4621
#define FBIOPUT_DIAGINVALID	0x4622
#define FBIOPUT_DIAGSUBVALID    0x4623
#define FBIOPUT_DIAGSUBINVALID  0x4624
#define FBIOGET_SUBBUFFER   0x4625

struct fb_var_subdispinfo {
	__u8 user_contrast;
	__u8 elec_vol_init;
	__u8 elec_vol_temp[18];
	__u8 elec_vol_user[5];
	__u8 langage;
	__u8 watch_dispinfo;
	__u8 clock_dispinfo;
	__s32 timezone;
	__u8 reserve[16];
};

#if defined (CONFIG_DISP_EXT_SUB_ALWAYS_ON) || defined(CONFIG_DISP_EXT_SUB_CONTRAST)
/* FB_SUB_USERCONT_TYPE_* */
#define FB_SUB_USERCONT_TYPE_1     0
#define FB_SUB_USERCONT_TYPE_2     1
#define FB_SUB_USERCONT_TYPE_3     2
#define FB_SUB_USERCONT_TYPE_4     3
#define FB_SUB_USERCONT_TYPE_5     4
#define FB_SUB_USERCONT_TYPE_NONE  0xFF
/* FB_SUB_LANGUAGE_TYPE_* */
#define FB_SUB_LANGUAGE_TYPE_JP    0
#define FB_SUB_LANGUAGE_TYPE_EN    1
#define FB_SUB_LANGUAGE_TYPE_NONE  0xFF
/* FB_SUB_WATCH_TYPE* */
#define FB_SUB_WATCH_TYPE_DIGITAL  0
#define FB_SUB_WATCH_TYPE_ANALOG   1
#define FB_SUB_WATCH_TYPE_NONE     0xFF
/* FB_SUB_CLOCK_TYPE_* */
#define FB_SUB_CLOCK_TYPE_24H      0
#define FB_SUB_CLOCK_TYPE_12H      1
#define FB_SUB_CLOCK_TYPE_NONE     0xFF

struct fb_kcjprop_subdisp_data {
	__u8 rw_sublcd_vcnt_init_i;
	__u8 rw_sublcd_vcnt_off_vs_temp_i[18];
	__u8 rw_sublcd_vcnt_user_i[5];
};
#endif /* CONFIG_DISP_EXT_SUB_ALWAYS_ON || CONFIG_DISP_EXT_SUB_CONTRAST */


struct disp_ext_sub_write_cmd {
	uint8_t len;
	uint8_t data[255];
};


#define KC_DISP_EXT_SUB_IOCTL_MAGIC 's'
#define KC_DISP_EXT_SUB_WRITE_CMD  _IOWR(KC_DISP_EXT_SUB_IOCTL_MAGIC, 100, \
					struct disp_ext_sub_write_cmd)
#define KC_DISP_EXT_SUB_STATUS_CMD  _IOWR(KC_DISP_EXT_SUB_IOCTL_MAGIC, 101, int)
#define KC_DISP_EXT_SUB_WRITE_DATA  _IOWR(KC_DISP_EXT_SUB_IOCTL_MAGIC, 105, \
					struct disp_ext_sub_write_cmd)
#define KC_DISP_EXT_SUB_GET_BATTERY_TEMP  _IOWR(KC_DISP_EXT_SUB_IOCTL_MAGIC, 107, int)
#define KC_DISP_EXT_SUB_MODEM_DRAW_CMD  _IOWR(KC_DISP_EXT_SUB_IOCTL_MAGIC, 102, int)
#define FBIOEXT_SETSUBDISPINFO  _IOWR(KC_DISP_EXT_SUB_IOCTL_MAGIC, 103, int)
#define KC_DISP_EXT_SUB_MODEM_CMD  _IOWR(KC_DISP_EXT_SUB_IOCTL_MAGIC, 104, int)
#define KC_DISP_EXT_SUB_SET_CONTRAST  _IOWR(KC_DISP_EXT_SUB_IOCTL_MAGIC, 106, int)


#endif /* _UAPI_LINUX_FB_KC_H */

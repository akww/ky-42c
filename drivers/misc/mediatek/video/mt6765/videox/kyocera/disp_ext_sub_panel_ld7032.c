/*
 * This software is contributed or developed by KYOCERA Corporation.
 * (C) 2021 KYOCERA Corporation
 * (C) 2022 KYOCERA Corporation
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
#include <linux/of_device.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/fb.h>
#include <linux/fb_kc.h>
#include <linux/regulator/consumer.h>
#include <linux/power_supply.h>

//#include "../mdss_fb.h"
//#include "../mdss_dsi.h"
#include "disp_ext_sub_ctrl.h"
//#include "disp_ext_i2c.h"

#define DISP_EXT_SUB_SIG_P_LEN 2
#define DISP_EXT_SUB_FORAMT_MASK 0x0010
#define DISP_EXT_SUB_FORAMT_SHIFT 4
#define DISP_EXT_SUB_POWER_OFF_WAIT 900000L

static void disp_ext_sub_conv_img(
	struct fb_var_screeninfo *var,
	uint8_t* src_p,
	uint8_t* dst_p );
static int disp_ext_sub_send_img(
	struct disp_ext_sub_pdata *pdata,
	uint8_t* img_p );
static int disp_ext_sub_set_img_data(
	struct disp_ext_sub_pdata *pdata,
	uint8_t* img_p );

static int disp_ext_sub_exe_seq(
	struct disp_ext_sub_pdata *pdata,
	struct disp_ext_sub_cmds *sub_cmds_p );
static int disp_ext_sub_set_wait( void * payload_p, unsigned char payload_len );
static int disp_ext_sub_set_sig(
	void * payload_p,
	unsigned char payload_len,
	struct disp_ext_sub_pdata *pdata );
static void disp_ext_sub_set_gpio( int gpio_handle, int on );
static int disp_ext_sub_panel_enable_vreg(
	struct disp_ext_sub_pdata *pdata,
	int enable);

static int disp_ext_sub_get_cmd_dt(
	struct device_node *np,
	struct disp_ext_sub_cmds *cmd_p,
	char *cmd_key
);

ktime_t disp_ext_last_disp_off_ktime;

/*********************************************************************************/

#define I2C_RETRY_TIMES 2

static struct i2c_client *disp_ext_i2c_client;

void disp_ext_i2c_clk_enable(void)
{
//	int ret;

	pr_debug("%s start\n",__func__);

#if 0 //test
	if (!disp_ext_i2c_client) {
		pr_err("%s : disp_ext_i2c_client NULL!\n",__func__);
		return;
	}

	ret = i2c_clk_prepare_enable(disp_ext_i2c_client->adapter);
	if (ret) {
		pr_err("%s : i2c_clk_prepare_enable error:%d\n",__func__, ret);
		return;
	}
#endif

	pr_debug("%s end\n",__func__);
}

void disp_ext_i2c_clk_disable(void)
{
	pr_debug("%s start\n",__func__);

#if 0 //test
	if (!disp_ext_i2c_client) {
		pr_err("%s : disp_ext_i2c_client NULL!\n",__func__);
		return;
	}

	i2c_clk_disable_unprepare(disp_ext_i2c_client->adapter);
#endif

	pr_debug("%s end\n",__func__);
}


int32_t disp_ext_i2c_write(uint8_t *data_p, uint32_t size)
{
	int32_t ret = 0;
	struct i2c_msg msg;
	int retry;

	pr_debug("%s start size=%d\n",__func__,size);
	if (!disp_ext_i2c_client) {
		pr_err("%s : disp_ext_i2c_client NULL!\n",__func__);
		return -ENODEV;
	}
	if (!data_p){
		pr_err("%s : data NULL!\n",__func__);
		return -ENODEV;
	}
	if (!size){
		pr_err("%s : data size zero!\n",__func__);
		return -ENODEV;
	}

	disp_ext_sub_seq_log(data_p, size);

	msg.addr = disp_ext_i2c_client->addr;
	msg.flags = 0;
	msg.len = size;
	msg.buf = data_p;

	for (retry = 0; retry < I2C_RETRY_TIMES; retry++) {
		ret = i2c_transfer(disp_ext_i2c_client->adapter, &msg, 1);
		if (ret == 1)
			break;
		pr_err("i2c_transfer ret:%d, retry after 20ms\n", ret);
		msleep(20);
	}

	if (ret == 1) {
		ret = 0;
	} else {
		pr_err("i2c_transfer error\n");
		ret = -1;
	}

	pr_debug("%s end ret=%d\n",__func__, ret);
	return ret;
}

static int32_t disp_ext_i2c_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	pr_debug("%s start\n",__func__);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("i2c_check_functionality err\n");
		return -ENODEV;
	}

	disp_ext_i2c_client = client;

	pr_debug("%s end\n",__func__);

	return 0;
}

static int disp_ext_i2c_remove(struct i2c_client *client)
{
	pr_debug("%s start\n",__func__);

	pr_debug("%s end\n",__func__);

	return 0;
}

#define DISP_EXT_I2C_NAME "ld7032"
#define DISP_EXT_I2C_DTS_NAME "ldt,ld7032"

static const struct i2c_device_id ld7032_id[] = {
	{ DISP_EXT_I2C_NAME, 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, ld7032_id);

static struct of_device_id ld7032_match_table[] = {
	{ .compatible = DISP_EXT_I2C_DTS_NAME,},
	{ },
};

static struct i2c_driver ld7032_driver = {
	.driver = {
		.name   = DISP_EXT_I2C_NAME,
		.owner  = THIS_MODULE,
		.of_match_table = ld7032_match_table,
	},
	.probe  = disp_ext_i2c_probe,
	.remove = disp_ext_i2c_remove,
	.id_table = ld7032_id,
};

int disp_ext_i2c_init( void )
{
	int32_t rc = 0;

	pr_debug("%s start\n",__func__);

	rc = i2c_add_driver(&ld7032_driver);
	if (rc != 0) {
		pr_err("can't add i2c driver\n");
		rc = -ENOTSUPP;
	}

	pr_debug("%s end\n",__func__);

	return 0; //rc;
}

static void __exit disp_ext_i2c_exit(void)
{
	pr_debug("%s start\n",__func__);

	i2c_del_driver(&ld7032_driver);

	pr_debug("%s end\n",__func__);
}

module_exit(disp_ext_i2c_exit);

/*********************************************************************************/

static int ld7032_disp_ext_sub_panel_set_status(disp_ext_sub_state_type next_status, struct disp_ext_sub_pdata *pdata);
static int ld7032_disp_ext_sub_panel_update(struct fb_var_screeninfo *var, struct fb_info *info, uint8_t* apps_img_p);
static int ld7032_disp_ext_sub_set_cmd(void * payload_p, unsigned char payload_len, struct disp_ext_sub_pdata *pdata);
static int ld7032_disp_ext_sub_panel_bus_init(struct disp_ext_sub_pdata *pdata);
static int ld7032_disp_ext_sub_panel_signal_init(struct disp_ext_sub_pdata *pdata);
static int ld7032_disp_ext_sub_get_panel_dt(struct device_node * np, struct disp_ext_sub_pdata *pdata);
static int ld7032_disp_ext_sub_get_signal_dt(struct device_node * np, struct disp_ext_sub_pdata *pdata);
static int ld7032_disp_ext_sub_get_seq_dt(struct device_node * np, struct disp_ext_sub_pdata *pdata);
static int ld7032_disp_ext_sub_get_vreg_dt(struct device *dev, struct disp_ext_sub_pdata *pdata);

static int ld7032_disp_ext_sub_panel_set_status(
	disp_ext_sub_state_type next_status,
	struct disp_ext_sub_pdata *pdata )
{
	int ret = 0;
	struct disp_ext_sub_cmds *sub_cmds_p = NULL;
	s64 poff_wait;
	s64 disp_off_time;
	s64 shutdown_time;
	s64 delta;

	pr_debug("%s start\n",__func__);

	if(!pdata){
		pr_err("%s end - null pdata[%x]\n",__func__,(int)pdata);
		return -ENODEV;
	}

	if ( pdata->state == next_status ) {
		pr_debug("%s end\n",__func__);
		return 0;
	}

	switch (next_status) {
	case DISP_SUB_STATE_OFF:
		pr_debug("%s: OFF seq start\n",__func__);
		if (pdata->state == DISP_SUB_STATE_PWR_OFF) {
			pr_err("%s end - bad state:PWR_OFF->OFF\n",__func__);
			return -EPERM;
		}
		if (pdata->state == DISP_SUB_STATE_PWR_ON) {
			pr_debug("%s end - state:PWR_ON->OFF\n",__func__);
			return 0;
		}
		sub_cmds_p = &(pdata->off_cmd);
		ret = disp_ext_sub_exe_seq( pdata, sub_cmds_p );
		disp_ext_i2c_clk_disable();
		disp_ext_sub_panel_enable_vreg(pdata, 0);
		disp_ext_last_disp_off_ktime = ktime_get();
		pr_debug("%s: OFF seq end\n",__func__);
		break;
	case DISP_SUB_STATE_ON:
		pr_debug("%s: ON seq start\n",__func__);
		if (pdata->state == DISP_SUB_STATE_PWR_OFF) {
			pr_err("%s end - bad state:PWR_OFF->ON\n",__func__);
			return -EPERM;
		}
		disp_ext_sub_panel_enable_vreg(pdata, 1);
		disp_ext_i2c_clk_enable();
		sub_cmds_p = &(pdata->on_cmd);
		ret = disp_ext_sub_exe_seq( pdata, sub_cmds_p );
		pdata->first_update = true;
		pr_debug("%s: ON seq end\n",__func__);

		if(ret == -1){
			sub_cmds_p = &(pdata->off_cmd);
			disp_ext_sub_exe_seq( pdata, sub_cmds_p );
			disp_ext_sub_panel_enable_vreg(pdata, 0);
			pr_err("%s: ERROR OFF seq end\n",__func__);
		}
		break;
	case DISP_SUB_STATE_PWR_OFF:
		pr_debug("%s: PWR_OFF seq start\n",__func__);
		if ( pdata->state == DISP_SUB_STATE_ON ) {
			sub_cmds_p = &(pdata->off_cmd);
			ret = disp_ext_sub_exe_seq( pdata, sub_cmds_p );
			disp_ext_i2c_clk_disable();
			disp_ext_sub_panel_enable_vreg(pdata, 0);
			disp_ext_last_disp_off_ktime = ktime_get();
			pr_debug("%s: Display OFF\n",__func__);
		}
		sub_cmds_p = &(pdata->pwroff_cmd);
		ret = disp_ext_sub_exe_seq( pdata, sub_cmds_p );

		poff_wait = 0;
		disp_off_time = ktime_to_us(disp_ext_last_disp_off_ktime);
		shutdown_time = ktime_to_us(ktime_get());
		if (shutdown_time > disp_off_time) {
			delta = shutdown_time - disp_off_time;
			pr_debug("%s: (shutdown_time > disp_off_time) delta=%lld\n", __func__, delta);
		} else {
			delta = disp_off_time - shutdown_time;
			pr_debug("%s: (shutdown_time <= disp_off_time) delta=%lld\n", __func__, delta);
		}

		if (delta < DISP_EXT_SUB_POWER_OFF_WAIT) {
			poff_wait = DISP_EXT_SUB_POWER_OFF_WAIT - delta;
			pr_debug("%s: (delta < DISP_EXT_SUB_POWER_OFF_WAIT) poff_wait=%lld\n", __func__, poff_wait);
		}
		if(poff_wait > 0)
			usleep_range( poff_wait, poff_wait );
		pr_debug("%s: PWR_OFF seq end\n",__func__);
		break;
	case DISP_SUB_STATE_PWR_ON:
		pr_debug("%s: PWR_ON seq start\n",__func__);
		if (pdata->state != DISP_SUB_STATE_PWR_OFF) {
			pr_debug("%s end - state:!PWR_OFF->PWR_ON\n",__func__);
			return 0;
		}
		sub_cmds_p = &(pdata->pwron_cmd);
		ret = disp_ext_sub_exe_seq( pdata, sub_cmds_p );
		disp_ext_last_disp_off_ktime = ktime_get();
		pr_debug("%s: PWR_ON seq end\n",__func__);
		break;
	default:
		pr_err("%s : undefine state[%x]\n",__func__,(int)next_status);
		ret = -1;
		break;
	}

	if (!ret) {
		pdata->state = next_status;
	}
	pr_debug("%s end\n",__func__);
	return ret;
}

static int ld7032_disp_ext_sub_panel_update(
	struct fb_var_screeninfo *var,
	struct fb_info *info,
	uint8_t* apps_img_p )
{
	int ret = 0;
	uint8_t* send_img_p;
	uint32_t size;
	struct disp_ext_sub_info* sub_info_p;
	struct disp_ext_sub_pdata* pdata;
	int skip_dispon = 0;

	pr_debug("%s start\n",__func__);

	if( (!var) || (!info) || (!apps_img_p) ){
		pr_err("%s end - bad parm: var[%x] info[%x] img[%x]\n",
				__func__,(int)var,(int)info,(int)apps_img_p);
		return -ENODEV;
	}

	sub_info_p = (struct disp_ext_sub_info*)info->par;
	pdata = &(sub_info_p->pdata);
	if (pdata->state != DISP_SUB_STATE_ON) {
		pr_err("%s end - bad state:%d\n", __func__, pdata->state);
		return -EPERM;
	}

	size = (var->xres) * (var->yres) / 8;
	send_img_p = (uint8_t*)kmalloc( size, GFP_KERNEL );
	if( !send_img_p ){
		pr_err("%s : fail malloc\n",__func__ );
		ret = -ENOMEM;
		goto exit;
	}

	disp_ext_sub_img_dump((void*)apps_img_p,((var->xres) * (var->yres)*2),2);
	disp_ext_sub_conv_img( var, apps_img_p, send_img_p );
	disp_ext_sub_img_dump((void*)send_img_p,((var->xres) * (var->yres)),1);

	ret = disp_ext_sub_send_img( pdata, send_img_p );
	if (ret) {
		pr_err("%s: disp_ext_sub_send_img error:%d, do recovery\n",
			__func__, ret);
		ret = disp_ext_sub_exe_seq(pdata, &pdata->on_cmd);
		pdata->first_update = true;
		ret = disp_ext_sub_send_img(pdata, send_img_p);
		if (ret) {
			pr_err("%s : error send image skip\n",__func__ );
			ret = disp_ext_sub_exe_seq(pdata, &pdata->on_cmd);
			skip_dispon = 1;
		}
	}
	kfree( send_img_p );

	if ((pdata->first_update)&&(!skip_dispon)) {
		ret = disp_ext_sub_exe_seq(pdata, &pdata->post_on_cmd);
		if (ret) {
			pr_err("%s : Display ON command fail.\n",__func__ );
			goto exit;
		}
		pr_debug("%s Display ON command send for first time update.\n",__func__);
		pdata->first_update = false;
	}

exit:
	pr_debug("%s end\n",__func__);
	return ret;
}

static void disp_ext_sub_conv_img(
	struct fb_var_screeninfo *var,
	uint8_t* src_p,
	uint8_t* dst_p )
{
	uint16_t src_one_pix_data = 0;
	uint8_t  dst_one_pix_data = 0;
	uint16_t* src_tmp_p;
	uint8_t dst_tmp = 0;
	uint32_t size;
	uint32_t src_cnt;
	int i,j;

	pr_debug("%s start\n",__func__);

	src_tmp_p = (uint16_t*)src_p;
	size = (var->xres) * (var->yres) / 8;
	src_cnt = 0;

	for(i=0; i<size; i++){
		for(j=0; j<8; j++){
			src_one_pix_data = src_tmp_p[src_cnt++];
			dst_one_pix_data = (src_one_pix_data & DISP_EXT_SUB_FORAMT_MASK)
								>> DISP_EXT_SUB_FORAMT_SHIFT;
			dst_tmp = dst_tmp | ( dst_one_pix_data << (7-j) );
		}
		dst_p[i] = dst_tmp;
		dst_tmp = 0;
	}
	pr_debug("%s end\n",__func__);
	return;
}

static int disp_ext_sub_send_img(
	struct disp_ext_sub_pdata *pdata,
	uint8_t* img_p )
{
	int ret;
	struct disp_ext_sub_cmds *sub_cmds_p = NULL;

	pr_debug("%s start\n",__func__);

	if( (!pdata) || (!img_p) ){
		pr_err("%s end - bad parm: pdata[%x] img[%x]\n",
				__func__,(int)pdata,(int)img_p);
		return -ENODEV;
	}

	sub_cmds_p = &(pdata->ram_wr_cmd);
	ret = disp_ext_sub_exe_seq( pdata, sub_cmds_p );
	ret = disp_ext_sub_set_img_data( pdata, img_p );

	pr_debug("%s end\n",__func__);
	return ret;
}

static int disp_ext_sub_exe_seq(
	struct disp_ext_sub_pdata *pdata,
	struct disp_ext_sub_cmds *sub_cmds_p )
{
	int ret = 0;
	unsigned int cmd_cnt;
	unsigned char ctrl_kind;
	unsigned char payload_len;
	void * payload_p;
	int i;

	pr_debug("%s start\n",__func__);

	if(!sub_cmds_p){
		pr_err("%s end - null cmd data[%x]\n",__func__,(int)sub_cmds_p);
		return -ENODEV;
	}

	cmd_cnt = sub_cmds_p->cmd_cnt;

	for(i=0; i<cmd_cnt; i++){
		ctrl_kind = sub_cmds_p->cmd_p[i].cmd_hdr.ctrl_kind;
		payload_len = sub_cmds_p->cmd_p[i].cmd_hdr.payload_len;
		payload_p = sub_cmds_p->cmd_p[i].payload_p;

		switch (ctrl_kind) {
		case DISP_SUB_CTRL_CMD:
			if(ret != -1)
			{
				ret = ld7032_disp_ext_sub_set_cmd( payload_p, payload_len, pdata );
			}
			else
			{
				pr_err("%s cmd error detect.skip cmd \n",__func__);
			}

			break;
		case DISP_SUB_CTRL_WAIT:
			ret = disp_ext_sub_set_wait( payload_p, payload_len );
			break;
		case DISP_SUB_CTRL_SIG:
			ret = disp_ext_sub_set_sig( payload_p, payload_len, pdata );
			break;
		default:
			break;
		}
	}

	pr_debug("%s end\n",__func__);
	return ret;
}

static int disp_ext_sub_set_img_data(
	struct disp_ext_sub_pdata *pdata,
	uint8_t* img_p )
{
	int ret = 0;
	uint8_t* send_data_p;
	uint32_t send_size;

	pr_debug("%s start\n",__func__);

	if( (!pdata) || (!img_p) ){
		pr_err("%s end - bad parm: pdata[%x] img[%x]\n",
				__func__,(int)pdata,(int)img_p);
		return -ENODEV;
	}

	send_size = 1 + (pdata->xres / 8) * pdata->yres;
	send_data_p = kmalloc(send_size, GFP_KERNEL);
	if (!send_data_p)
		return -ENOMEM;
	send_data_p[0] = 0x08;
	memcpy(&send_data_p[1], img_p, send_size);

	ret = disp_ext_i2c_write(send_data_p, send_size);

	kfree(send_data_p);

	pr_debug("%s end\n",__func__);
	return ret;
}

static int ld7032_disp_ext_sub_set_cmd(
	void * payload_p,
	unsigned char payload_len,
	struct disp_ext_sub_pdata *pdata )
{
	int ret = 0;

	pr_debug("%s start\n",__func__);

	if( (!payload_p) || (!pdata) ){
		pr_err("%s end - null payload[%x] pdata[%d]\n",
				__func__,(int)payload_p,(int)pdata);
		return -ENODEV;
	}
	if(!payload_len){
		pr_err("%s end - bad length :payload_len[%x]\n",__func__,(int)payload_len);
		return -ENODEV;
	}

	ret = disp_ext_i2c_write(payload_p, payload_len);

	pr_debug("%s end\n",__func__);
	return ret;
}

static int disp_ext_sub_set_wait( void * payload_p, unsigned char payload_len )
{
	int ret = 0;
	unsigned char *data_p;
	unsigned char offset;
	unsigned long us_time = 0;
	int i;

	pr_debug("%s start\n",__func__);

	if( !payload_p ){
		pr_err("%s end - null payload[%x]\n",__func__,(int)payload_p );
		return -ENODEV;
	}
	if(!payload_len){
		pr_err("%s end - bad length :payload_len[%x]\n",__func__,(int)payload_len);
		return -ENODEV;
	}

	data_p = (unsigned char*)payload_p;
	offset = payload_len;

	for( i=0; i<payload_len; i++ ){
		offset--;
		us_time |= data_p[i]<<(offset*8);
	}

	pr_debug("%s: wait [%d]us\n",__func__,(int)us_time);
	usleep_range( us_time, us_time );

	pr_debug("%s end\n",__func__);
	return ret;
}

static int disp_ext_sub_set_sig(
	void * payload_p,
	unsigned char payload_len,
	struct disp_ext_sub_pdata *pdata )
{
	int ret = 0;
	unsigned char *data_p;
	unsigned char sig_kind;
	int on;
	int gpio_handle = 0;

	pr_debug("%s start\n",__func__);

	if( (!payload_p) || (!pdata) ){
		pr_err("%s end - null payload[%x] pdata[%d]\n",
				__func__,(int)payload_p,(int)pdata);
		return -ENODEV;
	}
	if(DISP_EXT_SUB_SIG_P_LEN != payload_len){
		pr_err("%s end - bad length :payload_len[%x]\n",__func__,(int)payload_len);
		return -ENODEV;
	}

	data_p = (unsigned char*)payload_p;
	sig_kind = data_p[0];
	on = (int)data_p[1];

	switch (sig_kind) {
	case DISP_SUB_SIG_VH:
		gpio_handle = pdata->vh_gpio;
		pr_debug("%s: set VH\n",__func__);
		break;
	case DISP_SUB_SIG_RESET:
		gpio_handle = pdata->rst_gpio;
		pr_debug("%s: set Reset\n",__func__);
		break;
	case DISP_SUB_SIG_VOLED:
		gpio_handle = pdata->voled_gpio;
		pr_debug("%s: set VOLED\n",__func__);
		break;
	default:
		pr_err("%s : undefine sig[%x]\n",__func__,(int)sig_kind);
		break;
	}

	if( gpio_handle ){
		disp_ext_sub_set_gpio( gpio_handle, on );
		if (sig_kind == DISP_SUB_SIG_VH && !on) {
			disp_ext_last_disp_off_ktime = ktime_get();
		}
	} else {
		ret = -1;
	}

	pr_debug("%s end\n",__func__);
	return ret;
}

static void disp_ext_sub_set_gpio( int gpio_handle, int on )
{
	pr_debug("%s gpio[%d] -> %d\n",__func__, gpio_handle, on);
	gpio_set_value( gpio_handle, on );
	return;
}

static int disp_ext_sub_panel_enable_vreg(
	struct disp_ext_sub_pdata *pdata,
	int enable)
{
	int rc = 0;

	(void)pdata;
	(void)enable;

	return rc;
}

static int ld7032_disp_ext_sub_panel_bus_init( struct disp_ext_sub_pdata *pdata )
{
	int ret = 0;
	pr_debug("%s start\n",__func__);

	if(!pdata){
		pr_err("%s end - null pdata[%x]\n",__func__,(int)pdata);
		return -ENODEV;
	}

	ret = disp_ext_i2c_init();

	pr_debug("%s end\n",__func__);
	return ret;
}

static int ld7032_disp_ext_sub_panel_signal_init( struct disp_ext_sub_pdata *pdata )
{
	int ret = 0;
	pr_debug("%s start\n",__func__);

	if(!pdata){
		pr_err("%s end - null pdata[%x]\n",__func__,(int)pdata);
		return -ENODEV;
	}

	if (!gpio_is_valid(pdata->rst_gpio)) {
		pr_err("%s: reset gpio not specified\n",__func__ );
		return -ENODEV;
	}
	if (!gpio_is_valid(pdata->vh_gpio)) {
		pr_err("%s: VH gpio not specified\n",__func__ );
		return -ENODEV;
	}

	ret = gpio_request(pdata->rst_gpio, "oled_rst_n");
	if (ret) {
		pr_err("%s: request reset gpio failed, rc=%d\n",__func__,ret);
		gpio_free(pdata->rst_gpio);
		return ret;
	}
	ret = gpio_request(pdata->vh_gpio, "oled_vh_n");
	if (ret) {
		pr_err("%s: request VH gpio failed, rc=%d\n",__func__,ret);
		gpio_free(pdata->rst_gpio);
		gpio_free(pdata->vh_gpio);
		return ret;
	}

	pr_debug("%s end\n",__func__);
	return ret;
}

static int ld7032_disp_ext_sub_get_panel_dt(
	struct device_node * np,
	struct disp_ext_sub_pdata *pdata
)
{
	int ret = 0;
	u32 tmp;

	pr_debug("%s start\n",__func__);

	if( (!np) | (!pdata) ) {
		pr_err("%s end - null np[%x] pdata[%x]\n",__func__,(int)np,(int)pdata);
		return -ENODEV;
	}

	ret = of_property_read_u32(np, "kc,disp-ext-sub-panel-width", &tmp);
	if (ret) {
		pr_err("%s end - fail get panel width\n",__func__);
		return -EINVAL;
	}
	pdata->xres = tmp;

	ret = of_property_read_u32(np, "kc,disp-ext-sub-panel-height", &tmp);
	if (ret) {
		pr_err("%s end - fail get panel width\n",__func__);
		return -EINVAL;
	}
	pdata->yres = tmp;

	ret = of_property_read_u32(np, "kc,disp-ext-sub-bpp", &tmp);
	if (ret) {
		pr_err("%s end - fail get bpp\n",__func__);
		return -EINVAL;
	}
	pdata->bpp = tmp;

	pr_debug("%s end\n",__func__);
	return ret;
}

static int ld7032_disp_ext_sub_get_signal_dt(
	struct device_node * np,
	struct disp_ext_sub_pdata *pdata
)
{
	int ret = 0;
	pr_debug("%s start\n",__func__);

	if( (!np) | (!pdata) ) {
		pr_err("%s end - null np[%x] pdata[%x]\n",__func__,(int)np,(int)pdata);
		return -ENODEV;
	}

	pdata->rst_gpio = of_get_named_gpio(np, "kc,disp-ext-sub-rst-gpio", 0);
	pdata->vh_gpio = of_get_named_gpio(np, "kc,disp-ext-sub-vh-gpio", 0);
	pdata->voled_gpio = of_get_named_gpio(np,"kc,disp-ext-sub-voled_gpio", 0);

	pr_debug("%s end\n",__func__);
	return ret;
}

static int ld7032_disp_ext_sub_get_seq_dt(
	struct device_node * np,
	struct disp_ext_sub_pdata *pdata
)
{
	int ret = 0;

	pr_debug("%s start\n",__func__);

	if( (!np) | (!pdata) ) {
		pr_err("%s end - null np[%x] pdata[%x]\n",__func__,(int)np,(int)pdata);
		return -ENODEV;
	}

	ret = disp_ext_sub_get_cmd_dt(np,&(pdata->pwron_cmd),"kc,disp-ext-sub-pwron-cmd");
	if (ret) {
		pr_err("%s end - fail get power on command\n",__func__);
		return -EINVAL;
	}

	ret = disp_ext_sub_get_cmd_dt(np,&(pdata->on_cmd),"kc,disp-ext-sub-on-cmd");
	if (ret) {
		pr_err("%s end - fail get on command\n",__func__);
		return -EINVAL;
	}

	ret = disp_ext_sub_get_cmd_dt(np,&(pdata->post_on_cmd),"kc,disp-ext-sub-post-on-cmd");
	if (ret) {
		pr_err("%s end - fail get post on command\n",__func__);
		return -EINVAL;
	}

	ret = disp_ext_sub_get_cmd_dt(np,&(pdata->off_cmd),"kc,disp-ext-sub-off-cmd");
	if (ret) {
		pr_err("%s end - fail get off command\n",__func__);
		return -EINVAL;
	}

	ret = disp_ext_sub_get_cmd_dt(np,&(pdata->pwroff_cmd),"kc,disp-ext-sub-pwroff-cmd");
	if (ret) {
		pr_err("%s end - fail get power off command\n",__func__);
		return -EINVAL;
	}

	ret = disp_ext_sub_get_cmd_dt(np,&(pdata->ram_wr_cmd),"kc,disp-ext-sub-ram-wr-cmd");
	if (ret) {
		pr_err("%s end - fail get ram write command\n",__func__);
		return -EINVAL;
	}

	pr_debug("%s end\n",__func__);
	return ret;
}

static int disp_ext_sub_get_cmd_dt(
	struct device_node *np,
	struct disp_ext_sub_cmds *sub_cmd_p,
	char *cmd_key
)
{
	int ret = 0;

	const char *dt_data_p;
	int dt_data_len = 0;
	char *buf_p;

	struct disp_ext_sub_cmd_hdr *cmd_hdr_p;
	int dt_data_tmp_len = 0;
	char *buf_tmp_p;

	int i, cnt;

	pr_debug("%s start\n",__func__);

	dt_data_p = of_get_property(np, cmd_key, &dt_data_len);
	pr_debug("%s cmd_key[%s] dt_data_p[%x] dt_data_len[%d]\n",
							 __func__,cmd_key ? cmd_key : "(null)", (int)dt_data_p,(int)dt_data_len);
	if (!dt_data_p) {
		pr_err("%s end - fail key=%s\n",__func__,cmd_key);
		return -ENOMEM;
	}

	buf_p = kzalloc(sizeof(char) * dt_data_len, GFP_KERNEL);
	if (!buf_p ){
		pr_err("%s end - fail kzalloc\n",__func__);
		return -ENOMEM;
	}

	memcpy(buf_p, dt_data_p, dt_data_len);

	buf_tmp_p = buf_p;
	dt_data_tmp_len = dt_data_len;
	cnt = 0;
	while (dt_data_tmp_len > sizeof(struct disp_ext_sub_cmd_hdr)) {
		cmd_hdr_p = (struct disp_ext_sub_cmd_hdr *)buf_tmp_p;
		if (cmd_hdr_p->payload_len > dt_data_tmp_len) {
			pr_err("%s payload_len[%d] dt_data_tmp_len[%d]\n",
						 __func__,
						(int)(cmd_hdr_p->payload_len),
						(int)dt_data_tmp_len );
			goto exit_free;
		}

		buf_tmp_p += sizeof(struct disp_ext_sub_cmd_hdr);
		buf_tmp_p += cmd_hdr_p->payload_len;
		dt_data_tmp_len -= sizeof(struct disp_ext_sub_cmd_hdr);
		dt_data_tmp_len -= cmd_hdr_p->payload_len;

		cnt++;
	}

	if (dt_data_tmp_len != 0) {
		pr_err("%s dt_data_tmp_len[%d]\n",__func__,(int)dt_data_tmp_len );
		goto exit_free;
	}

	sub_cmd_p->cmd_p = kzalloc(cnt * sizeof(struct  disp_ext_sub_cmd_detail),
							GFP_KERNEL);

	if (!sub_cmd_p->cmd_p){
		goto exit_free;
	}

	sub_cmd_p->buf_p = buf_p;
	sub_cmd_p->blen = dt_data_len;
	sub_cmd_p->cmd_cnt = cnt;

	buf_tmp_p = buf_p;
	dt_data_tmp_len = dt_data_len;
	for (i = 0; i < cnt; i++) {
		cmd_hdr_p = (struct disp_ext_sub_cmd_hdr *)buf_tmp_p;
		buf_tmp_p += sizeof(struct disp_ext_sub_cmd_hdr);
		sub_cmd_p->cmd_p[i].cmd_hdr = *cmd_hdr_p;
		sub_cmd_p->cmd_p[i].payload_p = buf_tmp_p;

		buf_tmp_p += cmd_hdr_p->payload_len;
		dt_data_tmp_len -= cmd_hdr_p->payload_len;
	}

	pr_debug("%s end\n",__func__);
	return ret;

exit_free:
	kfree(buf_p);
	return -ENOMEM;
}

static int ld7032_disp_ext_sub_get_vreg_dt(
	struct device *dev,
	struct disp_ext_sub_pdata *pdata
)
{
	int rc = 0;

	if (!dev || !pdata) {
		pr_err("%s: invalid input\n", __func__);
		rc = -EINVAL;
		return rc;
	}

	return rc;
}

void ld7032_disp_ext_sub_func_register(struct disp_ext_sub_funcs *funcs)
{
	funcs->pdisp_ext_sub_panel_set_status 	= 	ld7032_disp_ext_sub_panel_set_status;
	funcs->pdisp_ext_sub_panel_update		=	ld7032_disp_ext_sub_panel_update;
	funcs->pdisp_ext_sub_set_cmd			=	ld7032_disp_ext_sub_set_cmd;
	funcs->pdisp_ext_sub_panel_bus_init		=	ld7032_disp_ext_sub_panel_bus_init;
	funcs->pdisp_ext_sub_panel_signal_init	=	ld7032_disp_ext_sub_panel_signal_init;
	funcs->pdisp_ext_sub_get_panel_dt		=	ld7032_disp_ext_sub_get_panel_dt;
	funcs->pdisp_ext_sub_get_signal_dt		=	ld7032_disp_ext_sub_get_signal_dt;
	funcs->pdisp_ext_sub_get_seq_dt			=	ld7032_disp_ext_sub_get_seq_dt;
	funcs->pdisp_ext_sub_get_vreg_dt		=	ld7032_disp_ext_sub_get_vreg_dt;
}


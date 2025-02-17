/***************************************************************
** Copyright (C),  2018,  OPLUS Mobile Comm Corp.,  Ltd
** File : oplus_display_private_api.h
** Description : oplus display private api implement
** Version : 1.0
** Date : 2018/03/20
**
** ------------------------------- Revision History: -----------
**  <author>        <data>        <version >        <desc>
**   Hu.Jie          2018/03/20        1.0           Build this moudle
******************************************************************/
#include "oplus_display_private_api.h"
#include "oplus_onscreenfingerprint.h"
#include "oplus_aod.h"
#include "oplus_ffl.h"
#include "oplus_display_panel_power.h"
#include "oplus_display_panel_seed.h"
#include "oplus_display_panel_hbm.h"
#include "oplus_display_panel_common.h"
#include "oplus_display_panel.h"
#include "sde_trace.h"
/*
 * we will create a sysfs which called /sys/kernel/oplus_display,
 * In that directory, oplus display private api can be called
 */
#include <linux/notifier.h>
#include <linux/msm_drm_notify.h>
#include <soc/oplus/device_info.h>
#if defined(OPLUS_FEATURE_PXLW_IRIS5)
#include <video/mipi_display.h>
#include "dsi_iris5_api.h"
#include "dsi_iris5_lightup.h"
#include "dsi_iris5_loop_back.h"
#endif
#include "dsi_pwr.h"

#ifdef OPLUS_FEATURE_ADFR
#include "oplus_adfr.h"
#endif
#include "oplus_display_panel_cabc.h"

extern int hbm_mode;
extern int spr_mode;
extern int lcd_closebl_flag;
int lcd_closebl_flag_fp = 0;
int iris_recovery_check_state = -1;

extern int oplus_underbrightness_alpha;
int oplus_dimlayer_fingerprint_failcount = 0;
extern int msm_drm_notifier_call_chain(unsigned long val, void *v);
int oplus_dc2_alpha;
int oplus_dimlayer_bl_enable_v3 = 0;
int oplus_dimlayer_bl_enable_v2 = 0;
int oplus_dimlayer_bl_enable = 0;
int oplus_dc_dimlayer_bl_alpha = 223;
int oplus_dimlayer_bl_alpha_value = 223;
int oplus_dc_dimlayer_bl_enable_real = 0;
int oplus_dimlayer_dither_threshold = 0;
int oplus_dimlayer_dither_bitdepth = 6;
int oplus_dc_dimlayer_bl_delay = -1;
int oplus_dc_dimlayer_bl_delay_after = -1;
int oplus_dimlayer_bl_enable_v3_real;

int oplus_dimlayer_bl_enable_v2_real = 0;
bool oplus_skip_datadimming_sync = false;

extern int oplus_debug_max_brightness;

extern int oplus_dimlayer_hbm;

extern PANEL_VOLTAGE_BAK panel_vol_bak[PANEL_VOLTAGE_ID_MAX];
extern u32 panel_pwr_vg_base;
extern int seed_mode;
extern int aod_light_mode;
extern int oplus_dimlayer_bl;
/*#ifdef OPLUS_BUG_STABILITY*/
int oplus_dsi_log_type = OPLUS_DEBUG_LOG_DISABLED;
EXPORT_SYMBOL(oplus_dsi_log_type);
/*#endif*/
#define PANEL_CMD_MIN_TX_COUNT 2

extern int dsi_display_read_panel_reg(struct dsi_display *display, u8 cmd, void *data, size_t len);

static BLOCKING_NOTIFIER_HEAD(lcdinfo_notifiers);
int register_lcdinfo_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&lcdinfo_notifiers, nb);
}
int unregister_lcdinfo_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&lcdinfo_notifiers, nb);
}

EXPORT_SYMBOL(register_lcdinfo_notifier);
EXPORT_SYMBOL(unregister_lcdinfo_notifier);

void lcdinfo_notify(unsigned long val, void *v)
{
	blocking_notifier_call_chain(&lcdinfo_notifiers, val, v);
}

int oplus_set_display_vendor(struct dsi_display *display)
{
	if (!display || !display->panel ||
	    !display->panel->oplus_priv.vendor_name ||
	    !display->panel->oplus_priv.manufacture_name) {
		pr_err("failed to config lcd proc device");
		return -EINVAL;
	}
	register_device_proc("lcd", (char *)display->panel->oplus_priv.vendor_name,
			     (char *)display->panel->oplus_priv.manufacture_name);

	return 0;
}

#if defined(OPLUS_FEATURE_PXLW_IRIS5)
extern int iris_panel_dcs_type_set(struct dsi_cmd_desc *cmd, void *data, size_t len);

extern int iris_panel_dcs_write_wrapper(struct dsi_panel *panel, void *data, size_t len);

extern int iris_panel_dcs_read_wrapper(struct dsi_display *display, u8 cmd, void *rbuf, size_t rlen);
#endif

bool is_dsi_panel(struct drm_crtc *crtc)
{
	struct dsi_display *display = get_main_display();

	if (!display || !display->drm_conn || !display->drm_conn->state) {
		pr_err("failed to find dsi display\n");
		return false;
	}

	if (crtc != display->drm_conn->state->crtc) {
		return false;
	}

	return true;
}

extern int dsi_display_spr_mode(struct dsi_display *display, int mode);

static ssize_t oplus_display_set_hbm(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count) {
	struct dsi_display *display = get_main_display();
	int temp_save = 0;
	int ret = 0;

	sscanf(buf, "%du", &temp_save);
	printk(KERN_INFO "%s oplus_display_set_hbm = %d\n", __func__, temp_save);
	if (get_oplus_display_power_status() != OPLUS_DISPLAY_POWER_ON) {
		printk(KERN_ERR	 "%s oplus_display_set_hbm = %d, but now display panel status is not on\n", __func__, temp_save);
		return -EFAULT;
	}

	if (!display) {
		printk(KERN_INFO "oplus_display_set_hbm and main display is null");
		return -EINVAL;
	}
	__oplus_display_set_hbm(temp_save);

	if (!strcmp(display->panel->oplus_priv.vendor_name, "S6E3HC3")) {
		if((hbm_mode > 1) &&(hbm_mode <= 10)) {
			ret = dsi_display_normal_hbm_on(get_main_display());
		} else if(hbm_mode == 1) {
			ret = dsi_display_normal_hbm_on(get_main_display());
		} else if(hbm_mode == 0) {
			ret = dsi_display_hbm_off(get_main_display());
		}
	} else {
		if((hbm_mode > 1) &&(hbm_mode <= 10)) {
			ret = dsi_display_normal_hbm_on(get_main_display());
		} else if(hbm_mode == 1) {
			ret = dsi_display_hbm_on(get_main_display());
		} else if(hbm_mode == 0) {
			ret = dsi_display_hbm_off(get_main_display());
		}
	}

	if (ret) {
		pr_err("failed to set hbm status ret=%d", ret);
		return ret;
	}

	return count;
}

static ssize_t oplus_display_set_seed(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count) {
	int temp_save = 0;

	sscanf(buf, "%du", &temp_save);
	printk(KERN_INFO "%s oplus_display_set_seed = %d\n", __func__, temp_save);

	__oplus_display_set_seed(temp_save);
	if(get_oplus_display_power_status() == OPLUS_DISPLAY_POWER_ON) {
		if(get_main_display() == NULL) {
			printk(KERN_INFO "oplus_display_set_seed and main display is null");
			return count;
		}

		dsi_display_seed_mode(get_main_display(), seed_mode);
	} else {
		printk(KERN_ERR	 "%s oplus_display_set_seed = %d, but now display panel status is not on\n", __func__, temp_save);
	}
	return count;
}

static ssize_t oplus_set_aod_light_mode(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count) {
	int temp_save = 0;

	sscanf(buf, "%du", &temp_save);

	__oplus_display_set_aod_light_mode(temp_save);
	oplus_update_aod_light_mode();

	return count;
}

extern int __oplus_display_set_spr(int mode);
int oplus_dsi_update_spr_mode(void)
{
	struct dsi_display *display = get_main_display();
	int ret = 0;

	if (!display) {
		pr_err("failed for: %s %d\n", __func__, __LINE__);
		return -EINVAL;
	}

	if (!display->panel->oplus_priv.is_spr_support) {
		LCD_DEBUG_COMMON("not support spr\n");
		return 0;
	}

	ret = dsi_display_spr_mode(display, spr_mode);

	return ret;
}

static ssize_t oplus_display_set_spr(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count) {
	int temp_save = 0;

	sscanf(buf, "%du", &temp_save);
	printk(KERN_INFO "%s oplus_display_set_spr = %d\n", __func__, temp_save);

	__oplus_display_set_spr(temp_save);
	if(get_oplus_display_power_status() == OPLUS_DISPLAY_POWER_ON) {
		if(get_main_display() == NULL) {
			printk(KERN_INFO "oplus_display_set_spr and main display is null");
			return count;
		}

		dsi_display_spr_mode(get_main_display(), spr_mode);
	} else {
		printk(KERN_ERR	 "%s oplus_display_set_spr = %d, but now display panel status is not on\n", __func__, temp_save);
	}
	return count;
}

extern int oplus_display_audio_ready;
static ssize_t oplus_display_set_audio_ready(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count) {

	sscanf(buf, "%du", &oplus_display_audio_ready);

	return count;
}

static ssize_t oplus_display_get_hbm(struct device *dev,
struct device_attribute *attr, char *buf) {

	printk(KERN_INFO "oplus_display_get_hbm = %d\n",hbm_mode);

	return sprintf(buf, "%d\n", hbm_mode);
}

static ssize_t oplus_display_get_seed(struct device *dev,
struct device_attribute *attr, char *buf) {

	printk(KERN_INFO "oplus_display_get_seed = %d\n",seed_mode);

	return sprintf(buf, "%d\n", seed_mode);
}

static ssize_t oplus_get_aod_light_mode(struct device *dev,
struct device_attribute *attr, char *buf) {

	printk(KERN_INFO "oplus_get_aod_light_mode = %d\n",aod_light_mode);

	return sprintf(buf, "%d\n", aod_light_mode);
}

static ssize_t oplus_display_get_spr(struct device *dev,
struct device_attribute *attr, char *buf) {

	printk(KERN_INFO "oplus_display_get_spr = %d\n",spr_mode);

	return sprintf(buf, "%d\n", spr_mode);
}

static ssize_t oplus_display_get_iris_state(struct device *dev,
struct device_attribute *attr, char *buf) {

#if defined(OPLUS_FEATURE_PXLW_IRIS5)
	if (iris_get_feature() && iris_loop_back_validate() == 0) {
		iris_recovery_check_state = 0;
	}

	printk(KERN_INFO "oplus_display_get_iris_state = %d\n",iris_recovery_check_state);
#endif

	return sprintf(buf, "%d\n", iris_recovery_check_state);
}

static ssize_t oplus_display_regulator_control(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count) {
	int temp_save = 0;
	struct dsi_display *temp_display;
	sscanf(buf, "%du", &temp_save);
	printk(KERN_INFO "%s oplus_display_regulator_control = %d\n", __func__, temp_save);
	if(get_main_display() == NULL) {
		printk(KERN_INFO "oplus_display_regulator_control and main display is null");
		return count;
	}
	temp_display = get_main_display();
	if(temp_save == 0) {
#if defined(OPLUS_FEATURE_PXLW_IRIS5)
		if (iris_get_feature())
			iris5_control_pwr_regulator(false);
#endif
		dsi_pwr_enable_regulator(&temp_display->panel->power_info, false);
	} else if(temp_save == 1) {
#if defined(OPLUS_FEATURE_PXLW_IRIS5)
		if (iris_get_feature())
			iris5_control_pwr_regulator(true);
#endif
		dsi_pwr_enable_regulator(&temp_display->panel->power_info, true);
	}
	return count;
}

static ssize_t oplus_display_get_panel_serial_number(struct device *dev,
struct device_attribute *attr, char *buf) {
	int ret = 0;
	unsigned char read[30];
	PANEL_SERIAL_INFO panel_serial_info;
	uint64_t serial_number;
	struct dsi_display *display = get_main_display();
	struct dsi_parser_utils *utils;
	unsigned base_index = 10;
	unsigned base_reg = 0xA1;
	unsigned val = 0;
	int i;

	if (!display) {
		printk(KERN_INFO "oplus_display_get_panel_serial_number and main display is null");
		return -1;
	}
	utils = &display->panel->utils;

	if(get_oplus_display_power_status() != OPLUS_DISPLAY_POWER_ON) {
		printk(KERN_ERR"%s display panel in off status\n", __func__);
		return ret;
	}

	ret = utils->read_u32(utils->data, "oplus,mdss-panel-serial-number-index", &val);
	if (ret) {
		DSI_DEBUG("[%s] panel-serial-number-index unspecified, defaulting to base_index\n",
			 display->panel->name);
	} else {
		base_index = val;
	}
	ret = utils->read_u32(utils->data, "oplus,mdss-panel-serial-number-reg", &val);
	if (ret) {
		DSI_DEBUG("[%s] panel-serial-number-reg unspecified, defaulting to base_index\n",
			 display->panel->name);
	} else {
		base_reg = val;
	}

	/*
	 * for some unknown reason, the panel_serial_info may read dummy,
	 * retry when found panel_serial_info is abnormal.
	 */
	for (i = 0;i < 10; i++) {
		ret = dsi_display_read_panel_reg(get_main_display(), base_reg, read, 17);
		if(ret < 0) {
			ret = scnprintf(buf, PAGE_SIZE,
					"Get panel serial number failed, reason:%d",ret);
			msleep(20);
			continue;
		}

		/*  0xA1               11th        12th    13th    14th    15th
		 *  HEX                0x32        0x0C    0x0B    0x29    0x37
		 *  Bit           [D7:D4][D3:D0] [D5:D0] [D5:D0] [D5:D0] [D5:D0]
		 *  exp              3      2       C       B       29      37
		 *  Yyyy,mm,dd      2014   2m      12d     11h     41min   55sec
		*/
		panel_serial_info.reg_index = base_index;

		panel_serial_info.year		= (read[panel_serial_info.reg_index] & 0xF0) >> 0x4;
		panel_serial_info.month		= read[panel_serial_info.reg_index]	& 0x0F;
		panel_serial_info.day		= read[panel_serial_info.reg_index + 1]	& 0x1F;
		panel_serial_info.hour		= read[panel_serial_info.reg_index + 2]	& 0x1F;
		panel_serial_info.minute	= read[panel_serial_info.reg_index + 3]	& 0x3F;
		panel_serial_info.second	= read[panel_serial_info.reg_index + 4]	& 0x3F;
		panel_serial_info.reserved[0] = read[panel_serial_info.reg_index + 5];
		panel_serial_info.reserved[1] = read[panel_serial_info.reg_index + 6];

		if (!strcmp(display->panel->oplus_priv.vendor_name,"AMS644VA04")) {
			/* 1-4th oplus code xxx
			 * 5th vendor code/Month
			 * 6th Year/Day
			 * 7th Hour
			 * 8th Minute
			 * 9th Second
			 * 10-11th Second(ms)
			 */
			panel_serial_info.year		= ((read[base_index + 1] & 0xE0) >> 5) + 7;;
			panel_serial_info.month		= read[base_index] & 0x0F;
			panel_serial_info.day		= read[base_index + 1] & 0x1F;
		}

		serial_number = (panel_serial_info.year		<< 56)\
			+ (panel_serial_info.month		<< 48)\
			+ (panel_serial_info.day		<< 40)\
			+ (panel_serial_info.hour		<< 32)\
			+ (panel_serial_info.minute	<< 24)\
			+ (panel_serial_info.second	<< 16)\
			+ (panel_serial_info.reserved[0] << 8)\
			+ (panel_serial_info.reserved[1]);
		if (!panel_serial_info.year) {
			/*
			 * the panel we use always large than 2011, so
			 * force retry when year is 2011
			 */
			msleep(20);
			continue;
		}

		ret = scnprintf(buf, PAGE_SIZE, "Get panel serial number: %llx\n",serial_number);
		break;
	}

	return ret;
}

extern char oplus_rx_reg[PANEL_TX_MAX_BUF];
extern char oplus_rx_len;
static ssize_t oplus_display_get_panel_reg(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct dsi_display *display = get_main_display();
	int i, cnt = 0;

	if (!display)
		return -EINVAL;
	mutex_lock(&display->display_lock);

	for (i = 0; i < oplus_rx_len; i++)
		cnt += snprintf(buf + cnt, PANEL_TX_MAX_BUF - cnt,
				"%02x ", oplus_rx_reg[i]);
	cnt += snprintf(buf + cnt, PANEL_TX_MAX_BUF - cnt, "\n");
	mutex_unlock(&display->display_lock);

	return cnt;
}

static ssize_t oplus_display_set_panel_reg(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count) {
	char reg[PANEL_TX_MAX_BUF] = {0x0};
	char payload[PANEL_TX_MAX_BUF] = {0x0};
	u32 index = 0, value = 0, step =0;
	int ret = 0;
	int len = 0;
	char *bufp = (char *)buf;
	struct dsi_display *display = get_main_display();
	char read;

	if (!display || !display->panel) {
		pr_err("debug for: %s %d\n", __func__, __LINE__);
		return -EFAULT;
	}

	if (sscanf(bufp, "%c%n", &read, &step) && read == 'r') {
		bufp += step;
		sscanf(bufp, "%x %d", &value, &len);
		if (len > PANEL_TX_MAX_BUF) {
			pr_err("failed\n");
			return -EINVAL;
		}
#if defined(OPLUS_FEATURE_PXLW_IRIS5)
		if (iris_get_feature() && (iris5_abypass_mode_get(get_main_display()->panel) == PASS_THROUGH_MODE))
			iris_panel_dcs_read_wrapper(get_main_display(), value, reg, len);
		else
			dsi_display_read_panel_reg(get_main_display(),value, reg, len);
#else
		dsi_display_read_panel_reg(get_main_display(),value, reg, len);
#endif

		for (index; index < len; index++) {
			printk("%x ", reg[index]);
		}
		mutex_lock(&display->display_lock);
		memcpy(oplus_rx_reg, reg, PANEL_TX_MAX_BUF);
		oplus_rx_len = len;
		mutex_unlock(&display->display_lock);
		return count;
	}

	while (sscanf(bufp, "%x%n", &value, &step) > 0) {
		reg[len++] = value;
		if (len >= PANEL_TX_MAX_BUF) {
			pr_err("wrong input reg len\n");
			return -EFAULT;
		}
		bufp += step;
	}

	for(index; index < len; index ++ ) {
		payload[index] = reg[index + 1];
	}

	if(get_oplus_display_power_status() == OPLUS_DISPLAY_POWER_ON) {
			/* enable the clk vote for CMD mode panels */
		mutex_lock(&display->display_lock);
		mutex_lock(&display->panel->panel_lock);

		if (display->panel->panel_initialized) {
			if (display->config.panel_mode == DSI_OP_CMD_MODE) {
				dsi_display_clk_ctrl(display->dsi_clk_handle,
						DSI_ALL_CLKS, DSI_CLK_ON);
			}
#if defined(OPLUS_FEATURE_PXLW_IRIS5)
			if (iris_get_feature() && (iris5_abypass_mode_get(display->panel) == PASS_THROUGH_MODE))
				ret = iris_panel_dcs_write_wrapper(display->panel, reg, len);
			else
				ret = mipi_dsi_dcs_write(&display->panel->mipi_device, reg[0],
							 payload, len -1);
#else
			ret = mipi_dsi_dcs_write(&display->panel->mipi_device, reg[0],
						 payload, len -1);
#endif

			if (display->config.panel_mode == DSI_OP_CMD_MODE) {
				dsi_display_clk_ctrl(display->dsi_clk_handle,
						DSI_ALL_CLKS, DSI_CLK_OFF);
			}
		}

		mutex_unlock(&display->panel->panel_lock);
		mutex_unlock(&display->display_lock);

		if (ret < 0) {
			return ret;
		}
	}

	return count;
}

static ssize_t oplus_display_get_panel_id(struct device *dev,
struct device_attribute *attr, char *buf) {
	struct dsi_display *display = get_main_display();
	int ret = 0;
	unsigned char read[30];
	char DA = 0;
	char DB = 0;
	char DC = 0;

	if(get_oplus_display_power_status() == OPLUS_DISPLAY_POWER_ON) {
		if(display == NULL) {
			printk(KERN_INFO "oplus_display_get_panel_id and main display is null");
			ret = -1;
			return ret;
		}

		ret = dsi_display_read_panel_reg(display,0xDA, read, 1);
		if(ret < 0) {
			pr_err("failed to read DA ret=%d\n", ret);
			return -EINVAL;
		}
		DA = read[0];

		ret = dsi_display_read_panel_reg(display, 0xDB, read, 1);
		if(ret < 0) {
			pr_err("failed to read DA ret=%d\n", ret);
			return -EINVAL;
		}
		DB = read[0];

		ret = dsi_display_read_panel_reg(display,0xDC, read, 1);
		if(ret < 0) {
			pr_err("failed to read DA ret=%d\n", ret);
			return -EINVAL;
		}
		DC = read[0];
		ret = scnprintf(buf, PAGE_SIZE, "%02x %02x %02x\n", DA, DB, DC);
	} else {
		printk(KERN_ERR	 "%s oplus_display_get_panel_id, but now display panel status is not on\n", __func__);
	}
	return ret;
}

static ssize_t oplus_display_get_panel_dsc(struct device *dev,
struct device_attribute *attr, char *buf) {
	int ret = 0;
	unsigned char read[30];

	if(get_oplus_display_power_status() == OPLUS_DISPLAY_POWER_ON) {
		if(get_main_display() == NULL) {
			printk(KERN_INFO "oplus_display_get_panel_dsc and main display is null");
			ret = -1;
			return ret;
		}

		ret = dsi_display_read_panel_reg(get_main_display(),0x03, read, 1);
		if(ret < 0) {
			ret = scnprintf(buf, PAGE_SIZE, "oplus_display_get_panel_dsc failed, reason:%d",ret);
		} else {
			ret = scnprintf(buf, PAGE_SIZE, "oplus_display_get_panel_dsc: 0x%x\n",read[0]);
		}
	} else {
		printk(KERN_ERR	 "%s oplus_display_get_panel_dsc, but now display panel status is not on\n", __func__);
	}
	return ret;
}

static ssize_t oplus_display_dump_info(struct device *dev,
struct device_attribute *attr, char *buf) {
	int ret = 0;
	struct dsi_display * temp_display;

	temp_display = get_main_display();

	if(temp_display == NULL ) {
		printk(KERN_INFO "oplus_display_dump_info and main display is null");
		ret = -1;
		return ret;
	}

	if(temp_display->modes == NULL) {
		printk(KERN_INFO "oplus_display_dump_info and display modes is null");
		ret = -1;
		return ret;
	}

	ret = scnprintf(buf , PAGE_SIZE, "oplus_display_dump_info: height =%d,width=%d,frame_rate=%d,clk_rate=%llu\n",
		temp_display->modes->timing.h_active,temp_display->modes->timing.v_active,
		temp_display->modes->timing.refresh_rate,temp_display->modes->timing.clk_rate_hz);

	return ret;
}

static ssize_t oplus_display_get_power_status(struct device *dev,
struct device_attribute *attr, char *buf) {

	printk(KERN_INFO "oplus_display_get_power_status = %d\n",get_oplus_display_power_status());

	return sprintf(buf, "%d\n", get_oplus_display_power_status());
}

static ssize_t oplus_display_set_power_status(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count) {
	int temp_save = 0;

	sscanf(buf, "%du", &temp_save);
	printk(KERN_INFO "%s oplus_display_set_power_status = %d\n", __func__, temp_save);

	__oplus_display_set_power_status(temp_save);

	return count;
}

static ssize_t oplus_display_get_closebl_flag(struct device *dev,
                                struct device_attribute *attr, char *buf)
{
	printk(KERN_INFO "oplus_display_get_closebl_flag = %d\n",lcd_closebl_flag);
	return sprintf(buf, "%d\n", lcd_closebl_flag);
}

static ssize_t oplus_display_set_closebl_flag(struct device *dev,
                               struct device_attribute *attr,
                               const char *buf, size_t count)
{
	int closebl = 0;
	sscanf(buf, "%du", &closebl);
	pr_err("lcd_closebl_flag = %d\n",closebl);
	if(1 != closebl)
		lcd_closebl_flag = 0;
	pr_err("oplus_display_set_closebl_flag = %d\n",lcd_closebl_flag);
	return count;
}

extern const char *cmd_set_prop_map[];
static ssize_t oplus_display_get_dsi_command(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int i, cnt;

	cnt = snprintf(buf, PAGE_SIZE,
		"read current dsi_cmd:\n"
		"    echo dump > dsi_cmd  - then you can find dsi cmd on kmsg\n"
		"set sence dsi cmd:\n"
		"  example hbm on:\n"
		"    echo qcom,mdss-dsi-hbm-on-command > dsi_cmd\n"
		"    echo [dsi cmd0] > dsi_cmd\n"
		"    echo [dsi cmd1] > dsi_cmd\n"
		"    echo [dsi cmdX] > dsi_cmd\n"
		"    echo flush > dsi_cmd\n"
		"available dsi_cmd sences:\n");
	for (i = 0; i < DSI_CMD_SET_MAX; i++)
		cnt += snprintf(buf + cnt, PAGE_SIZE - cnt,
				"    %s\n", cmd_set_prop_map[i]);

	return cnt;
}
static int oplus_display_dump_dsi_command(struct dsi_display *display)
{
	struct dsi_display_mode *mode;
	struct dsi_display_mode_priv_info *priv_info;
	struct dsi_panel_cmd_set *cmd_sets;
	enum dsi_cmd_set_state state;
	struct dsi_cmd_desc *cmds;
	const char *cmd_name;
	int i, j, k, cnt = 0;
	const u8 *tx_buf;
	char bufs[SZ_256];

	if (!display || !display->panel || !display->panel->cur_mode) {
		pr_err("failed to get main dsi display\n");
		return -EFAULT;
	}

	mode = display->panel->cur_mode;
	if (!mode || !mode->priv_info) {
		pr_err("failed to get dsi display mode\n");
		return -EFAULT;
	}

	priv_info = mode->priv_info;
	cmd_sets = priv_info->cmd_sets;

	for (i = 0; i < DSI_CMD_SET_MAX; i++) {
		cmd_name = cmd_set_prop_map[i];
		if (!cmd_name)
			continue;
		state = cmd_sets[i].state;
		pr_err("%s: %s", cmd_name, state == DSI_CMD_SET_STATE_LP ?
				"dsi_lp_mode" : "dsi_hs_mode");

		for (j = 0; j < cmd_sets[i].count; j++) {
			cmds = &cmd_sets[i].cmds[j];
			tx_buf = cmds->msg.tx_buf;
			cnt = snprintf(bufs, SZ_256,
				" %02x %02x %02x %02x %02x %02x %02x",
				cmds->msg.type, cmds->last_command,
				cmds->msg.channel,
				cmds->msg.flags == MIPI_DSI_MSG_REQ_ACK,
				cmds->post_wait_ms,
				(int)(cmds->msg.tx_len >> 8),
				(int)(cmds->msg.tx_len & 0xff));
			for (k = 0; k < cmds->msg.tx_len; k++)
				cnt += snprintf(bufs + cnt,
						SZ_256 > cnt ? SZ_256 - cnt : 0,
						" %02x", tx_buf[k]);
			pr_err("%s", bufs);
		}
	}

	return 0;
}

static int oplus_dsi_panel_get_cmd_pkt_count(const char *data, u32 length, u32 *cnt)
{
	const u32 cmd_set_min_size = 7;
	u32 count = 0;
	u32 packet_length;
	u32 tmp;

	while (length >= cmd_set_min_size) {
		packet_length = cmd_set_min_size;
		tmp = ((data[5] << 8) | (data[6]));
		packet_length += tmp;
		if (packet_length > length) {
			pr_err("format error packet_length[%d] length[%d] count[%d]\n",
				packet_length, length, count);
			return -EINVAL;
		}
		length -= packet_length;
		data += packet_length;
		count++;
	};

	*cnt = count;
	return 0;
}

static int oplus_dsi_panel_create_cmd_packets(const char *data,
					     u32 length,
					     u32 count,
					     struct dsi_cmd_desc *cmd)
{
	int rc = 0;
	int i, j;
	u8 *payload;

	for (i = 0; i < count; i++) {
		u32 size;

		cmd[i].msg.type = data[0];
		cmd[i].last_command = (data[1] == 1 ? true : false);
		cmd[i].msg.channel = data[2];
		cmd[i].msg.flags |= (data[3] == 1 ? MIPI_DSI_MSG_REQ_ACK : 0);
		cmd[i].msg.ctrl = 0;
		cmd[i].post_wait_ms = data[4];
		cmd[i].msg.tx_len = ((data[5] << 8) | (data[6]));

		size = cmd[i].msg.tx_len * sizeof(u8);

		payload = kzalloc(size, GFP_KERNEL);
		if (!payload) {
			rc = -ENOMEM;
			goto error_free_payloads;
		}

		for (j = 0; j < cmd[i].msg.tx_len; j++)
			payload[j] = data[7 + j];

		cmd[i].msg.tx_buf = payload;
		data += (7 + cmd[i].msg.tx_len);
	}

	return rc;
error_free_payloads:
	for (i = i - 1; i >= 0; i--) {
		cmd--;
		kfree(cmd->msg.tx_buf);
	}

	return rc;
}

static ssize_t oplus_display_set_dsi_command(struct device *dev,
                               struct device_attribute *attr,
                               const char *buf, size_t count)
{
	struct dsi_display *display = get_main_display();
	struct dsi_display_mode *mode;
	struct dsi_display_mode_priv_info *priv_info;
	struct dsi_panel_cmd_set *cmd_sets;
	char *bufp = (char *)buf;
	struct dsi_cmd_desc *cmds;
	struct dsi_panel_cmd_set *cmd;
	static char *cmd_bufs;
	static int cmd_counts;
	static u32 oplus_dsi_command = DSI_CMD_SET_MAX;
	static int oplus_dsi_state = DSI_CMD_SET_STATE_HS;
	u32 old_dsi_command = oplus_dsi_command;
	u32 packet_count = 0, size;
	int rc = count, i;
	char data[SZ_256];
	bool flush = false;

	if (!cmd_bufs) {
		cmd_bufs = kmalloc(SZ_4K, GFP_KERNEL);
		if (!cmd_bufs)
		return -ENOMEM;
	}

	if (strlen(buf) >= SZ_256 || count >= SZ_256) {
		pr_err("please recheck the buf and count again\n");
		return -EINVAL;
	}

	sscanf(buf, "%s", data);
	if (!strcmp("dump", data)) {
		rc = oplus_display_dump_dsi_command(display);
		if (rc < 0)
			return rc;
		return count;
	} else if (!strcmp("flush", data)) {
		flush = true;
	} else if (!strcmp("dsi_hs_mode", data)) {
		oplus_dsi_state = DSI_CMD_SET_STATE_HS;
	} else if (!strcmp("dsi_lp_mode", data)) {
		oplus_dsi_state = DSI_CMD_SET_STATE_LP;
	} else {
		for (i = 0; i < DSI_CMD_SET_MAX; i++) {
			if (!strcmp(cmd_set_prop_map[i], data)) {
				oplus_dsi_command = i;
				flush = true;
				break;
			}
		}
	}

	if (!flush) {
		u32 value = 0, step = 0;

		while (sscanf(bufp, "%x%n", &value, &step) > 0) {
			if (value > 0xff) {
				pr_err("input reg don't large than 0xff\n");
				return -EINVAL;
			}
			cmd_bufs[cmd_counts++] = value;
			if (cmd_counts >= SZ_4K) {
				pr_err("wrong input reg len\n");
				cmd_counts = 0;
				return -EFAULT;
			}
			bufp += step;
		}
		return count;
	}

	if (!cmd_counts)
		return rc;
	if (old_dsi_command >= DSI_CMD_SET_MAX) {
		pr_err("UnSupport dsi command set\n");
		goto error;
	}

	if (!display || !display->panel || !display->panel->cur_mode) {
		pr_err("failed to get main dsi display\n");
		rc = -EFAULT;
		goto error;
	}

	mode = display->panel->cur_mode;
	if (!mode || !mode->priv_info) {
		pr_err("failed to get dsi display mode\n");
		rc = -EFAULT;
		goto error;
	}

	priv_info = mode->priv_info;
	cmd_sets = priv_info->cmd_sets;

	cmd = &cmd_sets[old_dsi_command];

	rc = oplus_dsi_panel_get_cmd_pkt_count(cmd_bufs, cmd_counts,
			&packet_count);
	if (rc) {
		pr_err("commands failed, rc=%d\n", rc);
		goto error;
	}

	size = packet_count * sizeof(*cmd->cmds);

	cmds = kzalloc(size, GFP_KERNEL);
	if (!cmds) {
		rc = -ENOMEM;
		goto error;
	}

	rc = oplus_dsi_panel_create_cmd_packets(cmd_bufs, cmd_counts,
			packet_count, cmds);
	if (rc) {
		pr_err("failed to create cmd packets, rc=%d\n", rc);
		goto error_free_cmds;
	}

	mutex_lock(&display->panel->panel_lock);

	kfree(cmd->cmds);
	cmd->cmds = cmds;
	cmd->count = packet_count;
	if (oplus_dsi_state == DSI_CMD_SET_STATE_LP)
		cmd->state = DSI_CMD_SET_STATE_LP;
	else if (oplus_dsi_state == DSI_CMD_SET_STATE_LP)
		cmd->state = DSI_CMD_SET_STATE_HS;

	mutex_unlock(&display->panel->panel_lock);

	cmd_counts = 0;
	oplus_dsi_state = DSI_CMD_SET_STATE_HS;

	return count;

error_free_cmds:
	kfree(cmds);
error:
	cmd_counts = 0;
	oplus_dsi_state = DSI_CMD_SET_STATE_HS;

	return rc;
}

extern int oplus_panel_alpha;
struct oplus_brightness_alpha *oplus_brightness_alpha_lut = NULL;
int interpolate(int x, int xa, int xb, int ya, int yb, bool nosub)
{
	int bf, factor, plus;
	int sub = 0;

	bf = 2 * (yb - ya) * (x - xa) / (xb - xa);
	factor = bf / 2;
	plus = bf % 2;
	if ((!nosub) && (xa - xb) && (yb - ya))
		sub = 2 * (x - xa) * (x - xb) / (yb - ya) / (xa - xb);

	return ya + factor + plus + sub;
}

static ssize_t oplus_display_get_dim_alpha(struct device *dev,
                                struct device_attribute *attr, char *buf)
{
	struct dsi_display *display = get_main_display();

	if (!display->panel->is_hbm_enabled ||
	    (get_oplus_display_power_status() != OPLUS_DISPLAY_POWER_ON))
		return sprintf(buf, "%d\n", 0);

	return sprintf(buf, "%d\n", oplus_underbrightness_alpha);
}

static ssize_t oplus_display_set_dim_alpha(struct device *dev,
                               struct device_attribute *attr,
                               const char *buf, size_t count)
{
	sscanf(buf, "%x", &oplus_panel_alpha);

	return count;
}

static ssize_t oplus_display_get_dc_dim_alpha(struct device *dev,
                                struct device_attribute *attr, char *buf)
{
	int ret = 0;
	struct dsi_display *display = get_main_display();

	if (!display || !display->panel) {
		pr_err("%s main display is NULL\n", __func__);
		return sprintf(buf, "%d\n", 0);
	}

	if (display->panel->is_hbm_enabled ||
	    get_oplus_display_power_status() != OPLUS_DISPLAY_POWER_ON)
		ret = 0;
	if(oplus_dc2_alpha != 0) {
		ret = oplus_dc2_alpha;
	} else if(oplus_underbrightness_alpha != 0){
		ret = 0;
	} else if (oplus_dimlayer_bl_enable_v3_real) {
		ret = 1;
	}

	return sprintf(buf, "%d\n", ret);
}

static ssize_t oplus_display_get_dimlayer_backlight(struct device *dev,
                                struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d %d %d %d %d %d\n", oplus_dc_dimlayer_bl_alpha,
			oplus_dimlayer_bl_alpha_value, oplus_dimlayer_dither_threshold,
			oplus_dimlayer_dither_bitdepth, oplus_dc_dimlayer_bl_delay, oplus_dc_dimlayer_bl_delay_after);
}

static ssize_t oplus_display_set_dimlayer_backlight(struct device *dev,
                               struct device_attribute *attr,
                               const char *buf, size_t count)
{
	sscanf(buf, "%d %d %d %d %d %d", &oplus_dc_dimlayer_bl_alpha,
		&oplus_dimlayer_bl_alpha_value, &oplus_dimlayer_dither_threshold,
		&oplus_dimlayer_dither_bitdepth, &oplus_dc_dimlayer_bl_delay,
		&oplus_dc_dimlayer_bl_delay_after);

	return count;
}

int oplus_dc_dimlayer_bl_on_vblank = INT_MAX;
int oplus_dc_dimlayer_bl_off_vblank = INT_MAX;
int oplus_fod_on_vblank = -1;
int oplus_fod_off_vblank = -1;
static int oplus_datadimming_v3_debug_value = -1;
static int oplus_datadimming_v3_debug_delay = 16000;
static ssize_t oplus_display_get_debug(struct device *dev,
                                struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d %d %d %d %d %d\n", oplus_dc_dimlayer_bl_on_vblank, oplus_dc_dimlayer_bl_off_vblank, \
				oplus_fod_on_vblank, oplus_fod_off_vblank, oplus_datadimming_v3_debug_value, oplus_datadimming_v3_debug_delay);
}

static ssize_t oplus_display_set_debug(struct device *dev,
                               struct device_attribute *attr,
                               const char *buf, size_t count)
{
	sscanf(buf, "%d %d %d %d %d %d", &oplus_dc_dimlayer_bl_on_vblank, &oplus_dc_dimlayer_bl_off_vblank,
		&oplus_fod_on_vblank, &oplus_fod_off_vblank, &oplus_datadimming_v3_debug_value, &oplus_datadimming_v3_debug_delay);

	return count;
}


static ssize_t oplus_display_get_dimlayer_enable(struct device *dev,
                                struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d %d\n", oplus_dimlayer_bl_enable,oplus_dimlayer_bl_enable_v2);
}

int oplus_panel_update_backlight_unlock(struct dsi_panel *panel)
{
	return dsi_panel_set_backlight(panel, panel->bl_config.bl_level);
}

extern int oplus_datadimming_vblank_count;
extern atomic_t oplus_datadimming_vblank_ref;

static ssize_t oplus_display_set_dimlayer_enable(struct device *dev,
                               struct device_attribute *attr,
                               const char *buf, size_t count)
{
	struct dsi_display *display = NULL;
	struct drm_connector *dsi_connector = NULL;

	display = get_main_display();
	if (!display) {
		return -EINVAL;
	}

	dsi_connector = display->drm_conn;
	if (display && display->name) {
		int enable = 0;
		int err = 0;

		sscanf(buf, "%d", &enable);
		mutex_lock(&display->display_lock);
		if (!dsi_connector || !dsi_connector->state || !dsi_connector->state->crtc) {
			pr_err("[%s]: display not ready\n", __func__);
		}else {
			err = drm_crtc_vblank_get(dsi_connector->state->crtc);
			if (err) {
				pr_err("failed to get crtc vblank, error=%d\n", err);
			} else {
				/* do vblank put after 7 frames */
				oplus_datadimming_vblank_count= 7;
				atomic_inc(&oplus_datadimming_vblank_ref);
			}
		}

		usleep_range(17000, 17100);
		if (!strcmp(display->panel->oplus_priv.vendor_name,"ANA6706")) {
			oplus_dimlayer_bl_enable = enable;
		} else {
			if (!strcmp(display->name,"qcom,mdss_dsi_oplus19101boe_nt37800_1080_2400_cmd"))
				oplus_dimlayer_bl_enable_v3 = enable;
			else
				oplus_dimlayer_bl_enable_v2 = enable;
		}
		mutex_unlock(&display->display_lock);
	}

	return count;
}

static ssize_t oplus_display_get_dimlayer_hbm(struct device *dev,
                                struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", oplus_dimlayer_hbm);
}

extern int oplus_dimlayer_hbm_vblank_count;
extern atomic_t oplus_dimlayer_hbm_vblank_ref;
static ssize_t oplus_display_set_dimlayer_hbm(struct device *dev,
                               struct device_attribute *attr,
                               const char *buf, size_t count)
{
	struct dsi_display *display = get_main_display();
	struct drm_connector *dsi_connector = display->drm_conn;
	int err = 0;
	int value = 0;

	if (!strcmp(display->panel->oplus_priv.panel_physical_type, "lcd"))
		return count;

	sscanf(buf, "%d", &value);
	value = !!value;
	if (oplus_dimlayer_hbm == value)
		return count;
	if (!dsi_connector || !dsi_connector->state || !dsi_connector->state->crtc) {
		pr_err("[%s]: display not ready\n", __func__);
	} else {
		err = drm_crtc_vblank_get(dsi_connector->state->crtc);
		if (err) {
			pr_err("failed to get crtc vblank, error=%d\n", err);
		} else {
			/* do vblank put after 5 frames */
			oplus_dimlayer_hbm_vblank_count = 5;
			atomic_inc(&oplus_dimlayer_hbm_vblank_ref);
		}
	}
	oplus_dimlayer_hbm = value;

	pr_err("debug for oplus_display_set_dimlayer_hbm set oplus_dimlayer_hbm = %d\n",oplus_dimlayer_hbm);

	return count;
}

int oplus_force_screenfp = 0;
static ssize_t oplus_display_get_forcescreenfp(struct device *dev,
                                struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", oplus_force_screenfp);
}

static ssize_t oplus_display_set_forcescreenfp(struct device *dev,
                               struct device_attribute *attr,
                               const char *buf, size_t count)
{
	sscanf(buf, "%x", &oplus_force_screenfp);

	return count;
}

static ssize_t oplus_display_get_esd_status(struct device *dev,
                                struct device_attribute *attr, char *buf)
{
	struct dsi_display *display = get_main_display();
	int rc = 0;

	if (!display)
		return -ENODEV;
	mutex_lock(&display->display_lock);

	if (!display->panel) {
		rc = -EINVAL;
		goto error;
	}

	rc = sprintf(buf, "%d\n", display->panel->esd_config.esd_enabled);

error:
	mutex_unlock(&display->display_lock);
	return rc;
}

static ssize_t oplus_display_set_esd_status(struct device *dev,
                               struct device_attribute *attr,
                               const char *buf, size_t count)
{
	struct dsi_display *display = get_main_display();
	int enable = 0;

	sscanf(buf, "%du", &enable);

	pr_err("debug for oplus_display_set_esd_status, the enable value = %d\n", enable);

	if (!display)
		return -ENODEV;

	if (!display->panel || !display->drm_conn) {
		return -EINVAL;
	}

	if (!enable) {
		if (display->panel->esd_config.esd_enabled)
		{
			sde_connector_schedule_status_work(display->drm_conn, false);
			display->panel->esd_config.esd_enabled = false;
			pr_err("disable esd work");
		}
	} else {
		sde_connector_schedule_status_work(display->drm_conn, true);
		display->panel->esd_config.esd_enabled = true;
		pr_err("enable esd work");
	}

	return count;
}

static ssize_t oplus_display_notify_panel_blank(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count) {

	struct msm_drm_notifier notifier_data;
	int blank;
	int temp_save = 0;

	sscanf(buf, "%du", &temp_save);
	printk(KERN_INFO "%s oplus_display_notify_panel_blank = %d\n", __func__, temp_save);

	if(temp_save == 1) {
		blank = MSM_DRM_BLANK_UNBLANK;
		notifier_data.data = &blank;
		notifier_data.id = 0;
		msm_drm_notifier_call_chain(MSM_DRM_EARLY_EVENT_BLANK,
						   &notifier_data);
		msm_drm_notifier_call_chain(MSM_DRM_EVENT_BLANK,
						   &notifier_data);
	} else if (temp_save == 0) {
		blank = MSM_DRM_BLANK_POWERDOWN;
		notifier_data.data = &blank;
		notifier_data.id = 0;
		msm_drm_notifier_call_chain(MSM_DRM_EARLY_EVENT_BLANK,
						   &notifier_data);
	}
	return count;
}

extern int is_ffl_enable;
static ssize_t oplus_get_ffl_setting(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", is_ffl_enable);
}

static ssize_t oplus_set_ffl_setting(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int enable = 0;

	sscanf(buf, "%du", &enable);
	printk(KERN_INFO "%s oplus_set_ffl_setting = %d\n", __func__, enable);

	oplus_ffl_set(enable);

	return count;
}

int oplus_onscreenfp_status = 0;
ktime_t oplus_onscreenfp_pressed_time;
u32 oplus_onscreenfp_vblank_count = 0;
#ifdef OPLUS_FEATURE_AOD_RAMLESS
int oplus_display_mode = 1;
static DECLARE_WAIT_QUEUE_HEAD(oplus_aod_wait);

bool is_oplus_display_aod_mode(void)
{
	return !oplus_onscreenfp_status && !oplus_display_mode;
}

bool is_oplus_aod_ramless(void)
{
	struct dsi_display *display = get_main_display();
	if (!display || !display->panel)
		return false;
	return display->panel->oplus_priv.is_aod_ramless;
}

int oplus_display_atomic_check(struct drm_crtc *crtc, struct drm_crtc_state *state)
{
	struct dsi_display *display = get_main_display();

	if (!is_dsi_panel(crtc))
		return 0;

	if (display && display->panel &&
	    display->panel->oplus_priv.is_aod_ramless &&
	    is_oplus_display_aod_mode() &&
	    (crtc->state->mode.flags & DRM_MODE_FLAG_CMD_MODE_PANEL)) {
		wait_event_timeout(oplus_aod_wait, !is_oplus_display_aod_mode(),
				   msecs_to_jiffies(100));
	}

	return 0;
}
#endif /* OPLUS_FEATURE_AOD_RAMLESS */

static ssize_t oplus_display_notify_fp_press(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct dsi_display *display = get_main_display();
	struct drm_device *drm_dev = display->drm_dev;
	struct drm_connector *dsi_connector = display->drm_conn;
	struct drm_mode_config *mode_config = &drm_dev->mode_config;
	struct msm_drm_private *priv = drm_dev->dev_private;
	struct drm_atomic_state *state;
	struct drm_crtc_state *crtc_state;
	struct drm_crtc *crtc;
	int onscreenfp_status = 0;
	int vblank_get = -EINVAL;
	int err = 0;
	int i;
	bool if_con = false;
	static ktime_t on_time;
#ifdef OPLUS_FEATURE_AOD_RAMLESS
	struct drm_display_mode *cmd_mode = NULL;
	struct drm_display_mode *vid_mode = NULL;
	struct drm_display_mode *mode = NULL;
	bool mode_changed = false;
#endif /* OPLUS_FEATURE_AOD_RAMLESS */

	if (!dsi_connector || !dsi_connector->state || !dsi_connector->state->crtc) {
		pr_err("[%s]: display not ready\n", __func__);
		return count;
	}

	sscanf(buf, "%du", &onscreenfp_status);
	onscreenfp_status = !!onscreenfp_status;
	if (onscreenfp_status == oplus_onscreenfp_status)
		return count;

	pr_info("notify fingerpress %s\n", onscreenfp_status ? "on" : "off");

	if (OPLUS_DISPLAY_AOD_SCENE == get_oplus_display_scene()) {
		if (onscreenfp_status) {
			on_time = ktime_get();
		} else {
			ktime_t now = ktime_get();
			ktime_t delta = ktime_sub(now, on_time);

			if (ktime_to_ns(delta) < 300000000)
				msleep(300 - (ktime_to_ns(delta) / 1000000));
		}
	}

	if ((oplus_get_panel_brightness() == 0) && onscreenfp_status) {
		pr_info("notify fingerpress return as screen is off\n");
		return count;
	}

	vblank_get = drm_crtc_vblank_get(dsi_connector->state->crtc);
	if (vblank_get) {
		pr_err("failed to get crtc vblank\n", vblank_get);
	}
	oplus_onscreenfp_status = onscreenfp_status;

	if_con = false; /* onscreenfp_status && (OPLUS_DISPLAY_AOD_SCENE == get_oplus_display_scene()); */
#ifdef OPLUS_FEATURE_AOD_RAMLESS
	if_con = if_con && !display->panel->oplus_priv.is_aod_ramless;
#endif /* OPLUS_FEATURE_AOD_RAMLESS */
	if (if_con) {
		/* enable the clk vote for CMD mode panels */
		if (display->config.panel_mode == DSI_OP_CMD_MODE) {
			dsi_display_clk_ctrl(display->dsi_clk_handle,
					DSI_ALL_CLKS, DSI_CLK_ON);
		}

		mutex_lock(&display->panel->panel_lock);

		if (display->panel->panel_initialized)
			err = dsi_panel_tx_cmd_set(display->panel, DSI_CMD_AOD_HBM_ON);

		mutex_unlock(&display->panel->panel_lock);
		if (err)
			pr_err("failed to setting aod hbm on mode %d\n", err);

		if (display->config.panel_mode == DSI_OP_CMD_MODE) {
			dsi_display_clk_ctrl(display->dsi_clk_handle,
					DSI_ALL_CLKS, DSI_CLK_OFF);
		}
	}
#ifdef OPLUS_FEATURE_AOD_RAMLESS
	if (!display->panel->oplus_priv.is_aod_ramless) {
#endif /* OPLUS_FEATURE_AOD_RAMLESS */
		oplus_onscreenfp_vblank_count = drm_crtc_vblank_count(
			dsi_connector->state->crtc);
		oplus_onscreenfp_pressed_time = ktime_get();
#ifdef OPLUS_FEATURE_AOD_RAMLESS
	}
#endif /* OPLUS_FEATURE_AOD_RAMLESS */

	drm_modeset_lock_all(drm_dev);

	state = drm_atomic_state_alloc(drm_dev);
	if (!state)
		goto error;

	state->acquire_ctx = mode_config->acquire_ctx;
	crtc = dsi_connector->state->crtc;
	crtc_state = drm_atomic_get_crtc_state(state, crtc);
	for (i = 0; i < priv->num_crtcs; i++) {
		if (priv->disp_thread[i].crtc_id == crtc->base.id) {
			if (priv->disp_thread[i].thread)
				kthread_flush_worker(&priv->disp_thread[i].worker);
		}
	}

#ifdef OPLUS_FEATURE_AOD_RAMLESS
	if (display->panel->oplus_priv.is_aod_ramless) {
		struct drm_display_mode *set_mode = NULL;

		if (oplus_display_mode == 2)
			goto error;

		list_for_each_entry(mode, &dsi_connector->modes, head) {
			if (drm_mode_vrefresh(mode) == 0)
				continue;
			if (mode->flags & DRM_MODE_FLAG_VID_MODE_PANEL)
				vid_mode = mode;
			if (mode->flags & DRM_MODE_FLAG_CMD_MODE_PANEL)
				cmd_mode = mode;
		}

		set_mode = oplus_display_mode ? vid_mode : cmd_mode;
		set_mode = onscreenfp_status ? vid_mode : set_mode;
		if (!crtc_state->active || !crtc_state->enable)
			goto error;

		if (set_mode && drm_mode_vrefresh(set_mode) != drm_mode_vrefresh(&crtc_state->mode)) {
			mode_changed = true;
		} else {
			mode_changed = false;
		}

		if (mode_changed) {
			display->panel->dyn_clk_caps.dyn_clk_support = false;
			drm_atomic_set_mode_for_crtc(crtc_state, set_mode);
		}

		wake_up(&oplus_aod_wait);
	}
#endif /* OPLUS_FEATURE_AOD_RAMLESS */

	err = drm_atomic_commit(state);
	drm_atomic_state_put(state);

#ifdef OPLUS_FEATURE_AOD_RAMLESS
	if (display->panel->oplus_priv.is_aod_ramless && mode_changed) {
		for (i = 0; i < priv->num_crtcs; i++) {
			if (priv->disp_thread[i].crtc_id == crtc->base.id) {
				if (priv->disp_thread[i].thread) {
					kthread_flush_worker(&priv->disp_thread[i].worker);
				}
			}
		}
		if (oplus_display_mode == 1)
			display->panel->dyn_clk_caps.dyn_clk_support = true;
	}
#endif /* OPLUS_FEATURE_AOD_RAMLESS */

error:
	drm_modeset_unlock_all(drm_dev);
	if (!vblank_get)
		drm_crtc_vblank_put(dsi_connector->state->crtc);

	return count;
}

static ssize_t oplus_display_get_roundcorner(struct device *dev,
                                struct device_attribute *attr, char *buf)
{
	struct dsi_display *display = get_main_display();
	bool roundcorner = true;

	if (display && display->name &&
	    !strcmp(display->name,"qcom,mdss_dsi_oplus19101boe_nt37800_1080_2400_cmd"))
		roundcorner = false;

	return sprintf(buf, "%d\n", roundcorner);
}

DEFINE_MUTEX(dynamic_osc_clock_lock);
extern int dynamic_osc_clock;

int dsi_update_dynamic_osc_clock(void)
{
	struct dsi_display *display = get_main_display();
	int rc = 0;
	int osc_clock_rate = dynamic_osc_clock;

	if (!display||!display->panel) {
		pr_err("failed for: %s %d\n", __func__, __LINE__);
		return -EINVAL;
	}

	if (!display->panel->oplus_priv.is_osc_support) {
		LCD_DEBUG_COMMON("not support osc\n");
		return 0;
	}

	mutex_lock(&display->display_lock);
	mutex_lock(&display->panel->panel_lock);

	if (!dsi_panel_initialized(display->panel)) {
		rc = -EINVAL;
		goto unlock;
	}

	/* enable the clk vote for CMD mode panels */
	if (display->config.panel_mode == DSI_OP_CMD_MODE) {
		dsi_display_clk_ctrl(display->dsi_clk_handle,
				DSI_CORE_CLK, DSI_CLK_ON);
	}

	if (osc_clock_rate) {
		if (osc_clock_rate == display->panel->oplus_priv.osc_clk_mode0_rate) {
			rc = dsi_panel_tx_cmd_set(display->panel, DSI_CMD_OSC_CLK_MODEO0);
		} else if (osc_clock_rate == display->panel->oplus_priv.osc_clk_mode1_rate) {
			rc = dsi_panel_tx_cmd_set(display->panel, DSI_CMD_OSC_CLK_MODEO1);
		} else {
			pr_err("[%s]not support osc clk rate=%d\n", __func__, osc_clock_rate);
		}
		if (rc)
			pr_err("Failed to configure osc dynamic clk\n");
	} else {
		pr_info("[%s] osc clk rate is 0, not config !\n", __func__);
	}

	if (display->config.panel_mode == DSI_OP_CMD_MODE) {
		rc = dsi_display_clk_ctrl(display->dsi_clk_handle,
				DSI_CORE_CLK, DSI_CLK_OFF);
	}

unlock:
	mutex_unlock(&display->panel->panel_lock);
	mutex_unlock(&display->display_lock);

	return rc;
}


static ssize_t oplus_display_get_dynamic_osc_clock(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct dsi_display *display = get_main_display();
	int rc = 0;

	if (!display) {
		pr_err("failed for: %s %d\n", __func__, __LINE__);
		return -EINVAL;
	}

	mutex_lock(&display->display_lock);

	rc = snprintf(buf, PAGE_SIZE, "%d\n", dynamic_osc_clock);
	pr_debug("%s: read dsi clk rate %d\n", __func__,
			dynamic_osc_clock);

	mutex_unlock(&display->display_lock);

	return rc;
}

static ssize_t oplus_display_set_dynamic_osc_clock(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct dsi_display *display = get_main_display();
	int osc_clk = 0;
	int rc = 0;

	if (!display) {
		pr_err("failed for: %s %d\n", __func__, __LINE__);
		return -EINVAL;
	}

	if (!display->panel->oplus_priv.is_osc_support) {
		LCD_DEBUG_COMMON("osc is not support\n");
		return -EINVAL;
	}

	if(get_oplus_display_power_status() != OPLUS_DISPLAY_POWER_ON) {
		printk(KERN_ERR"%s display panel in off status\n", __func__);
		return -EFAULT;
	}

	sscanf(buf, "%du", &osc_clk);
	if (display->panel->panel_mode != DSI_OP_CMD_MODE) {
		pr_err("only supported for command mode\n");
		return -EFAULT;
	}

	LCD_INFO("osc clk param value: '%d'\n", osc_clk);

	mutex_lock(&display->display_lock);
	mutex_lock(&display->panel->panel_lock);

	if (!dsi_panel_initialized(display->panel)) {
		rc = -EINVAL;
		goto unlock;
	}

	/* enable the clk vote for CMD mode panels */
	if (display->config.panel_mode == DSI_OP_CMD_MODE) {
		dsi_display_clk_ctrl(display->dsi_clk_handle,
				DSI_CORE_CLK, DSI_CLK_ON);
	}

	if (display->panel->oplus_priv.osc_clk_mode0_rate == osc_clk) {
		rc = dsi_panel_tx_cmd_set(display->panel, DSI_CMD_OSC_CLK_MODEO0);
	} else if (display->panel->oplus_priv.osc_clk_mode1_rate == osc_clk) {
		rc = dsi_panel_tx_cmd_set(display->panel, DSI_CMD_OSC_CLK_MODEO1);
	} else {
		LCD_INFO("osc clk value: '%d' not support!\n", osc_clk);
	}
	if (rc)
		pr_err("Failed to configure osc dynamic clk\n");
	else
		rc = count;

	if (display->config.panel_mode == DSI_OP_CMD_MODE) {
		rc = dsi_display_clk_ctrl(display->dsi_clk_handle,
				DSI_CORE_CLK, DSI_CLK_OFF);
	}
	dynamic_osc_clock = osc_clk;

unlock:
	mutex_unlock(&display->panel->panel_lock);
	mutex_unlock(&display->display_lock);

	return count;
}

static ssize_t oplus_display_get_max_brightness(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct dsi_display *display = get_main_display();
	struct dsi_panel *panel = display->panel;

	if(oplus_debug_max_brightness == 0) {
		return sprintf(buf, "%d\n", panel->bl_config.brightness_normal_max_level);
	} else {
		return sprintf(buf, "%d\n", oplus_debug_max_brightness);
	}
}

static ssize_t oplus_display_set_max_brightness(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	sscanf(buf, "%du", &oplus_debug_max_brightness);

	return count;
}

static ssize_t oplus_display_get_ccd_check(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct dsi_display *display = get_main_display();
	struct mipi_dsi_device *mipi_device;
	int rc = 0;
	int ccd_check = 0;

	if (!display || !display->panel) {
		pr_err("failed for: %s %d\n", __func__, __LINE__);
		return -EINVAL;
	}

	if(get_oplus_display_power_status() != OPLUS_DISPLAY_POWER_ON) {
		printk(KERN_ERR"%s display panel in off status\n", __func__);
		return -EFAULT;
	}

	if (display->panel->panel_mode != DSI_OP_CMD_MODE) {
		pr_err("only supported for command mode\n");
		return -EFAULT;
	}

	if (!(display && display->panel->oplus_priv.vendor_name) ||
		!strcmp(display->panel->oplus_priv.vendor_name,"NT37800") ||
		!strcmp(display->panel->oplus_priv.vendor_name, "AMB655XL08")||
		!strcmp(display->panel->oplus_priv.vendor_name, "AMS655Ug04")) {
		ccd_check = 0;
		goto end;
	}

	mipi_device = &display->panel->mipi_device;

	mutex_lock(&display->display_lock);
	mutex_lock(&display->panel->panel_lock);

	if (!dsi_panel_initialized(display->panel)) {
		rc = -EINVAL;
		goto unlock;
	}

	rc = dsi_display_cmd_engine_enable(display);
	if (rc) {
		pr_err("%s, cmd engine enable failed\n", __func__);
		goto unlock;
	}

	/* enable the clk vote for CMD mode panels */
	if (display->config.panel_mode == DSI_OP_CMD_MODE) {
		dsi_display_clk_ctrl(display->dsi_clk_handle,
				DSI_CORE_CLK, DSI_CLK_ON);
	}

	if (!strcmp(display->panel->oplus_priv.vendor_name,"AMB655UV01")) {
		{
			char value[] = { 0x5A, 0x5A };
			rc = mipi_dsi_dcs_write(mipi_device, 0xF0, value, sizeof(value));
		}
		{
			char value[] = { 0x44, 0x50 };
			rc = mipi_dsi_dcs_write(mipi_device, 0xE7, value, sizeof(value));
		}
		usleep_range(1000, 1100);
		{
			char value[] = { 0x03 };
			rc = mipi_dsi_dcs_write(mipi_device, 0xB0, value, sizeof(value));
		}
	} else {
		{
			char value[] = { 0x5A, 0x5A };
			rc = mipi_dsi_dcs_write(mipi_device, 0xF0, value, sizeof(value));
		}
		{
			char value[] = { 0x02 };
			rc = mipi_dsi_dcs_write(mipi_device, 0xB0, value, sizeof(value));
		}
		{
			char value[] = { 0x44, 0x50 };
			rc = mipi_dsi_dcs_write(mipi_device, 0xCC, value, sizeof(value));
		}
		usleep_range(1000, 1100);
		{
			char value[] = { 0x05 };
			rc = mipi_dsi_dcs_write(mipi_device, 0xB0, value, sizeof(value));
		}
	}

	if (display->config.panel_mode == DSI_OP_CMD_MODE) {
		rc = dsi_display_clk_ctrl(display->dsi_clk_handle,
				DSI_CORE_CLK, DSI_CLK_OFF);
	}
	dsi_display_cmd_engine_disable(display);

	mutex_unlock(&display->panel->panel_lock);
	mutex_unlock(&display->display_lock);
	if (!strcmp(display->panel->oplus_priv.vendor_name,"AMB655UV01")) {
		{
			unsigned char read[10];

			rc = dsi_display_read_panel_reg(display, 0xE1, read, 1);

			pr_err("read ccd_check value = 0x%x rc=%d\n", read[0], rc);
			ccd_check = read[0];
		}
	} else {
		{
			unsigned char read[10];

			rc = dsi_display_read_panel_reg(display, 0xCC, read, 1);

			pr_err("read ccd_check value = 0x%x rc=%d\n", read[0], rc);
			ccd_check = read[0];
		}
	}

	mutex_lock(&display->display_lock);
	mutex_lock(&display->panel->panel_lock);

	if (!dsi_panel_initialized(display->panel)) {
		rc = -EINVAL;
		goto unlock;
	}

	rc = dsi_display_cmd_engine_enable(display);
	if (rc) {
		pr_err("%s, cmd engine enable failed\n", __func__);
		goto unlock;
	}

	if (display->config.panel_mode == DSI_OP_CMD_MODE) {
		dsi_display_clk_ctrl(display->dsi_clk_handle,
				DSI_CORE_CLK, DSI_CLK_ON);
	}

	{
		char value[] = { 0xA5, 0xA5 };
		rc = mipi_dsi_dcs_write(mipi_device, 0xF0, value, sizeof(value));
	}

	if (display->config.panel_mode == DSI_OP_CMD_MODE) {
		rc = dsi_display_clk_ctrl(display->dsi_clk_handle,
				DSI_CORE_CLK, DSI_CLK_OFF);
	}

	dsi_display_cmd_engine_disable(display);
unlock:

	mutex_unlock(&display->panel->panel_lock);
	mutex_unlock(&display->display_lock);
end:
	pr_err("[%s] ccd_check = %d\n",  display->panel->oplus_priv.vendor_name, ccd_check);
	return sprintf(buf, "%d\n", ccd_check);
}

int dsi_display_oplus_set_power(struct drm_connector *connector,
		int power_mode, void *disp)
{
	struct dsi_display *display = disp;
	int rc = 0;
	struct msm_drm_notifier notifier_data;
	int blank;
	bool notify_off = false;

	if (!display || !display->panel) {
		pr_err("invalid display/panel\n");
		return -EINVAL;
	}

#if defined(OPLUS_FEATURE_PXLW_IRIS5)
	if (iris_get_feature() && NULL != display->display_type && !strcmp(display->display_type, "secondary")) {
		//pr_err("%s return\n", __func__);
		return rc;
	}
#endif

	LCD_INFO("<%s> power_mode : %d\n", display->panel->oplus_priv.vendor_name, power_mode);

	if (power_mode == SDE_MODE_DPMS_OFF)
		atomic_set(&display->panel->esd_pending, 1);

	switch (power_mode) {
	case SDE_MODE_DPMS_LP1:
	case SDE_MODE_DPMS_LP2:
		if (power_mode == SDE_MODE_DPMS_LP1 &&
		    display->panel->power_mode == SDE_MODE_DPMS_ON)
			notify_off = true;

		memset(&notifier_data, 0, sizeof(notifier_data));

		if (notify_off) {
			blank = MSM_DRM_BLANK_POWERDOWN;
			notifier_data.data = &blank;
			notifier_data.id = 0;

			msm_drm_notifier_call_chain(MSM_DRM_EARLY_EVENT_BLANK,
					&notifier_data);
		}

		switch(get_oplus_display_scene()) {
		case OPLUS_DISPLAY_NORMAL_SCENE:
		case OPLUS_DISPLAY_NORMAL_HBM_SCENE:
			rc = dsi_panel_set_lp1(display->panel);
			rc = dsi_panel_set_lp2(display->panel);
			set_oplus_display_scene(OPLUS_DISPLAY_AOD_SCENE);
			break;
		case OPLUS_DISPLAY_AOD_HBM_SCENE:
			/* Skip aod off if fingerprintpress exist */
			if (!sde_crtc_get_fingerprint_pressed(connector->state->crtc->state)) {
				mutex_lock(&display->panel->panel_lock);
				dsi_display_clk_ctrl(display->dsi_clk_handle,
						     DSI_CORE_CLK, DSI_CLK_ON);
				if (display->panel->panel_initialized) {
					rc = dsi_panel_tx_cmd_set(display->panel, DSI_CMD_AOD_HBM_OFF);
					mutex_unlock(&display->panel->panel_lock);
					mutex_lock(&display->panel->panel_lock);
					oplus_update_aod_light_mode_unlock(display->panel);
				} else {
					pr_err("[%s][%d]failed to setting dsi command", __func__, __LINE__);
				}
				dsi_display_clk_ctrl(display->dsi_clk_handle,
						     DSI_CORE_CLK, DSI_CLK_OFF);
				mutex_unlock(&display->panel->panel_lock);
				set_oplus_display_scene(OPLUS_DISPLAY_AOD_SCENE);
			}

			break;
		case OPLUS_DISPLAY_AOD_SCENE:
		default:
			break;
		}
		if (notify_off)
			msm_drm_notifier_call_chain(MSM_DRM_EVENT_BLANK,
					&notifier_data);

		set_oplus_display_power_status(OPLUS_DISPLAY_POWER_DOZE_SUSPEND);
		break;
	case SDE_MODE_DPMS_ON:
		blank = MSM_DRM_BLANK_UNBLANK;
		notifier_data.data = &blank;
		notifier_data.id = 0;
		msm_drm_notifier_call_chain(MSM_DRM_EARLY_EVENT_BLANK,
					   &notifier_data);
		if ((display->panel->power_mode == SDE_MODE_DPMS_LP1) ||
			(display->panel->power_mode == SDE_MODE_DPMS_LP2)) {
			if (sde_crtc_get_fingerprint_mode(connector->state->crtc->state)) {
				mutex_lock(&display->panel->panel_lock);
				dsi_display_clk_ctrl(display->dsi_clk_handle,
						     DSI_CORE_CLK, DSI_CLK_ON);
				if (display->panel->panel_initialized && !display->panel->is_hbm_enabled) {
					SDE_ATRACE_BEGIN("DSI_CMD_AOD_HBM_ON2");
					rc = dsi_panel_tx_cmd_set(display->panel, DSI_CMD_AOD_HBM_ON);
					SDE_ATRACE_END("DSI_CMD_AOD_HBM_ON2");
				} else {
					pr_err("[%s][%d]failed to setting dsi command", __func__, __LINE__);
				}
				dsi_display_clk_ctrl(display->dsi_clk_handle,
						     DSI_CORE_CLK, DSI_CLK_OFF);
				mutex_unlock(&display->panel->panel_lock);
				set_oplus_display_scene(OPLUS_DISPLAY_AOD_HBM_SCENE);
			} else {
				rc = dsi_panel_set_nolp(display->panel);
				set_oplus_display_scene(OPLUS_DISPLAY_NORMAL_SCENE);
			}
		}

		oplus_dsi_update_seed_mode();
		dsi_update_dynamic_osc_clock();
		oplus_dsi_update_spr_mode();
		set_oplus_display_power_status(OPLUS_DISPLAY_POWER_ON);
		msm_drm_notifier_call_chain(MSM_DRM_EVENT_BLANK,
					    &notifier_data);
		break;
	case SDE_MODE_DPMS_OFF:
	default:
		return rc;
	}

	DSI_DEBUG("Power mode transition from %d to %d %s",
			display->panel->power_mode, power_mode,
			rc ? "failed" : "successful");
	if (!rc)
		display->panel->power_mode = power_mode;

	return rc;
}


static int oplus_display_find_vreg_by_name(const char *name)
{
	int count = 0, i =0;
	struct dsi_vreg *vreg = NULL;
	struct dsi_regulator_info *dsi_reg = NULL;
	struct dsi_display *display = get_main_display();

	if (!display)
		return -ENODEV;

	if (!display->panel )
		return -EINVAL;

	dsi_reg = &display->panel->power_info;
        count = dsi_reg->count;
	for( i =0; i < count; i++ ){
		vreg = &dsi_reg->vregs[i];
		pr_err("%s : find  %s",__func__, vreg->vreg_name);
		if (!strcmp(vreg->vreg_name, name)) {
			pr_err("%s : find the vreg %s",__func__, name);
			return i;
		}else {
			continue;
		}
	}
	pr_err("%s : dose not find the vreg [%s]",__func__, name);

	return -EINVAL;
}

static u32 update_current_voltage(u32 id)
{
	int vol_current = 0, pwr_id = 0;
	struct dsi_vreg *dsi_reg = NULL;
	struct dsi_regulator_info *dsi_reg_info = NULL;
	struct dsi_display *display = get_main_display();

	if (!display)
		return -ENODEV;
	if (!display->panel || !display->drm_conn)
		return -EINVAL;

	dsi_reg_info = &display->panel->power_info;
	pwr_id = oplus_display_find_vreg_by_name(panel_vol_bak[id].pwr_name);
	if (pwr_id < 0)
	{
		pr_err("%s: can't find the pwr_id, please check the vreg name\n",__func__);
		return pwr_id;
	}

	dsi_reg = &dsi_reg_info->vregs[pwr_id];
	if (!dsi_reg)
		return -EINVAL;

	vol_current = regulator_get_voltage(dsi_reg->vreg);

	return vol_current;
}

static ssize_t oplus_display_get_panel_pwr(struct device *dev,
	struct device_attribute *attr, char *buf) {

	int ret = 0;
	u32 i = 0;

	for (i = 0; i < (PANEL_VOLTAGE_ID_MAX-1); i++)
	{
		ret = update_current_voltage(panel_vol_bak[i].voltage_id);

		if (ret < 0)
			pr_err("%s : update_current_voltage error = %d\n", __func__, ret);
		else
			panel_vol_bak[i].voltage_current = ret;
	}
	return sprintf(buf, "%d %d %d %d %d %d %d %d %d %d %d %d\n",
		panel_vol_bak[0].voltage_id,panel_vol_bak[0].voltage_min,
		panel_vol_bak[0].voltage_current,panel_vol_bak[0].voltage_max,
		panel_vol_bak[1].voltage_id,panel_vol_bak[1].voltage_min,
		panel_vol_bak[1].voltage_current,panel_vol_bak[1].voltage_max,
		panel_vol_bak[2].voltage_id,panel_vol_bak[2].voltage_min,
		panel_vol_bak[2].voltage_current,panel_vol_bak[2].voltage_max);
}

static ssize_t oplus_display_set_panel_pwr(struct device *dev,
                               struct device_attribute *attr,
                               const char *buf, size_t count)
{
	int panel_vol_value = 0, rc = 0, panel_vol_id = 0, pwr_id = 0;
	struct dsi_vreg *dsi_reg = NULL;
	struct dsi_regulator_info *dsi_reg_info = NULL;
	struct dsi_display *display = get_main_display();

	sscanf(buf, "%d %d", &panel_vol_id, &panel_vol_value);
	panel_vol_id = panel_vol_id & 0x0F;

	pr_err("debug for %s, buf = [%s], id = %d value = %d, count = %d\n",
		__func__, buf, panel_vol_id, panel_vol_value,count);

	if (panel_vol_id < 0 || panel_vol_id > PANEL_VOLTAGE_ID_MAX)
		return -EINVAL;

	if (panel_vol_value < panel_vol_bak[panel_vol_id].voltage_min ||
		panel_vol_id > panel_vol_bak[panel_vol_id].voltage_max)
		return -EINVAL;

	if (!display)
		return -ENODEV;
	if (!display->panel || !display->drm_conn) {
		return -EINVAL;
	}

	if (panel_vol_id == PANEL_VOLTAGE_ID_VG_BASE)
	{
		pr_err("%s: set the VGH_L pwr = %d \n",__func__,panel_vol_value );
		panel_pwr_vg_base = panel_vol_value;
		return count;
	}

	dsi_reg_info = &display->panel->power_info;

	pwr_id = oplus_display_find_vreg_by_name(panel_vol_bak[panel_vol_id].pwr_name);
	if (pwr_id < 0)
	{
		pr_err("%s: can't find the vreg name, please re-check vreg name: %s \n",__func__,
                       panel_vol_bak[panel_vol_id].pwr_name);
		return pwr_id;
	}
	dsi_reg = &dsi_reg_info->vregs[pwr_id];

	rc = regulator_set_voltage(dsi_reg->vreg, panel_vol_value, panel_vol_value);
	if (rc) {
		pr_err("Set voltage(%s) fail, rc=%d\n",
			 dsi_reg->vreg_name, rc);
		return -EINVAL;
	}

	return count;
}

#ifdef OPLUS_FEATURE_AOD_RAMLESS
/*
struct aod_area {
	bool enable;
	int x;
	int y;
	int w;
	int h;
	int color;
	int bitdepth;
	int mono;
	int gray;
};

#define RAMLESS_AOD_AREA_NUM		6
#define RAMLESS_AOD_PAYLOAD_SIZE	100
static struct aod_area oplus_aod_area[RAMLESS_AOD_AREA_NUM];
*/
struct aod_area oplus_aod_area[RAMLESS_AOD_AREA_NUM];

int oplus_display_update_aod_area_unlock(void)
{
	struct dsi_display *display = get_main_display();
	struct mipi_dsi_device *mipi_device;
	char payload[RAMLESS_AOD_PAYLOAD_SIZE];
	int rc = 0;
	int i;

	if (!display || !display->panel || !display->panel->oplus_priv.is_aod_ramless)
		return 0;

	if (!dsi_panel_initialized(display->panel))
		return -EINVAL;

	mipi_device = &display->panel->mipi_device;

	/* enable the clk vote for CMD mode panels */
	if (display->config.panel_mode == DSI_OP_CMD_MODE) {
		dsi_display_clk_ctrl(display->dsi_clk_handle,
				DSI_CORE_CLK, DSI_CLK_ON);
	}

	memset(payload, 0, RAMLESS_AOD_PAYLOAD_SIZE);

	for (i = 0; i < RAMLESS_AOD_AREA_NUM; i++) {
		struct aod_area *area = &oplus_aod_area[i];

		payload[0] |= (!!area->enable) << (RAMLESS_AOD_AREA_NUM - i - 1);
		if (area->enable) {
			int h_start = area->x;
			int h_block = area->w / 100;
			int v_start = area->y;
			int v_end = area->y + area->h;
			int off = i * 5;

			/* Rect Setting */
			payload[1 + off] = h_start >> 4;
			payload[2 + off] = ((h_start & 0xf) << 4) | (h_block & 0xf);
			payload[3 + off] = v_start >> 4;
			payload[4 + off] = ((v_start & 0xf) << 4) | ((v_end >> 8) & 0xf);
			payload[5 + off] = v_end & 0xff;

			/* Mono Setting */
			#define SET_MONO_SEL(index, shift) \
			if (i == index) \
			payload[31] |= area->mono << shift;

			SET_MONO_SEL(0, 6);
			SET_MONO_SEL(1, 5);
			SET_MONO_SEL(2, 4);
			SET_MONO_SEL(3, 2);
			SET_MONO_SEL(4, 1);
			SET_MONO_SEL(5, 0);
			#undef SET_MONO_SEL

			/* Depth Setting */
			if (i < 4)
				payload[32] |= (area->bitdepth & 0x3) << ((3 - i) * 2);
			else if (i == 4)
				payload[33] |= (area->bitdepth & 0x3) << 6;
			else if (i == 5)
				payload[33] |= (area->bitdepth & 0x3) << 4;

			/* Color Setting */
			#define SET_COLOR_SEL(index, reg, shift) \
			if (i == index) \
			payload[reg] |= (area->color & 0x7) << shift;
			SET_COLOR_SEL(0, 34, 4);
			SET_COLOR_SEL(1, 34, 0);
			SET_COLOR_SEL(2, 35, 4);
			SET_COLOR_SEL(3, 35, 0);
			SET_COLOR_SEL(4, 36, 4);
			SET_COLOR_SEL(5, 36, 0);
			#undef SET_COLOR_SEL

			/* Area Gray Setting */
			payload[37 + i] = area->gray & 0xff;
		}
	}
	payload[43] = 0x00;

	rc = mipi_dsi_dcs_write(mipi_device, 0x81, payload, 43);
	pr_err("dsi_cmd aod_area[%x] updated \n", payload[0]);


	if (display->config.panel_mode == DSI_OP_CMD_MODE) {
		rc = dsi_display_clk_ctrl(display->dsi_clk_handle,
				DSI_CORE_CLK, DSI_CLK_OFF);
	}

	return 0;
}

static ssize_t oplus_display_get_aod_area(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct dsi_display *display = get_main_display();
	int i, cnt = 0;

	if (!display || !display->panel || !display->panel->oplus_priv.is_aod_ramless)
		return -EINVAL;

	mutex_lock(&display->display_lock);
	mutex_lock(&display->panel->panel_lock);

	cnt = snprintf(buf, PAGE_SIZE, "aod_area info:\n");
	for (i = 0; i < RAMLESS_AOD_AREA_NUM; i++) {
		struct aod_area *area = &oplus_aod_area[i];

		if (area->enable) {
			cnt += snprintf(buf + cnt, PAGE_SIZE,
					"    area[%d]: [%dx%d]-[%dx%d]-%d-%d-%d-%d\n",
					cnt, area->x, area->y, area->w, area->h,
					area->color, area->bitdepth, area->mono, area->gray);
		}
	}

	cnt += snprintf(buf + cnt, PAGE_SIZE, "aod_area raw:\n");
	for (i = 0; i < RAMLESS_AOD_AREA_NUM; i++) {
		struct aod_area *area = &oplus_aod_area[i];

		if (area->enable) {
			cnt += snprintf(buf + cnt, PAGE_SIZE,
					"%d %d %d %d %d %d %d %d",
					area->x, area->y, area->w, area->h,
					area->color, area->bitdepth, area->mono, area->gray);
		}
		cnt += snprintf(buf + cnt, PAGE_SIZE, ":");
	}
	cnt += snprintf(buf + cnt, PAGE_SIZE, "aod_area end\n");

	mutex_unlock(&display->panel->panel_lock);
	mutex_unlock(&display->display_lock);

	return cnt;
}

static ssize_t oplus_display_set_aod_area(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count) {
	struct dsi_display *display = get_main_display();
	char *bufp = (char *)buf;
	char *token;
	int cnt = 0;

	if (!display || !display->panel || !display->panel->oplus_priv.is_aod_ramless) {
		pr_err("failed to find dsi display or is not ramless\n");
		return false;
	}

	mutex_lock(&display->display_lock);
	mutex_lock(&display->panel->panel_lock);

	memset(oplus_aod_area, 0, sizeof(struct aod_area) * RAMLESS_AOD_AREA_NUM);

	while ((token = strsep(&bufp, ":")) != NULL) {
		struct aod_area *area = &oplus_aod_area[cnt];
		if (!*token)
			continue;

		sscanf(token, "%d %d %d %d %d %d %d %d",
				&area->x, &area->y, &area->w, &area->h,
				&area->color, &area->bitdepth, &area->mono, &area->gray);
		pr_err("aod_area[%d]: [%dx%d]-[%dx%d]-%d-%d-%d-%d\n",
				cnt, area->x, area->y, area->w, area->h,
				area->color, area->bitdepth, area->mono, area->gray);
		area->enable = true;
		cnt++;
	}
	oplus_display_update_aod_area_unlock();
	mutex_unlock(&display->panel->panel_lock);
	mutex_unlock(&display->display_lock);

	return count;
}

static ssize_t oplus_display_get_video(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct dsi_display *display = get_main_display();
	bool is_aod_ramless = false;

	if (display && display->panel && display->panel->oplus_priv.is_aod_ramless)
		is_aod_ramless = true;

	return sprintf(buf, "%d\n", is_aod_ramless ? 1 : 0);
}

static ssize_t oplus_display_set_video(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count) {
	struct dsi_display *display = get_main_display();
	struct drm_device *drm_dev;
	struct drm_connector *dsi_connector;
	struct drm_mode_config *mode_config;
	struct msm_drm_private *priv;
	struct drm_display_mode *mode;
	struct drm_atomic_state *state;
	struct drm_crtc_state *crtc_state;
	struct drm_crtc *crtc;
	bool mode_changed = false;
	int mode_id = 0;
	int vblank_get = -EINVAL;
	int err = 0;
	int i;

	if (!display || !display->panel) {
		pr_err("failed to find dsi display\n");
		return -EINVAL;
	}

	drm_dev = display->drm_dev;
	dsi_connector = display->drm_conn;
	mode_config = &drm_dev->mode_config;
	priv = drm_dev->dev_private;

	if (!display->panel->oplus_priv.is_aod_ramless)
		return count;

	if (!dsi_connector || !dsi_connector->state || !dsi_connector->state->crtc) {
		pr_err("[%s]: display not ready\n", __func__);
		return count;
	}

	sscanf(buf, "%du", &mode_id);
	pr_err("setting display mode %d\n", mode_id);

	vblank_get = drm_crtc_vblank_get(dsi_connector->state->crtc);
	if (vblank_get) {
		pr_err("failed to get crtc vblank\n", vblank_get);
	}

	drm_modeset_lock_all(drm_dev);

	if (oplus_display_mode != 1)
		display->panel->dyn_clk_caps.dyn_clk_support = false;

	state = drm_atomic_state_alloc(drm_dev);
	if (!state)
		goto error;

	oplus_display_mode = mode_id;
	state->acquire_ctx = mode_config->acquire_ctx;
	crtc = dsi_connector->state->crtc;
	crtc_state = drm_atomic_get_crtc_state(state, crtc);

	 {
		struct drm_display_mode *set_mode = NULL;
		struct drm_display_mode *cmd_mode = NULL;
		struct drm_display_mode *vid_mode = NULL;

		list_for_each_entry(mode, &dsi_connector->modes, head) {
			if (drm_mode_vrefresh(mode) == 0)
				continue;
			if (mode->flags & DRM_MODE_FLAG_VID_MODE_PANEL)
				vid_mode = mode;
			if (mode->flags & DRM_MODE_FLAG_CMD_MODE_PANEL)
				cmd_mode = mode;
		}

		set_mode = oplus_display_mode ? vid_mode : cmd_mode;
		set_mode = oplus_onscreenfp_status ? vid_mode : set_mode;

		if (set_mode && drm_mode_vrefresh(set_mode) != drm_mode_vrefresh(&crtc_state->mode)) {
			mode_changed = true;
		} else {
			mode_changed = false;
		}

		if (mode_changed) {
			for (i = 0; i < priv->num_crtcs; i++) {
				if (priv->disp_thread[i].crtc_id == crtc->base.id) {
					if (priv->disp_thread[i].thread)
						kthread_flush_worker(&priv->disp_thread[i].worker);
				}
			}

			display->panel->dyn_clk_caps.dyn_clk_support = false;
			drm_atomic_set_mode_for_crtc(crtc_state, set_mode);
		}
		wake_up(&oplus_aod_wait);
	}
	err = drm_atomic_commit(state);
	drm_atomic_state_put(state);

	if (mode_changed) {
		for (i = 0; i < priv->num_crtcs; i++) {
			if (priv->disp_thread[i].crtc_id == crtc->base.id) {
				if (priv->disp_thread[i].thread)
					kthread_flush_worker(&priv->disp_thread[i].worker);
			}
		}
	}

	if (oplus_display_mode == 1)
		display->panel->dyn_clk_caps.dyn_clk_support = true;

error:
	drm_modeset_unlock_all(drm_dev);
	if (!vblank_get)
		drm_crtc_vblank_put(dsi_connector->state->crtc);

	return count;
}
#endif /* OPLUS_FEATURE_AOD_RAMLESS */

/*#ifdef OPLUS_BUG_STABILITY*/
static ssize_t oplus_display_get_dsi_log_switch(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "\
		dynamic conctrl debug log, 0x0 --> disable all debug log\n \
		1 -> enable  0-> disable\n \
		BIT(0) --> dump register log\n \
		BIT(1) --> backlight log\n \
		BIT(2) --> common log\n \
		current velue:0x%x\n", oplus_dsi_log_type);
}

static ssize_t oplus_display_set_dsi_log_switch(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	sscanf(buf, "%d", &oplus_dsi_log_type);
	pr_err("debug for %s, buf = [%s], oplus_dsi_log_type = %d , count = %d\n",
				__func__, buf, oplus_dsi_log_type, count);

	return count;
}
/*#endif*/

static ssize_t oplus_display_get_panel_cabc(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	uint32_t cabc_mode = 0;
	oplus_display_panel_get_cabc(&cabc_mode);
	pr_info("%s: cabc_mode=%d\n", __func__, cabc_mode);
	return sprintf(buf, "%d\n", cabc_mode);
}

static ssize_t oplus_display_set_panel_cabc(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	uint32_t temp_save = 0;
	int rc = 0;

	sscanf(buf, "%du", &temp_save);
	pr_info("%s: to set cabc_mode=%d\n", __func__, temp_save);
	rc = oplus_display_panel_set_cabc(&temp_save);
	if (rc) {
		pr_err("%s: Not to config cabc_mode=%d", __func__, temp_save);
		return -EINVAL;
	}

	return count;
}

static struct kobject *oplus_display_kobj;

static DEVICE_ATTR(hbm, S_IRUGO|S_IWUSR, oplus_display_get_hbm, oplus_display_set_hbm);
static DEVICE_ATTR(audio_ready, S_IRUGO|S_IWUSR, NULL, oplus_display_set_audio_ready);
static DEVICE_ATTR(seed, S_IRUGO|S_IWUSR, oplus_display_get_seed, oplus_display_set_seed);
static DEVICE_ATTR(panel_serial_number, S_IRUGO|S_IWUSR, oplus_display_get_panel_serial_number, NULL);
static DEVICE_ATTR(dump_info, S_IRUGO|S_IWUSR, oplus_display_dump_info, NULL);
static DEVICE_ATTR(panel_dsc, S_IRUGO|S_IWUSR, oplus_display_get_panel_dsc, NULL);
static DEVICE_ATTR(power_status, S_IRUGO|S_IWUSR, oplus_display_get_power_status, oplus_display_set_power_status);
static DEVICE_ATTR(display_regulator_control, S_IRUGO|S_IWUSR, NULL, oplus_display_regulator_control);
static DEVICE_ATTR(panel_id, S_IRUGO|S_IWUSR, oplus_display_get_panel_id, NULL);
static DEVICE_ATTR(sau_closebl_node, S_IRUGO|S_IWUSR, oplus_display_get_closebl_flag, oplus_display_set_closebl_flag);
static DEVICE_ATTR(write_panel_reg, S_IRUGO|S_IWUSR, oplus_display_get_panel_reg, oplus_display_set_panel_reg);
static DEVICE_ATTR(dsi_cmd, S_IRUGO|S_IWUSR, oplus_display_get_dsi_command, oplus_display_set_dsi_command);
static DEVICE_ATTR(dim_alpha, S_IRUGO|S_IWUSR, oplus_display_get_dim_alpha, oplus_display_set_dim_alpha);
static DEVICE_ATTR(dim_dc_alpha, S_IRUGO|S_IWUSR, oplus_display_get_dc_dim_alpha, oplus_display_set_dim_alpha);
static DEVICE_ATTR(dimlayer_hbm, S_IRUGO|S_IWUSR, oplus_display_get_dimlayer_hbm, oplus_display_set_dimlayer_hbm);
static DEVICE_ATTR(dimlayer_bl_en, S_IRUGO|S_IWUSR, oplus_display_get_dimlayer_enable, oplus_display_set_dimlayer_enable);
static DEVICE_ATTR(dimlayer_set_bl, S_IRUGO|S_IWUSR, oplus_display_get_dimlayer_backlight, oplus_display_set_dimlayer_backlight);
static DEVICE_ATTR(debug, S_IRUGO|S_IWUSR, oplus_display_get_debug, oplus_display_set_debug);
static DEVICE_ATTR(force_screenfp, S_IRUGO|S_IWUSR, oplus_display_get_forcescreenfp, oplus_display_set_forcescreenfp);
static DEVICE_ATTR(esd_status, S_IRUGO|S_IWUSR, oplus_display_get_esd_status, oplus_display_set_esd_status);
static DEVICE_ATTR(notify_panel_blank, S_IRUGO|S_IWUSR, NULL, oplus_display_notify_panel_blank);
static DEVICE_ATTR(ffl_set, S_IRUGO|S_IWUSR, oplus_get_ffl_setting, oplus_set_ffl_setting);
static DEVICE_ATTR(notify_fppress, S_IRUGO|S_IWUSR, NULL, oplus_display_notify_fp_press);
static DEVICE_ATTR(aod_light_mode_set, S_IRUGO|S_IWUSR, oplus_get_aod_light_mode, oplus_set_aod_light_mode);
static DEVICE_ATTR(spr, S_IRUGO|S_IWUSR, oplus_display_get_spr, oplus_display_set_spr);
static DEVICE_ATTR(roundcorner, S_IRUGO|S_IRUSR, oplus_display_get_roundcorner, NULL);
static DEVICE_ATTR(dynamic_osc_clock, S_IRUGO|S_IWUSR, oplus_display_get_dynamic_osc_clock, oplus_display_set_dynamic_osc_clock);
static DEVICE_ATTR(max_brightness, S_IRUGO|S_IWUSR, oplus_display_get_max_brightness, oplus_display_set_max_brightness);
static DEVICE_ATTR(ccd_check, S_IRUGO|S_IRUSR, oplus_display_get_ccd_check, NULL);
static DEVICE_ATTR(iris_rm_check, S_IRUGO|S_IWUSR, oplus_display_get_iris_state, NULL);
static DEVICE_ATTR(panel_pwr, S_IRUGO|S_IWUSR, oplus_display_get_panel_pwr, oplus_display_set_panel_pwr);
#ifdef OPLUS_FEATURE_AOD_RAMLESS
static DEVICE_ATTR(aod_area, S_IRUGO|S_IWUSR, oplus_display_get_aod_area, oplus_display_set_aod_area);
static DEVICE_ATTR(video, S_IRUGO|S_IWUSR, oplus_display_get_video, oplus_display_set_video);
#endif /* OPLUS_FEATURE_AOD_RAMLESS */

#ifdef OPLUS_FEATURE_ADFR
static DEVICE_ATTR(adfr_debug, S_IRUGO|S_IWUSR, oplus_adfr_get_debug, oplus_adfr_set_debug);
static DEVICE_ATTR(vsync_switch, S_IRUGO|S_IWUSR, oplus_get_vsync_switch, oplus_set_vsync_switch);
#endif

/*#ifdef OPLUS_BUG_STABILITY*/
static DEVICE_ATTR(dsi_log_switch, S_IRUGO | S_IWUSR, oplus_display_get_dsi_log_switch, oplus_display_set_dsi_log_switch);
/*#endif*/

static DEVICE_ATTR(LCM_CABC, S_IRUGO|S_IWUSR, oplus_display_get_panel_cabc, oplus_display_set_panel_cabc);
/* fp type config */
static DEVICE_ATTR(fp_type, S_IRUGO|S_IWUSR, oplus_ofp_get_fp_type_attr, oplus_ofp_set_fp_type_attr);
/*
 * Create a group of attributes so that we can create and destroy them all
 * at once.
 */
static struct attribute *oplus_display_attrs[] = {
	&dev_attr_hbm.attr,
	&dev_attr_audio_ready.attr,
	&dev_attr_seed.attr,
	&dev_attr_panel_serial_number.attr,
	&dev_attr_dump_info.attr,
	&dev_attr_panel_dsc.attr,
	&dev_attr_power_status.attr,
	&dev_attr_display_regulator_control.attr,
	&dev_attr_panel_id.attr,
	&dev_attr_sau_closebl_node.attr,
	&dev_attr_write_panel_reg.attr,
	&dev_attr_dsi_cmd.attr,
	&dev_attr_dim_alpha.attr,
	&dev_attr_dim_dc_alpha.attr,
	&dev_attr_dimlayer_hbm.attr,
	&dev_attr_dimlayer_set_bl.attr,
	&dev_attr_dimlayer_bl_en.attr,
	&dev_attr_debug.attr,
	&dev_attr_force_screenfp.attr,
	&dev_attr_esd_status.attr,
	&dev_attr_notify_panel_blank.attr,
	&dev_attr_ffl_set.attr,
	&dev_attr_notify_fppress.attr,
	&dev_attr_aod_light_mode_set.attr,
	&dev_attr_spr.attr,
	&dev_attr_roundcorner.attr,
	&dev_attr_dynamic_osc_clock.attr,
	&dev_attr_max_brightness.attr,
	&dev_attr_ccd_check.attr,
	&dev_attr_iris_rm_check.attr,
	&dev_attr_panel_pwr.attr,
#ifdef OPLUS_FEATURE_AOD_RAMLESS
	&dev_attr_aod_area.attr,
	&dev_attr_video.attr,
#endif /* OPLUS_FEATURE_AOD_RAMLESS */

#ifdef OPLUS_FEATURE_ADFR
	&dev_attr_adfr_debug.attr,
	&dev_attr_vsync_switch.attr,
#endif

/*#ifdef OPLUS_BUG_STABILITY*/
	&dev_attr_dsi_log_switch.attr,
/*#endif*/
	&dev_attr_LCM_CABC.attr,
	/* fp type config */
	&dev_attr_fp_type.attr,
	NULL,	/* need to NULL terminate the list of attributes */
};

static struct attribute_group oplus_display_attr_group = {
	.attrs = oplus_display_attrs,
};

/*
 * Create a new API to get display resolution
 */
int oplus_display_get_resolution(unsigned int *xres, unsigned int *yres)
{
	*xres = *yres = 0;
	if (get_main_display() && get_main_display()->modes) {
		*xres = get_main_display()->modes->timing.v_active;
		*yres = get_main_display()->modes->timing.h_active;
	}
	return 0;
}
EXPORT_SYMBOL(oplus_display_get_resolution);

static int __init oplus_display_private_api_init(void)
{
	struct dsi_display *display = get_main_display();
	int retval;

	if (!display)
		return -EPROBE_DEFER;
	oplus_display_kobj = kobject_create_and_add("oplus_display", kernel_kobj);
	if (!oplus_display_kobj)
		return -ENOMEM;

	/* Create the files associated with this kobject */
	retval = sysfs_create_group(oplus_display_kobj, &oplus_display_attr_group);
	if (retval)
		goto error_remove_kobj;

	retval = sysfs_create_link(oplus_display_kobj,
				   &display->pdev->dev.kobj, "panel");
	if (retval)
		goto error_remove_sysfs_group;

	if(oplus_ffl_thread_init())
		pr_err("fail to init oplus_ffl_thread\n");



	return 0;

error_remove_sysfs_group:
	sysfs_remove_group(oplus_display_kobj, &oplus_display_attr_group);
error_remove_kobj:
	kobject_put(oplus_display_kobj);
	oplus_display_kobj = NULL;

	return retval;
}

static void __exit oplus_display_private_api_exit(void)
{
	oplus_ffl_thread_exit();

	sysfs_remove_link(oplus_display_kobj, "panel");
	sysfs_remove_group(oplus_display_kobj, &oplus_display_attr_group);
	kobject_put(oplus_display_kobj);
}

module_init(oplus_display_private_api_init);
module_exit(oplus_display_private_api_exit);
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Hujie <hujie@oplus.com>");

/*
 * Copyright (C) 2018 CopuLab Ltd.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/i2c.h>
#include <linux/device.h>
#include <linux/gpio/consumer.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_graph.h>
#include <linux/slab.h>
#include <linux/workqueue.h>

#include <drm/drmP.h>
#include <drm/drm_atomic.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_edid.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_connector.h>
#include <video/mipi_display.h>
#include <video/of_videomode.h>
#include <video/videomode.h>

#include "sn65dsi86_brg.h"

/* Register Address */
#define  SN65DSI86_DEVICE_ID            0x00
#define  SN65DSI86_DEVICE_REV           0x08
#define  SN65DSI86_SOFT_RESET           0x09
#define  SN65DSI86_PLL_REFCLK_CFG       0x0A
#define  SN65DSI86_PLL_EN           0x0D
#define  SN65DSI86_DSI_CFG1         0x10
#define  SN65DSI86_DSI_CFG2         0x11
#define  SN65DSI86_DSI_CHA_CLK_RANGE        0x12
#define  SN65DSI86_DSI_CHB_CLK_RANGE        0x13
#define SN65DSI86_PAGE_SELECT           0x16
#define  SN65DSI86_VIDEO_CHA_LINE_LOW       0x20
#define  SN65DSI86_VIDEO_CHA_LINE_HIGH      0x21
#define  SN65DSI86_VIDEO_CHB_LINE_LOW       0x22
#define  SN65DSI86_VIDEO_CHB_LINE_HIGH      0x23
#define  SN65DSI86_CHA_VERT_DISP_SIZE_LOW   0x24
#define  SN65DSI86_CHA_VERT_DISP_SIZE_HIGH  0x25
#define  SN65DSI86_CHA_HSYNC_PULSE_WIDTH_LOW    0x2C
#define  SN65DSI86_CHA_HSYNC_PULSE_WIDTH_HIGH   0x2D
#define  SN65DSI86_CHA_VSYNC_PULSE_WIDTH_LOW    0x30
#define  SN65DSI86_CHA_VSYNC_PULSE_WIDTH_HIGH   0x31
#define  SN65DSI86_CHA_HORIZONTAL_BACK_PORCH    0x34
#define  SN65DSI86_CHA_VERTICAL_BACK_PORCH  0x36
#define  SN65DSI86_CHA_HORIZONTAL_FRONT_PORCH   0x38
#define  SN65DSI86_CHA_VERTICAL_FRONT_PORCH 0x3a
#define  SN65DSI86_COLOR_BAR_CFG        0x3c
#define  SN65DSI86_FRAMING_CFG          0x5a
#define  SN65DSI86_DP_24BPP_EN          0x5b
#define  SN65DSI86_DP_HPD_DISABLE       0x5c
#define  SN65DSI86_GPIO_CTRL_CFG        0x5f
#define  SN65DSI86_DP_SSC_CFG           0x93
#define  SN65DSI86_DP_CFG           0x94
#define  SN65DSI86_TRAINING_CFG         0x95
#define  SN65DSI86_ML_TX_MODE           0x96
#define  SN65DSI86_CHA_ERR               0xF0
#define  SN65DSI86_IRQ_STATUS            0xF5
#define  SN65DSI86_ASS_RW_CONTROL       0xFF

#define SN65DSI86_HPD_IRQ_BIT       0x01
#define SN65DSI86_HPD_INSERTION_BIT 0x02
#define SN65DSI86_HPD_REMOVAL_BIT   0x04
#define SN65DSI86_HPD_REPLUG_BIT    0x08
#define SN65DSI86_LT_PASS_BIT 0x01

#define DUMP_IRQ_STATUS 0
#define TIME_ELAPSE_HPD_WORKQUEUE   100
#define TIMEOUT_HPD_UPDATE  (2000 / TIME_ELAPSE_HPD_WORKQUEUE)

struct workqueue_struct *workqueue_hpd = NULL;

struct work_struct work_hpd;

struct sn65dsi86_brg *g_brg = NULL;
void work_hpd_detect_handler(struct work_struct *work);

static int sn65dsi86_brg_power_on(struct sn65dsi86_brg *brg)
{
    dev_dbg(&brg->client->dev,"%s\n",__func__);
    if(workqueue_hpd == NULL){
        g_brg = brg;
        g_brg->hpd_debuncing_count = 0;
        g_brg->hpd_debuncing_status = 0xFF;
        workqueue_hpd = alloc_workqueue("workqueue_hpd", 0, 0);

        INIT_WORK(&work_hpd, work_hpd_detect_handler);

        queue_work(workqueue_hpd, &work_hpd);
    }
    return 0;
}

static void sn65dsi86_brg_power_off(struct sn65dsi86_brg *brg)
{
    dev_dbg(&brg->client->dev,"%s\n",__func__);
}

static int sn65dsi86_write(struct i2c_client *client, u8 reg, u8 val)
{
    int ret;

    ret = i2c_smbus_write_byte_data(client, reg, val);

    if (ret)
        dev_err(&client->dev, "failed to write at 0x%02x", reg);

    dev_dbg(&client->dev, "%s: write reg 0x%02x data 0x%02x", __func__, reg, val);

    return ret;
}
#define SN65DSI86_WRITE(reg,val) sn65dsi86_write(client, (reg) , (val))

static int sn65dsi86_read(struct i2c_client *client, u8 reg)
{
    int ret;

    dev_dbg(&client->dev, "client 0x%p", client);
    ret = i2c_smbus_read_byte_data(client, reg);

    if (ret < 0) {
        dev_err(&client->dev, "failed reading at 0x%02x", reg);
        return ret;
    }

    dev_dbg(&client->dev, "%s: read reg 0x%02x data 0x%02x", __func__, reg, ret);

    return ret;
}
#define SN65DSI86_READ(reg) sn65dsi86_read(client, (reg))

static int sn65dsi86_brg_start_stream(struct sn65dsi86_brg *brg)
{
#if DUMP_IRQ_STATUS
    int regval;
#endif
    struct i2c_client *client = I2C_CLIENT(brg);

    dev_dbg(&client->dev,"%s +\n",__func__);
    /* Set the PLL_EN bit (CSR 0x0D.0) */
    SN65DSI86_WRITE(SN65DSI86_PLL_EN, 0x1);
    /* Wait for the PLL_LOCK bit to be set (CSR 0x0A.7) */
    msleep(100);

    /* Perform SW reset to apply changes */
    SN65DSI86_WRITE(SN65DSI86_SOFT_RESET, 0x01);

    /* Read CHA Error register */
#if DUMP_IRQ_STATUS
    regval = SN65DSI86_READ(SN65DSI86_CHA_ERR);
    dev_info(&client->dev, "CHA (0x%02x) = 0x%02x",
         SN65DSI86_CHA_ERR, regval);
    regval = SN65DSI86_READ(SN65DSI86_CHA_ERR + 1);
    dev_info(&client->dev, "CHA (0x%02x) = 0x%02x",
         SN65DSI86_CHA_ERR + 1, regval);
    regval = SN65DSI86_READ(SN65DSI86_CHA_ERR + 2);
    dev_info(&client->dev, "CHA (0x%02x) = 0x%02x",
         SN65DSI86_CHA_ERR + 2, regval);
    regval = SN65DSI86_READ(SN65DSI86_CHA_ERR + 3);
    dev_info(&client->dev, "CHA (0x%02x) = 0x%02x",
         SN65DSI86_CHA_ERR + 3, regval);
    regval = SN65DSI86_READ(SN65DSI86_CHA_ERR + 4);
    dev_info(&client->dev, "CHA (0x%02x) = 0x%02x",
         SN65DSI86_CHA_ERR + 4, regval);
    regval = SN65DSI86_READ(SN65DSI86_IRQ_STATUS);
    dev_info(&client->dev, "SN65DSI86_IRQ_STATUS (0x%02x) = 0x%02x",
         SN65DSI86_IRQ_STATUS, regval);
    regval = SN65DSI86_READ(SN65DSI86_CHA_ERR + 6);
    dev_info(&client->dev, "CHA (0x%02x) = 0x%02x",
         SN65DSI86_CHA_ERR + 6, regval);
    regval = SN65DSI86_READ(SN65DSI86_CHA_ERR + 7);
    dev_info(&client->dev, "CHA (0x%02x) = 0x%02x",
         SN65DSI86_CHA_ERR + 7, regval);
    regval = SN65DSI86_READ(SN65DSI86_CHA_ERR + 8);
    dev_info(&client->dev, "CHA (0x%02x) = 0x%02x",
         SN65DSI86_CHA_ERR + 8, regval);
#else
    usleep_range(10000, 12000);
#endif
    dev_dbg(&client->dev,"%s -\n",__func__);
    return 0;
}

static void sn65dsi86_brg_stop_stream(struct sn65dsi86_brg *brg)
{
    struct i2c_client *client = I2C_CLIENT(brg);
    dev_dbg(&client->dev,"%s\n",__func__);
    /* Clear the PLL_EN bit (CSR 0x0D.0) */
    SN65DSI86_WRITE(SN65DSI86_PLL_EN, 0x00);
}

static int sn65dsi86_brg_configure(struct sn65dsi86_brg *brg)
{
    int regval = 0;
    int i ;
    struct i2c_client *client = I2C_CLIENT(brg);
    dev_dbg(&client->dev,"%s +\n",__func__);
    /* Clear IRQ Status */
    SN65DSI86_WRITE(SN65DSI86_CHA_ERR, 0xFF);
    SN65DSI86_WRITE(SN65DSI86_CHA_ERR + 1, 0xFF);
    SN65DSI86_WRITE(SN65DSI86_CHA_ERR + 2, 0xFF);
    SN65DSI86_WRITE(SN65DSI86_CHA_ERR + 3, 0xFF);
    SN65DSI86_WRITE(SN65DSI86_CHA_ERR + 4, 0xFF);
    SN65DSI86_WRITE(SN65DSI86_IRQ_STATUS, 0xFF);
    SN65DSI86_WRITE(SN65DSI86_CHA_ERR + 6, 0xFF);
    SN65DSI86_WRITE(SN65DSI86_CHA_ERR + 7, 0xFF);
    SN65DSI86_WRITE(SN65DSI86_CHA_ERR + 8, 0xFF);
    usleep_range(10000, 12000);

    /* ASSR RW Control */
    SN65DSI86_WRITE(SN65DSI86_ASS_RW_CONTROL, 0x07);
    SN65DSI86_WRITE(SN65DSI86_PAGE_SELECT, 0x01);
    SN65DSI86_WRITE(SN65DSI86_ASS_RW_CONTROL, 0x00);
    usleep_range(10000, 12000);
    /* REFCLK 27MHz */
    SN65DSI86_WRITE(SN65DSI86_PLL_REFCLK_CFG, 0x06);
    usleep_range(10000, 12000);

    /* Single 4 DSI lanes */
    SN65DSI86_WRITE(SN65DSI86_DSI_CFG1, 0x26);
    usleep_range(10000, 12000);

    /* DSI CHA CLK FREQ 480MHz */
    SN65DSI86_WRITE(SN65DSI86_DSI_CHA_CLK_RANGE, 0x60);
    usleep_range(10000, 12000);

    /* DSI CHB CLK FREQ 480MHz */
    SN65DSI86_WRITE(SN65DSI86_DSI_CHB_CLK_RANGE, 0x60);
    usleep_range(10000, 12000);    

    /* L0mV HBR  2.7Gbps  Data Rate*/
    SN65DSI86_WRITE(SN65DSI86_DP_CFG, 0x80);
    usleep_range(10000, 12000);

    /* PLL ENABLE */
    SN65DSI86_WRITE(SN65DSI86_PLL_EN, 0x01);
    usleep_range(10000, 12000);
    /* DP_PLL_LOCK */
    for(i = 0; i < 10; i++){
        regval = SN65DSI86_READ(SN65DSI86_PLL_REFCLK_CFG);
        usleep_range(10000, 12000);
        dev_dbg(&client->dev, "SN65DSI86_PLL_REFCLK_CFG (0x%02x) = 0x%02x",
        SN65DSI86_PLL_REFCLK_CFG, regval);
        if(regval & 0x80){
            break;
        }
    }

    /* enhanced framing */
    SN65DSI86_WRITE(SN65DSI86_FRAMING_CFG, 0x04);
    usleep_range(10000, 12000);

    /* Pre0dB 2 lanes no SSC */
    SN65DSI86_WRITE(SN65DSI86_DP_SSC_CFG, 0x20);
    usleep_range(10000, 12000);

    /* Semi-Auto TRAIN */
    SN65DSI86_WRITE(SN65DSI86_ML_TX_MODE, 0x0A);
    usleep_range(10000, 12000);
    for(i = 0; i < 10; i++){
        regval = SN65DSI86_READ(SN65DSI86_CHA_ERR + 8);
        usleep_range(10000, 12000);
        dev_dbg(&client->dev, "LT_PASS (0x%02x) = 0x%02x", (SN65DSI86_CHA_ERR + 8), regval);
        if(regval & SN65DSI86_LT_PASS_BIT){
            break;
        }
    }

    /* CHA_ACTIVE_LINE_LENGTH */
    SN65DSI86_WRITE(SN65DSI86_VIDEO_CHA_LINE_LOW, 0x80);
    SN65DSI86_WRITE(SN65DSI86_VIDEO_CHA_LINE_HIGH, 0x07);
    usleep_range(10000, 12000);

    /* CHB_ACTIVE_LINE_LENGTH */
    SN65DSI86_WRITE(SN65DSI86_VIDEO_CHB_LINE_LOW, 0x00);
    SN65DSI86_WRITE(SN65DSI86_VIDEO_CHB_LINE_HIGH, 0x00);
    usleep_range(10000, 12000);

    /* CHA_VERTICAL_DISPLAY_SIZE */
    SN65DSI86_WRITE(SN65DSI86_CHA_VERT_DISP_SIZE_LOW, 0x38);
    SN65DSI86_WRITE(SN65DSI86_CHA_VERT_DISP_SIZE_HIGH, 0x04);
    usleep_range(10000, 12000);

    /* CHA_HSYNC_PULSE_WIDTH */
    SN65DSI86_WRITE(SN65DSI86_CHA_HSYNC_PULSE_WIDTH_LOW, 0x2C);
    SN65DSI86_WRITE(SN65DSI86_CHA_HSYNC_PULSE_WIDTH_HIGH, 0x00);
    usleep_range(10000, 12000);

    /* CHA_VSYNC_PULSE_WIDTH */
    SN65DSI86_WRITE(SN65DSI86_CHA_VSYNC_PULSE_WIDTH_LOW, 0x05);
    SN65DSI86_WRITE(SN65DSI86_CHA_VSYNC_PULSE_WIDTH_HIGH, 0x00);
    usleep_range(10000, 12000);

    /* CHA_HORIZONTAL_BACK_PORCH */
    SN65DSI86_WRITE(SN65DSI86_CHA_HORIZONTAL_BACK_PORCH, 0x94);
    usleep_range(10000, 12000);

    /* CHA_VERTICAL_BACK_PORCH */
    SN65DSI86_WRITE(SN65DSI86_CHA_VERTICAL_BACK_PORCH, 0x24);
    usleep_range(10000, 12000);

    /* CHA_HORIZONTAL_FRONT_PORCH */
    SN65DSI86_WRITE(SN65DSI86_CHA_HORIZONTAL_FRONT_PORCH, 0x58);
    usleep_range(10000, 12000);

    /* CHA_VERTICAL_FRONT_PORCH */
    SN65DSI86_WRITE(SN65DSI86_CHA_VERTICAL_FRONT_PORCH, 0x04);
    usleep_range(10000, 12000);

    /* DP-24BPP Enable */
    SN65DSI86_WRITE(SN65DSI86_DP_24BPP_EN, 0x00);
    msleep(10);
   
    /* COLOR BAR */
    SN65DSI86_WRITE(SN65DSI86_COLOR_BAR_CFG, 0x00);
    msleep(10);

    /* enhanced framing and Vstream enable */
    SN65DSI86_WRITE(SN65DSI86_FRAMING_CFG, 0x0C);
    msleep(10);
    dev_dbg(&client->dev,"%s -\n",__func__);
    return 0;
}

static int sn65dsi86_brg_setup(struct sn65dsi86_brg *brg)
{
    struct i2c_client *client = I2C_CLIENT(brg);
    dev_dbg(&client->dev,"%s\n",__func__);
    sn65dsi86_brg_configure(brg);
    return 0;
}

static int sn65dsi86_brg_reset(struct sn65dsi86_brg *brg)
{
    /* Soft Reset reg value at power on should be 0x00 */
    struct i2c_client *client = I2C_CLIENT(brg);
    int ret = SN65DSI86_READ(SN65DSI86_SOFT_RESET);
    dev_dbg(&client->dev,"%s\n",__func__);
    if (ret != 0x00) {
        dev_err(&client->dev,"Failed to reset the device");
        return -ENODEV;
    }
    return 0;
}

static int sn65dsi86_brg_hpd_detect(struct sn65dsi86_brg *brg)
{
    return brg->hpd_status;
}

void work_hpd_detect_handler(struct work_struct *work)
{
    struct i2c_client *client = I2C_CLIENT(g_brg);
    int ret = sn65dsi86_read(client, SN65DSI86_DP_HPD_DISABLE);

    dev_dbg(&client->dev,"%s\n",__func__);
    if(g_brg != NULL){
        if(ret & 0x10){
            if(g_brg->hpd_debuncing_status != 0){
                dev_info(&client->dev,"HPD connected or replug");
                sn65dsi86_brg_setup(g_brg);
                sn65dsi86_brg_start_stream(g_brg);
            }
            if(g_brg->hpd_debuncing_count >= TIMEOUT_HPD_UPDATE){
                g_brg->hpd_status = 0;
                g_brg->hpd_debuncing_count = 0;
            }
            else{
                g_brg->hpd_debuncing_count += 1;
            }
            g_brg->hpd_debuncing_status = 0;
        }
        else{
            if(g_brg->hpd_debuncing_status == 0){
                dev_info(&client->dev,"HPD disconnected or unknown");
                sn65dsi86_brg_stop_stream(g_brg);
            }
             if(g_brg->hpd_debuncing_count >= TIMEOUT_HPD_UPDATE){
                g_brg->hpd_status = -ENODEV;
                g_brg->hpd_debuncing_count = 0;
            }
            else{
                g_brg->hpd_debuncing_count += 1;
            }
            g_brg->hpd_debuncing_status = -ENODEV;
        }
    }

    msleep(TIME_ELAPSE_HPD_WORKQUEUE);
    queue_work(workqueue_hpd, &work_hpd);
}

static struct sn65dsi86_brg_funcs brg_func = {
    .power_on = sn65dsi86_brg_power_on,
    .power_off = sn65dsi86_brg_power_off,
    .setup = sn65dsi86_brg_setup,
    .reset = sn65dsi86_brg_reset,
    .start_stream = sn65dsi86_brg_start_stream,
    .stop_stream = sn65dsi86_brg_stop_stream,
    .hpd_detect = sn65dsi86_brg_hpd_detect,
};

static struct sn65dsi86_brg brg = {
    .funcs = &brg_func,
};

struct sn65dsi86_brg *sn65dsi86_brg_get(void) {
    return &brg;
}

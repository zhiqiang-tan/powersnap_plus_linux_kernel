/*
 * Licensed under the GPL-2.
 */
#include <linux/device.h>
#include <linux/gpio/consumer.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_graph.h>
#include <linux/slab.h>

#include <drm/drmP.h>
#include <drm/drm_atomic.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_edid.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_connector.h>
#include <drm/drm_crtc_helper.h>
#include <video/mipi_display.h>
#include <video/of_videomode.h>
#include <video/videomode.h>

#include "sn65dsi86_timing.h"
#include "sn65dsi86_brg.h"

#include <linux/hardware_board_id.h>

struct sn65dsi86 {
    u8 channel_id;
    enum drm_connector_status status;
    bool powered;
    struct drm_display_mode curr_mode;
    struct drm_bridge bridge;
    struct drm_connector connector;
    struct device_node *host_node;
    struct mipi_dsi_device *dsi;
    struct sn65dsi86_brg *brg;
};

static int sn65dsi86_attach_dsi(struct sn65dsi86 *sn65dsi86);
#define DRM_DEVICE(A) A->dev->dev
/* Connector funcs */
static struct sn65dsi86 *connector_to_sn65dsi86(struct drm_connector *connector)
{
    return container_of(connector, struct sn65dsi86, connector);
}

static int sn65dsi86_connector_get_modes(struct drm_connector *connector)
{
    struct sn65dsi86 *sn65dsi86 = connector_to_sn65dsi86(connector);
    struct sn65dsi86_brg *brg = sn65dsi86->brg;
    struct device *dev = connector->dev->dev;
    struct drm_display_mode *mode;
    u32 bus_format = MEDIA_BUS_FMT_RGB888_1X24;
    u32 *bus_flags = &connector->display_info.bus_flags;
    int ret;

    mode = drm_mode_create(connector->dev);
    if (!mode) {
        DRM_DEV_ERROR(dev, "Failed to create display mode!\n");
        return 0;
    }

    drm_display_mode_from_videomode(&brg->vm, mode);
    mode->width_mm = brg->width_mm;
    mode->height_mm = brg->height_mm;
    mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;

    drm_mode_probed_add(connector, mode);
    drm_mode_connector_list_update(connector);

    connector->display_info.width_mm = mode->width_mm;
    connector->display_info.height_mm = mode->height_mm;

    if (brg->vm.flags & DISPLAY_FLAGS_DE_HIGH)
        *bus_flags |= DRM_BUS_FLAG_DE_HIGH;
    if (brg->vm.flags & DISPLAY_FLAGS_DE_LOW)
        *bus_flags |= DRM_BUS_FLAG_DE_LOW;
    if (brg->vm.flags & DISPLAY_FLAGS_PIXDATA_NEGEDGE)
        *bus_flags |= DRM_BUS_FLAG_PIXDATA_NEGEDGE;
    if (brg->vm.flags & DISPLAY_FLAGS_PIXDATA_POSEDGE)
        *bus_flags |= DRM_BUS_FLAG_PIXDATA_POSEDGE;

    ret = drm_display_info_set_bus_formats(&connector->display_info,
                           &bus_format, 1);
    if (ret)
        return ret;
    return 1;
}

static enum drm_mode_status
sn65dsi86_connector_mode_valid(struct drm_connector *connector,
                 struct drm_display_mode *mode)
{
    struct sn65dsi86 *sn65dsi86 = connector_to_sn65dsi86(connector);
    struct device *dev = connector->dev->dev;
	if (mode->clock > ( sn65dsi86->brg->vm.pixelclock / 1000 ))
		return MODE_CLOCK_HIGH;

    dev_dbg(dev, "%s: mode: %d*%d@%d is valid\n",__func__,
            mode->hdisplay,mode->vdisplay,mode->clock);
    return MODE_OK;
}

static struct drm_connector_helper_funcs sn65dsi86_connector_helper_funcs = {
    .get_modes = sn65dsi86_connector_get_modes,
    .mode_valid = sn65dsi86_connector_mode_valid,
};

static enum drm_connector_status
sn65dsi86_connector_detect(struct drm_connector *connector, bool force)
{
    struct sn65dsi86 *sn65dsi86 = connector_to_sn65dsi86(connector);
    struct device *dev = connector->dev->dev;
    enum drm_connector_status status;
    dev_dbg(dev, "%s\n",__func__);
    if(sn65dsi86->brg->funcs->hpd_detect(sn65dsi86->brg) == 0){
        status = connector_status_connected;
    }
    else{
        status = connector_status_disconnected;
    }
    sn65dsi86->status = status;
    return status;
}

int drm_helper_probe_single_connector_modes(struct drm_connector *connector,
                        uint32_t maxX, uint32_t maxY);

static struct drm_connector_funcs sn65dsi86_connector_funcs = {
    .dpms = drm_helper_connector_dpms,
    .fill_modes = drm_helper_probe_single_connector_modes,
    .detect = sn65dsi86_connector_detect,
    .destroy = drm_connector_cleanup,
    .reset = drm_atomic_helper_connector_reset,
    .atomic_duplicate_state = drm_atomic_helper_connector_duplicate_state,
    .atomic_destroy_state = drm_atomic_helper_connector_destroy_state,
};

/* Bridge funcs */
static struct sn65dsi86 *bridge_to_sn65dsi86(struct drm_bridge *bridge)
{
    return container_of(bridge, struct sn65dsi86, bridge);
}

static void sn65dsi86_bridge_enable(struct drm_bridge *bridge)
{
    /*struct sn65dsi86 *sn65dsi86 = bridge_to_sn65dsi86(bridge);*/

    dev_dbg(DRM_DEVICE(bridge),"%s\n",__func__);
    /*sn65dsi86->brg->funcs->setup(sn65dsi86->brg);
    sn65dsi86->brg->funcs->start_stream(sn65dsi86->brg);*/
}

static void sn65dsi86_bridge_disable(struct drm_bridge *bridge)
{
    /*struct sn65dsi86 *sn65dsi86 = bridge_to_sn65dsi86(bridge);*/
    dev_dbg(DRM_DEVICE(bridge),"%s\n",__func__);
    /*sn65dsi86->brg->funcs->stop_stream(sn65dsi86->brg);
    sn65dsi86->brg->funcs->power_off(sn65dsi86->brg);*/
}

static void sn65dsi86_bridge_mode_set(struct drm_bridge *bridge,
                    struct drm_display_mode *mode,
                    struct drm_display_mode *adj_mode)
{
    struct sn65dsi86 *sn65dsi86 = bridge_to_sn65dsi86(bridge);
    dev_dbg(DRM_DEVICE(bridge), "%s: mode: %d*%d@%d\n",__func__,
            mode->hdisplay,mode->vdisplay,mode->clock);
    drm_mode_copy(&sn65dsi86->curr_mode, adj_mode);
}

static int sn65dsi86_bridge_attach(struct drm_bridge *bridge)
{
    struct sn65dsi86 *sn65dsi86 = bridge_to_sn65dsi86(bridge);
    int ret;

    dev_dbg(DRM_DEVICE(bridge),"%s\n",__func__);
    if (!bridge->encoder) {
        DRM_ERROR("Parent encoder object not found");
        return -ENODEV;
    }

    sn65dsi86->connector.polled = DRM_CONNECTOR_POLL_CONNECT | DRM_CONNECTOR_POLL_DISCONNECT;

    ret = drm_connector_init(bridge->dev, &sn65dsi86->connector,
                 &sn65dsi86_connector_funcs,
                 DRM_MODE_CONNECTOR_DSI);
    if (ret) {
        DRM_ERROR("Failed to initialize connector with drm\n");
        return ret;
    }
    drm_connector_helper_add(&sn65dsi86->connector,
                 &sn65dsi86_connector_helper_funcs);
    drm_mode_connector_attach_encoder(&sn65dsi86->connector, bridge->encoder);

    ret = sn65dsi86_attach_dsi(sn65dsi86);

    return ret;
}

static struct drm_bridge_funcs sn65dsi86_bridge_funcs = {
    .enable = sn65dsi86_bridge_enable,
    .disable = sn65dsi86_bridge_disable,
    .mode_set = sn65dsi86_bridge_mode_set,
    .attach = sn65dsi86_bridge_attach,
};

static int sn65dsi86_parse_dt(struct device_node *np,
    struct sn65dsi86 *sn65dsi86)
{
    struct device *dev = &sn65dsi86->brg->client->dev;
    u32 num_lanes = 2, bpp = 24, format = 2, width = 149, height = 93;
    u32 num_channels;
    u8 burst_mode = 0;
    u8 de_neg_polarity = 0;
    struct device_node *endpoint;

    endpoint = of_graph_get_next_endpoint(np, NULL);
    if (!endpoint)
        return -ENODEV;

    sn65dsi86->host_node = of_graph_get_remote_port_parent(endpoint);
    if (!sn65dsi86->host_node) {
        of_node_put(endpoint);
        return -ENODEV;
    }

    of_property_read_u32(np, "ti,dsi-lanes", &num_lanes);
    of_property_read_u32(np, "ti,dp-format", &format);
    of_property_read_u32(np, "ti,dp-bpp", &bpp);
    of_property_read_u32(np, "ti,width-mm", &width);
    of_property_read_u32(np, "ti,height-mm", &height);
    burst_mode = of_property_read_bool(np, "ti,burst-mode");
    de_neg_polarity = of_property_read_bool(np, "ti,de-neg-polarity");

    if (num_lanes < 1 || num_lanes > 4) {
        dev_err(dev, "Invalid dsi-lanes: %d\n", num_lanes);
        return -EINVAL;
    }

    if (of_property_read_u32(np, "ti,dp-channels", &num_channels) < 0) {
        dev_info(dev, "dp-channels property not found, using default\n");
        num_channels = 1;
    } else {
        if (num_channels < 1 || num_channels > 2 ) {
            dev_err(dev, "dp-channels must be 1 or 2, not %u", num_channels);
            return -EINVAL;
        }
    }

    sn65dsi86->brg->num_dsi_lanes = num_lanes;
    sn65dsi86->brg->burst_mode = burst_mode;
    sn65dsi86->brg->de_neg_polarity = de_neg_polarity;
    sn65dsi86->brg->num_channels = num_channels;

    sn65dsi86->brg->format = format;
    sn65dsi86->brg->bpp = bpp;

    sn65dsi86->brg->width_mm = width;
    sn65dsi86->brg->height_mm = height;

    /* Read default timing if there is not device tree node for */
    if ((of_get_videomode(np, &sn65dsi86->brg->vm, 0)) < 0)
        videomode_from_timing(&panel_default_timing, &sn65dsi86->brg->vm);

    of_node_put(endpoint);
    of_node_put(sn65dsi86->host_node);

    return 0;
}

static int sn65dsi86_probe(struct i2c_client *i2c,
    const struct i2c_device_id *id)
{
    struct sn65dsi86 *sn65dsi86;
    struct device *dev = &i2c->dev;
    int ret;

	printk("sn65dsi86_probe()+\n");

	if ( get_hardware_board_sku() != HW_BOARD_ND108T_8MQ_4G32G ) {
		dev_err(dev, "It's not HW_BOARD_ND108T_8MQ_4G32G SKU, Skip enabling MIPI-DP bridge driver!!!\n");
		return -ENODEV;
	}

    dev_dbg(dev,"%s\n",__func__);
    if (!dev->of_node)
        return -EINVAL;

    sn65dsi86 = devm_kzalloc(dev, sizeof(*sn65dsi86), GFP_KERNEL);
    if (!sn65dsi86)
        return -ENOMEM;

    /* Initialize it before DT parser */
    sn65dsi86->brg = sn65dsi86_brg_get();
    sn65dsi86->brg->client = i2c;

    sn65dsi86->powered = false;
    sn65dsi86->status = connector_status_disconnected;

    i2c_set_clientdata(i2c, sn65dsi86);

    ret = sn65dsi86_parse_dt(dev->of_node, sn65dsi86);
    if (ret)
        return ret;

    sn65dsi86->brg->funcs->power_off(sn65dsi86->brg);
    sn65dsi86->brg->funcs->power_on(sn65dsi86->brg);
    ret  = sn65dsi86->brg->funcs->reset(sn65dsi86->brg);
    if (ret != 0x00) {
        dev_err(dev, "Failed to reset the device");
        return -ENODEV;
    }
    sn65dsi86->brg->funcs->power_off(sn65dsi86->brg);


    sn65dsi86->bridge.funcs = &sn65dsi86_bridge_funcs;
    sn65dsi86->bridge.of_node = dev->of_node;

    ret = drm_bridge_add(&sn65dsi86->bridge);
    if (ret) {
        dev_err(dev, "failed to add sn65dsi86 bridge\n");
    }
    
    printk("sn65dsi86_probe()-\n");
    return ret;
}

static int sn65dsi86_attach_dsi(struct sn65dsi86 *sn65dsi86)
{
    struct device *dev = &sn65dsi86->brg->client->dev;
    struct mipi_dsi_host *host;
    struct mipi_dsi_device *dsi;
    int ret = 0;
    const struct mipi_dsi_device_info info = { .type = "sn65dsi86",
                           .channel = 0,
                           .node = NULL,
                         };

    dev_dbg(dev, "%s\n",__func__);
    host = of_find_mipi_dsi_host_by_node(sn65dsi86->host_node);
    if (!host) {
        dev_err(dev, "failed to find dsi host\n");
        return -EPROBE_DEFER;
    }

    dsi = mipi_dsi_device_register_full(host, &info);
    if (IS_ERR(dsi)) {
        dev_err(dev, "failed to create dsi device\n");
        ret = PTR_ERR(dsi);
        return -ENODEV;
    }

    sn65dsi86->dsi = dsi;

    dsi->lanes = sn65dsi86->brg->num_dsi_lanes;
    dsi->format = MIPI_DSI_FMT_RGB888;
    dsi->mode_flags = MIPI_DSI_MODE_VIDEO;
    if (sn65dsi86->brg->burst_mode)
        dsi->mode_flags |= MIPI_DSI_MODE_VIDEO_BURST;
    else
        dsi->mode_flags |= MIPI_DSI_MODE_VIDEO_SYNC_PULSE;

    ret = mipi_dsi_attach(dsi);
    if (ret < 0) {
        dev_err(dev, "failed to attach dsi to host\n");
        mipi_dsi_device_unregister(dsi);
    }
    return ret;
}

static void sn65dsi86_detach_dsi(struct sn65dsi86 *sn65dsi86)
{
    struct device *dev = &sn65dsi86->brg->client->dev;
    dev_dbg(dev, "%s\n",__func__);
    mipi_dsi_detach(sn65dsi86->dsi);
    mipi_dsi_device_unregister(sn65dsi86->dsi);
}

static int sn65dsi86_remove(struct i2c_client *i2c)
{
    struct sn65dsi86 *sn65dsi86 = i2c_get_clientdata(i2c);
    struct device *dev = &sn65dsi86->brg->client->dev;
    dev_dbg(dev, "%s\n",__func__);

    sn65dsi86_detach_dsi(sn65dsi86);
    drm_bridge_remove(&sn65dsi86->bridge);

    return 0;
}

static const struct i2c_device_id sn65dsi86_i2c_ids[] = {
    { "sn65dsi86", 0 },
    { }
};
MODULE_DEVICE_TABLE(i2c, sn65dsi86_i2c_ids);

static const struct of_device_id sn65dsi86_of_ids[] = {
    { .compatible = "ti,sn65dsi86" },
    { }
};
MODULE_DEVICE_TABLE(of, sn65dsi86_of_ids);

static struct mipi_dsi_driver sn65dsi86_dsi_driver = {
    .driver.name = "sn65dsi86",
};

static struct i2c_driver sn65dsi86_driver = {
    .driver = {
        .name = "sn65dsi86",
        .of_match_table = sn65dsi86_of_ids,
    },
    .id_table = sn65dsi86_i2c_ids,
    .probe = sn65dsi86_probe,
    .remove = sn65dsi86_remove,
};

static int __init sn65dsi86_init(void)
{
    if (IS_ENABLED(CONFIG_DRM_MIPI_DSI))
        mipi_dsi_driver_register(&sn65dsi86_dsi_driver);

    return i2c_add_driver(&sn65dsi86_driver);
}
module_init(sn65dsi86_init);

static void __exit sn65dsi86_exit(void)
{
    i2c_del_driver(&sn65dsi86_driver);

    if (IS_ENABLED(CONFIG_DRM_MIPI_DSI))
        mipi_dsi_driver_unregister(&sn65dsi86_dsi_driver);
}
module_exit(sn65dsi86_exit);

MODULE_AUTHOR("CompuLab <compulab@compula.co.il>");
MODULE_DESCRIPTION("SN65DSI bridge driver");
MODULE_LICENSE("GPL");

#ifndef _SN65DSI86_BRG_H__
#define _SN65DSI86_BRG_H__

#include <linux/i2c.h>
#include <linux/gpio/consumer.h>
#include <video/videomode.h>

struct sn65dsi86_brg;
struct sn65dsi86_brg_funcs {
    int (*power_on)(struct sn65dsi86_brg *sn65dsi86_brg);
    void (*power_off)(struct sn65dsi86_brg *sn65dsi86_brg);
    int (*reset)(struct sn65dsi86_brg *sn65dsi86_brg);
    int (*setup)(struct sn65dsi86_brg *sn65dsi86_brg);
    int (*start_stream)(struct sn65dsi86_brg *sn65dsi86_brg);
    void (*stop_stream)(struct sn65dsi86_brg *sn65dsi86_brg);
    int (*hpd_detect)(struct sn65dsi86_brg *sn65dsi86_brg);
};

struct sn65dsi86_brg {
    struct i2c_client *client;
    /* Bridge Panel Parameters */
    struct videomode vm;
    u32 width_mm;
    u32 height_mm;
    u32 format;
    u32 bpp;
    int hpd_status;
    int hpd_debuncing_status;
    int hpd_debuncing_count;
    u8 num_dsi_lanes;
    u8 burst_mode;
    u8 de_neg_polarity;
    u8 num_channels;
    struct sn65dsi86_brg_funcs *funcs;
};
struct sn65dsi86_brg *sn65dsi86_brg_get(void);

#define I2C_DEVICE(A) &(A)->client->dev
#define I2C_CLIENT(A) (A)->client
#define VM(A) &(A)->vm
#define BPP(A) (A)->bpp
#define FORMAT(A) (A)->format
#define DSI_LANES(A) (A)->num_dsi_lanes

/* The caller has to have a vm structure defined */
#define PIXCLK vm->pixelclock
#define HACTIVE vm->hactive
#define HFP vm->hfront_porch
#define HBP vm->hback_porch
#define HPW vm->hsync_len
#define VACTIVE vm->vactive
#define VFP vm->vfront_porch
#define VBP vm->vback_porch
#define VPW vm->vsync_len
#define FLAGS vm->flags

#define HIGH(A) (((A) >> 8) & 0xFF)
#define LOW(A)  ((A)  & 0xFF)

#endif /* _SN65DSI86_BRG_H__ */

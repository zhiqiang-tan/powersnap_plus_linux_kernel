/*
 * include/linux/hardware_board_id.h
 *
 * Copyright (C) 2018 MiTAC
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef _LINUX_HW_BOARD_ID_H
#define _LINUX_HW_BOARD_ID_H

#define HW_BOARD_R0A	0x0
#define HW_BOARD_R0B	0x1
#define HW_BOARD_R0C	0x2
#define HW_BOARD_R0D	0x3
#define HW_BOARD_R0E	0x4
#define HW_BOARD_R01	0x5
#define HW_BOARD_R02	0x6
#define HW_BOARD_R03	0x7
#define HW_BOARD_R04	0x8
#define HW_BOARD_R05	0x9
#define HW_BOARD_R06	0xA
#define HW_BOARD_R07	0xB
#define HW_BOARD_R08	0xC
#define HW_BOARD_R09	0xD
#define HW_BOARD_R10	0xE
#define HW_BOARD_R11	0xF

enum hw_board_sku {
	HW_BOARD_ND108T_8MD_1G8G = 0x0,	//SKU-A, HDMI only
	HW_BOARD_ND108T_8MD_1G8G_LVDS,	//SKU-B, HDMI + LVDS
	HW_BOARD_ND108T_8MD_2G16G,		//SKU-C, HDMI only
	HW_BOARD_ND108T_8MQ_4G32G,		//SKU-D, HDMI + DP
    HW_BOARD_ND108T_8MQ_4G32G_LVDS,	//SKU-E, HDMI + LVDS
    HW_BOARD_ND108T_8MD_2G16G_LVDS,	//SKU-F, HDMI + LVDS
    HW_BOARD_ND108T_8MD_1G8G_LVDS_ONLY_COM,	//SKU-G, LVDS only (Giza)
    HW_BOARD_ND108T_8MQ_4G32G_LVDS_ONLY,	//SKU-H, LVDS only (DragonBall)
};

enum hw_interface_mode {
	HW_BOARD_GPIO_ONLY = 0x0,
	HW_BOARD_I2C,
	HW_BOARD_SPI,
	HW_BOARD_SPI_I2C,
	HW_BOARD_UART,
	HW_BOARD_UART_I2C,
	HW_BOARD_UART_SPI,
	HW_BOARD_UART_SPI_I2C,
};

enum f81439_mode {
	F81439_RS422_FULL_DUPLEX_1T_1R_MODE = 0x0,
	F81439_RS232_PURE_3T_5R_MODE,
	F81439_RS485_HALF_DUPLEX_1T_1R_LA_MODE,
	F81439_RS485_HALF_DUPLEX_1T_1R_HA_MODE,
	F81439_RS422_HALF_DUPLEX_1T_1R_BIAS_MODE,
	F81439_RS232_PURE_1T_1R_COEXISTS_MODE,
	F81439_RS485_HALF_DUPLEX_1T_1R_LA_BIAS_MODE,
	F81439_LOW_POWER_SHUTDOWN_MODE,
};

extern unsigned int get_hardware_board_ver(void);
extern unsigned int get_hardware_board_sku(void);
extern unsigned int get_hardware_board_mode(void);
extern unsigned int get_serial_port_mode(void);

#endif

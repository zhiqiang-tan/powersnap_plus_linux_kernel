/*
 * drivers/misc/mitac_systemlib.c
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

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/syscalls.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/hardware_board_id.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/delay.h>

#define DEVICE_NAME		"systemlib"

#define SYSTEMLIB_MAJOR					0xE2
#define IOCTL_GET_HW_BOARD_VERSION		_IOR(SYSTEMLIB_MAJOR, 100, char)
#define IOCTL_GET_HW_SKU_INFO			_IOR(SYSTEMLIB_MAJOR, 101, char)
#define IOCTL_GET_SW_IMAGE_VERSION		_IOR(SYSTEMLIB_MAJOR, 102, char)
#define IOCTL_SET_SERIAL_PORT_MODE		_IOW(SYSTEMLIB_MAJOR, 103, int)
#define IOCTL_GET_MITAC_CONFIG_SETTING	_IOR(SYSTEMLIB_MAJOR, 104, char)
#define IOCTL_SET_MITAC_CONFIG_SETTING	_IOW(SYSTEMLIB_MAJOR, 105, char)

#define FILE_VERSION_INFORMATION		"/etc/version-info"
#define FILE_EMMC_DEVICE				"/run/media/mmcblk0p1/config.txt"

#define MITAC_CONFIG_SETTING_SIZE		( 4096 )

enum bootargs_access_mode {
	READ_CONFIG_MODE = 0x0,
	WRITE_CONFIG_MODE,
};

struct plat_data {
	int pcie_clkreq_en;
	int pcie_clkreq_en_level;
	int f81439_mode0;
	int f81439_mode0_level;
	int f81439_mode1;
	int f81439_mode1_level;
	int f81439_mode2;
	int f81439_mode2_level;
	int f81439_slew;
	int f81439_slew_level;
	struct work_struct get_conf_work;
	struct work_struct set_conf_work;
	char *preadbuf;
	char *pwritebuf;
	int processing;
};

struct plat_data *pdata;

static int f81439_mode_conf(unsigned int mode)
{
	if ( (get_hardware_board_sku() != HW_BOARD_ND108T_8MD_1G8G_LVDS_ONLY_COM) &&
         (get_hardware_board_sku() != HW_BOARD_ND108T_8MQ_4G32G) &&
         (get_hardware_board_sku() != HW_BOARD_ND108T_8MQ_4G32G_LVDS) &&
         (get_hardware_board_sku() != HW_BOARD_ND108T_8MQ_4G32G_LVDS_ONLY) ) {
		pr_info("systemlib - It's not D, E G or H SKU, " \
				  "Skip enabling RS485/422/232 Combo Serial Port!!!\n");
		return -ENODEV;
	}

	if (!gpio_is_valid(pdata->f81439_mode0) ||
	    !gpio_is_valid(pdata->f81439_mode1) ||
	    !gpio_is_valid(pdata->f81439_mode2)) {
		pr_err("Fail to cofigure f81439 GPIOs!!!\n");
		return -EIO;
	}

	switch( mode ) {
		case F81439_RS422_FULL_DUPLEX_1T_1R_MODE:
			gpio_direction_output(pdata->f81439_mode0, 0);
			gpio_direction_output(pdata->f81439_mode1, 0);
			gpio_direction_output(pdata->f81439_mode2, 0);
			pr_info("systemlib - Set Serial Port to RS-422 Full Duplex 1T/1R Mode.\n");
			break;
		case F81439_RS232_PURE_3T_5R_MODE:
			gpio_direction_output(pdata->f81439_mode0, 0);
			gpio_direction_output(pdata->f81439_mode1, 0);
			gpio_direction_output(pdata->f81439_mode2, 1);
			pr_info("systemlib - Set Serial Port to Pure RS-232 3T/5R Mode.\n");
			break;
		case F81439_RS485_HALF_DUPLEX_1T_1R_LA_MODE:
			gpio_direction_output(pdata->f81439_mode0, 0);
			gpio_direction_output(pdata->f81439_mode1, 1);
			gpio_direction_output(pdata->f81439_mode2, 0);
			pr_info("systemlib - Set Serial Port to RS-485 Half Duplex 1T/1R Low Active Mode.\n");
			break;
		case F81439_RS485_HALF_DUPLEX_1T_1R_HA_MODE:
			gpio_direction_output(pdata->f81439_mode0, 0);
			gpio_direction_output(pdata->f81439_mode1, 1);
			gpio_direction_output(pdata->f81439_mode2, 1);
			pr_info("systemlib - Set Serial Port to RS-485 Half Duplex 1T/1R High Active Mode.\n");
			break;
		case F81439_RS422_HALF_DUPLEX_1T_1R_BIAS_MODE:
			gpio_direction_output(pdata->f81439_mode0, 1);
			gpio_direction_output(pdata->f81439_mode1, 0);
			gpio_direction_output(pdata->f81439_mode2, 0);
			pr_info("systemlib - Set Serial Port to RS-422 Half Duplex 1T/1R Bias Mode.\n");
			break;
		case F81439_RS232_PURE_1T_1R_COEXISTS_MODE:
			gpio_direction_output(pdata->f81439_mode0, 1);
			gpio_direction_output(pdata->f81439_mode1, 0);
			gpio_direction_output(pdata->f81439_mode2, 1);
			pr_info("systemlib - Set Serial Port to Pure RS-232 1T/1R Coexists Mode.\n");
			break;
		case F81439_RS485_HALF_DUPLEX_1T_1R_LA_BIAS_MODE:
			gpio_direction_output(pdata->f81439_mode0, 1);
			gpio_direction_output(pdata->f81439_mode1, 1);
			gpio_direction_output(pdata->f81439_mode2, 0);
			pr_info("systemlib - Set Serial Port to RS-485 Half Duplex 1T/1R Low Active & Bias Mode.\n");
			break;
		case F81439_LOW_POWER_SHUTDOWN_MODE:
		default:
			gpio_direction_output(pdata->f81439_mode0, 1);
			gpio_direction_output(pdata->f81439_mode1, 1);
			gpio_direction_output(pdata->f81439_mode2, 1);
			pr_info("systemlib - Set Serial Port to Low Power Shutdown Mode.\n");
			break;
	}
	return 0;
}

static int systemlib_conf_access(int mode, char *buf)
{
	int ret = 0;
	mm_segment_t oldfs;
	struct file *filp_emmc;
	ssize_t size;

	pr_debug("%s: (%c)+\n", __func__, (mode==WRITE_CONFIG_MODE)?'W':'R');

	oldfs = get_fs();
	set_fs(KERNEL_DS);
	filp_emmc = filp_open(FILE_EMMC_DEVICE, O_RDWR, 0);
	if (IS_ERR(filp_emmc)) {
		ret = PTR_ERR(filp_emmc);
	} else {
		filp_emmc->f_pos = 0;
		if (mode == WRITE_CONFIG_MODE) {
			size = vfs_write(filp_emmc, buf, MITAC_CONFIG_SETTING_SIZE, &filp_emmc->f_pos);
		} else {
			size = vfs_read(filp_emmc, buf, MITAC_CONFIG_SETTING_SIZE, &filp_emmc->f_pos);
		}

		if (size != MITAC_CONFIG_SETTING_SIZE) {
			ret = -EIO;
		}
		filp_close(filp_emmc, NULL);
	}
	set_fs(oldfs);

	pr_debug("%s: (%c)-\n", __func__, (mode==WRITE_CONFIG_MODE)?'W':'R');

	return ret;
}

static void systemlib_get_conf_work(struct work_struct *work)
{
	pdata->processing = true;
	systemlib_conf_access(READ_CONFIG_MODE, pdata->preadbuf);
	pdata->processing = false;
}

static void systemlib_set_conf_work(struct work_struct *work)
{
	systemlib_conf_access(WRITE_CONFIG_MODE, pdata->pwritebuf);
}

static int systemlib_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int systemlib_close(struct inode *inode, struct file *file)
{
	return 0;
}

static long systemlib_ioctl(struct file *file, unsigned int cmd,
			    unsigned long arg)
{
	long ret = 0;
	char hw_ver[4] = { 0 };
	char sku_info[32] = { 0 };
	char fw_ver[128] = { 0 };
	struct file *fp_file;
	mm_segment_t oldfs;
	int serial_port_mode;
	int retry = 300;

	pr_debug("%s: (0x%x)+\n", __func__, cmd);

	switch (cmd) {
	case IOCTL_GET_HW_BOARD_VERSION:
		switch (get_hardware_board_ver()) {
		case HW_BOARD_R0A:
			strcpy(hw_ver, "R0A");
			break;
		case HW_BOARD_R0B:
			strcpy(hw_ver, "R0B");
			break;
		case HW_BOARD_R0C:
			strcpy(hw_ver, "R0C");
			break;
		case HW_BOARD_R0D:
			strcpy(hw_ver, "R0D");
			break;
		case HW_BOARD_R0E:
			strcpy(hw_ver, "R0E");
			break;
		case HW_BOARD_R01:
			strcpy(hw_ver, "R01");
			break;
		case HW_BOARD_R02:
			strcpy(hw_ver, "R02");
			break;
		case HW_BOARD_R03:
			strcpy(hw_ver, "R03");
			break;
		case HW_BOARD_R04:
			strcpy(hw_ver, "R04");
			break;
		case HW_BOARD_R05:
			strcpy(hw_ver, "R05");
			break;
		case HW_BOARD_R06:
			strcpy(hw_ver, "R06");
			break;
		case HW_BOARD_R07:
			strcpy(hw_ver, "R07");
			break;
		case HW_BOARD_R08:
			strcpy(hw_ver, "R08");
			break;
		case HW_BOARD_R09:
			strcpy(hw_ver, "R09");
			break;
		case HW_BOARD_R10:
			strcpy(hw_ver, "R10");
			break;
		default:
			strcpy(hw_ver, "R00");
			break;
		}

		if (copy_to_user((char *)arg, hw_ver, strlen(hw_ver))) {
			pr_err("%s - Fail to copy buffer from kernel mode to user mode.\n", __func__);
			ret = -EFAULT;
		}
		break;

	case IOCTL_GET_HW_SKU_INFO:
		switch (get_hardware_board_sku()) {
		case HW_BOARD_ND108T_8MD_1G8G:					//SKU-A
			strcpy(sku_info, (char*)"ND108T_8MD_1G8G");
			break;
		case HW_BOARD_ND108T_8MD_1G8G_LVDS:				//SKU-B
			strcpy(sku_info, (char*)"ND108T_8MD_1G8G_LVDS");
			break;
		case HW_BOARD_ND108T_8MD_2G16G:					//SKU-C
			strcpy(sku_info, (char*)"ND108T_8MD_2G16G");
			break;
		case HW_BOARD_ND108T_8MQ_4G32G:					//SKU-D
			strcpy(sku_info, (char*)"ND108T_8MQ_4G32G");
			break;
        case HW_BOARD_ND108T_8MQ_4G32G_LVDS:			//SKU-E
            strcpy(sku_info, (char*)"ND108T_8MQ_4G32G_LVDS");
            break;
         case HW_BOARD_ND108T_8MD_2G16G_LVDS:			//SKU-F
            strcpy(sku_info, (char*)"ND108T_8MD_2G16G_LVDS");
            break;
         case HW_BOARD_ND108T_8MD_1G8G_LVDS_ONLY_COM:	//SKU-G
            strcpy(sku_info, (char*)"ND108T_8MD_1G8G_LVDS_ONLY_COM");
            break;
         case HW_BOARD_ND108T_8MQ_4G32G_LVDS_ONLY:		//SKU-H
            strcpy(sku_info, (char*)"ND108T_8MQ_4G32G_LVDS_ONLY");
            break;
		default:
			strcpy(sku_info, (char*)"UNKNOWN SKU");
			break;
		}

		if (copy_to_user((char *)arg, sku_info, strlen(sku_info))) {
			pr_err("%s - Fail to copy buffer from kernel mode to user mode.\n", __func__);
			ret = -EFAULT;
		}
		break;

	case IOCTL_GET_SW_IMAGE_VERSION:
		oldfs = get_fs();
		set_fs(get_ds());
		fp_file = filp_open(FILE_VERSION_INFORMATION, O_RDONLY, 0);
		if (IS_ERR(fp_file)) {
			ret = -EIO;
			break;
		}
		fp_file->f_pos = 0;
		if (vfs_read(fp_file, fw_ver, sizeof(fw_ver), &fp_file->f_pos) == 0) {
			ret = -EIO;
		}
		filp_close(fp_file, NULL);
		set_fs(oldfs);

		if (copy_to_user((char *)arg, fw_ver, strlen(fw_ver)-1)) {
			pr_err("%s - Fail to copy buffer from kernel mode to user mode.\n", __func__);
			ret = -EFAULT;
		}
		break;

	case IOCTL_SET_SERIAL_PORT_MODE:
		serial_port_mode = *(int *)arg;
		if ((serial_port_mode >= F81439_RS422_FULL_DUPLEX_1T_1R_MODE) &&
		    (serial_port_mode <= F81439_LOW_POWER_SHUTDOWN_MODE)) {
			ret = f81439_mode_conf(serial_port_mode);
		} else {
			ret = -EINVAL;
		}
		break;

	case IOCTL_GET_MITAC_CONFIG_SETTING:
		schedule_work(&pdata->get_conf_work);
		do {
			msleep(10);
		} while (pdata->processing && (retry-- > 0));

		/* Copy config setting from kernel space to user space. */
		if (copy_to_user((char *)arg, pdata->preadbuf, MITAC_CONFIG_SETTING_SIZE)) {
			pr_err("%s - Fail to copy buffer from kernel mode to user mode.\n", __func__);
			ret = -EFAULT;
		}
		pr_debug("%s - MiTAC Config Settings: %s\n", __func__, pdata->preadbuf);
		break;

	case IOCTL_SET_MITAC_CONFIG_SETTING:
		/* Reset data buffer */
		memset(pdata->preadbuf, 0, MITAC_CONFIG_SETTING_SIZE);
		memset(pdata->pwritebuf, 0, MITAC_CONFIG_SETTING_SIZE);

		/* Copy config setting from user space to kernel space. */
		if (copy_from_user(pdata->pwritebuf, (char *)arg, MITAC_CONFIG_SETTING_SIZE)) {
			pr_err("%s - Fail to copy buffer from user mode to kernel mode.\n", __func__);
			return -EFAULT;
		}

		/* Check the config partition if it's the same */
		schedule_work(&pdata->get_conf_work);
		do {
			msleep(10);
		} while (pdata->processing && (retry-- > 0));

		if (!strncmp(pdata->preadbuf, pdata->pwritebuf, strlen(pdata->pwritebuf))) {
			pr_debug("%s - MiTAC Config settings are no changed!!!\n", __func__);
		} else {
			pr_debug("%s - MiTAC Config Settings: %s\n", __func__, pdata->pwritebuf);
			schedule_work(&pdata->set_conf_work);
		}
		break;

	default:
		pr_err("%s - ioctl isn't correct parameter\n", __func__);
		ret = -EINVAL;
		break;
	}

	pr_debug("%s-\n", __func__);

	return ret;
}

static struct file_operations systemlib_fops = {
	.owner   =   THIS_MODULE,
	.open    =   systemlib_open,
	.release =   systemlib_close,
	.unlocked_ioctl   =   systemlib_ioctl,
};

static struct miscdevice systemlib = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = DEVICE_NAME,
	.fops = &systemlib_fops,
};

static int systemlib_probe(struct platform_device *dev)
{
	struct device_node *node = dev->dev.of_node;
	enum of_gpio_flags flags;

	pdata = kzalloc(sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;

	pdata->preadbuf = kzalloc(MITAC_CONFIG_SETTING_SIZE, GFP_KERNEL);
	if (!pdata->preadbuf)
		return -ENOMEM;

	pdata->pwritebuf = kzalloc(MITAC_CONFIG_SETTING_SIZE, GFP_KERNEL);
	if (!pdata->pwritebuf)
		return -ENOMEM;

	misc_register(&systemlib);

	INIT_WORK(&pdata->get_conf_work, systemlib_get_conf_work);
	INIT_WORK(&pdata->set_conf_work, systemlib_set_conf_work);
	cancel_work_sync(&pdata->get_conf_work);
	cancel_work_sync(&pdata->set_conf_work);

	pdata->pcie_clkreq_en = of_get_named_gpio_flags(node, "pcie-clkreq-gpios", 0, &flags);
	pdata->pcie_clkreq_en_level = !(flags == OF_GPIO_ACTIVE_LOW) ? 1 : 0;

	if (gpio_is_valid(pdata->pcie_clkreq_en)) {
		gpio_request(pdata->pcie_clkreq_en, "pcie_clkreq_en pin");
		gpio_direction_output(pdata->pcie_clkreq_en, pdata->pcie_clkreq_en_level);
		pr_info("systemlib - Set pcie_clkreq_en to GPIO Output %s\n", (pdata->pcie_clkreq_en_level) ? "High" : "Low");
	}

	if ( (get_hardware_board_sku() == HW_BOARD_ND108T_8MD_1G8G_LVDS_ONLY_COM) ||
         (get_hardware_board_sku() == HW_BOARD_ND108T_8MQ_4G32G) ||
         (get_hardware_board_sku() == HW_BOARD_ND108T_8MQ_4G32G_LVDS) ||
         (get_hardware_board_sku() == HW_BOARD_ND108T_8MQ_4G32G_LVDS_ONLY) ){

		pdata->f81439_mode0 = of_get_named_gpio_flags(node, "f81439-mode0", 0, &flags);
		pdata->f81439_mode0_level = !(flags == OF_GPIO_ACTIVE_LOW) ? 1 : 0;
		pdata->f81439_mode1 = of_get_named_gpio_flags(node, "f81439-mode1", 0, &flags);
		pdata->f81439_mode1_level = !(flags == OF_GPIO_ACTIVE_LOW) ? 1 : 0;
		pdata->f81439_mode2 = of_get_named_gpio_flags(node, "f81439-mode2", 0, &flags);
		pdata->f81439_mode2_level = !(flags == OF_GPIO_ACTIVE_LOW) ? 1 : 0;
		pdata->f81439_slew = of_get_named_gpio_flags(node, "f81439-slew", 0, &flags);
		pdata->f81439_slew_level = !(flags == OF_GPIO_ACTIVE_LOW) ? 1 : 0;

		if (gpio_is_valid(pdata->f81439_mode0)) {
			gpio_request(pdata->f81439_mode0, "f81439_mode0 pin");
			gpio_direction_output(pdata->f81439_mode0, pdata->f81439_mode0_level);
			pr_info("systemlib - Set f81439_mode0 to GPIO Output %s\n", (pdata->f81439_mode0_level) ? "High" : "Low");
		}
		if (gpio_is_valid(pdata->f81439_mode1)) {
			gpio_request(pdata->f81439_mode1, "f81439_mode1 pin");
			gpio_direction_output(pdata->f81439_mode1, pdata->f81439_mode1_level);
			pr_info("systemlib - Set f81439_mode1 to GPIO Output %s\n", (pdata->f81439_mode1_level) ? "High" : "Low");
		}
		if (gpio_is_valid(pdata->f81439_mode2)) {
			gpio_request(pdata->f81439_mode2, "f81439_mode2 pin");
			gpio_direction_output(pdata->f81439_mode2, pdata->f81439_mode2_level);
			pr_info("systemlib - Set f81439_mode2 to GPIO Output %s\n", (pdata->f81439_mode2_level) ? "High" : "Low");
		}
		if (gpio_is_valid(pdata->f81439_slew)) {
			gpio_request(pdata->f81439_slew, "f81439_slew pin");
			gpio_direction_output(pdata->f81439_slew, pdata->f81439_slew_level);
			pr_info("systemlib - Set f81439_slew to GPIO Output %s\n", (pdata->f81439_slew) ? "High" : "Low");
		}

		/* Configure F81439 to convert RS232 to RS422/485/232 */
		f81439_mode_conf(get_serial_port_mode());
	}

	pr_info("Register MiTAC system library driver success.\n");

	return 0;
}

static int systemlib_remove(struct platform_device *dev)
{
	misc_deregister(&systemlib);

	if (gpio_is_valid(pdata->f81439_mode0))
		gpio_free(pdata->f81439_mode0);
	if (gpio_is_valid(pdata->f81439_mode1))
		gpio_free(pdata->f81439_mode1);
	if (gpio_is_valid(pdata->f81439_mode2))
		gpio_free(pdata->f81439_mode2);
	if (gpio_is_valid(pdata->f81439_slew))
		gpio_free(pdata->f81439_slew);

	if (pdata->preadbuf) {
		kfree(pdata->preadbuf);
		pdata->preadbuf = NULL;
	}

	if (pdata->pwritebuf) {
		kfree(pdata->pwritebuf);
		pdata->pwritebuf = NULL;
	}

	if (pdata) {
		kfree(pdata);
		pdata = NULL;
	}

	pr_info("Unregister MiTAC system library driver success.\n");

	return 0;
}

static void systemlib_shutdown(struct platform_device *dev)
{
	return;
}

static int systemlib_suspend(struct device *dev)
{
	return 0;
}

static int systemlib_resume(struct device *dev)
{
	return 0;
}

static const struct dev_pm_ops systemlib_pm_ops = {
	.suspend = systemlib_suspend,
	.resume = systemlib_resume,
};

static struct of_device_id systemlib_platdata_of_match[] = {
	{ .compatible = "mitac-systemlib" },
	{ }
};
MODULE_DEVICE_TABLE(of, systemlib_platdata_of_match);

static struct platform_driver systemlib_driver = {
	.probe     = systemlib_probe,
	.remove    = systemlib_remove,
	.shutdown = systemlib_shutdown,
	.driver	= {
		.name = DEVICE_NAME,
		.owner =THIS_MODULE,
		.pm = &systemlib_pm_ops,
		.of_match_table = of_match_ptr(systemlib_platdata_of_match),
	},
};

static int __init systemlib_init(void)
{
	return platform_driver_register(&systemlib_driver);
}

static void __exit systemlib_exit(void)
{
	platform_driver_unregister(&systemlib_driver);
}

module_init(systemlib_init);
module_exit(systemlib_exit);

MODULE_DESCRIPTION("MiTAC system library driver");
MODULE_AUTHOR("Mark Chang <mark.chang@mic.com.tw>");
MODULE_LICENSE("GPL");

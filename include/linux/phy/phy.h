/*
 * phy.h -- generic phy header file
 *
 * Copyright (C) 2013 Texas Instruments Incorporated - http://www.ti.com
 *
 * Author: Kishon Vijay Abraham I <kishon@ti.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef __DRIVERS_PHY_H
#define __DRIVERS_PHY_H

#include <linux/err.h>
#include <linux/of.h>
#include <linux/device.h>
#include <linux/pm_runtime.h>
#include <linux/regulator/consumer.h>

struct phy;

enum phy_mode {
	PHY_MODE_INVALID,
	PHY_MODE_USB_HOST,
	PHY_MODE_USB_DEVICE,
	PHY_MODE_USB_OTG,
	PHY_MODE_SGMII,
	PHY_MODE_10GKR,
	PHY_MODE_MIPI_DPHY,
};

#include <linux/phy/phy-dp.h>
#include <linux/phy/phy-mipi-dphy.h>
/**
 * union phy_configure_opts - Opaque generic phy configuration
 *
 * @mipi_dphy:	Configuration set applicable for phys supporting
 *		the MIPI_DPHY phy mode.
 * @dp:		Configuration set applicable for phys supporting
 *		the DisplayPort protocol.
 */
union phy_configure_opts {
	struct phy_configure_opts_mipi_dphy	mipi_dphy;
	struct phy_configure_opts_dp		dp;
};

int phy_configure(struct phy *phy, union phy_configure_opts *opts);
int phy_validate(struct phy *phy, enum phy_mode mode, int submode,
		 union phy_configure_opts *opts);
/**
 * struct phy_ops - set of function pointers for performing phy operations
 * @init: operation to be performed for initializing phy
 * @exit: operation to be performed while exiting
 * @power_on: powering on the phy
 * @power_off: powering off the phy
 * @set_mode: set the mode of the phy
 * @reset: resetting the phy
 * @owner: the module owner containing the ops
 */
struct phy_ops {
	int	(*init)(struct phy *phy);
	int	(*exit)(struct phy *phy);
	int	(*power_on)(struct phy *phy);
	int	(*power_off)(struct phy *phy);
	int	(*set_mode)(struct phy *phy, enum phy_mode mode);
	int	(*reset)(struct phy *phy);
	int	(*calibrate)(struct phy *phy);
	struct module *owner;

	/**
	 * @configure:
	 *
	 * Optional.
	 *
	 * Used to change the PHY parameters. phy_init() must have
	 * been called on the phy.
	 *
	 * Returns: 0 if successful, an negative error code otherwise
	 */
	int	(*configure)(struct phy *phy, union phy_configure_opts *opts);
	/**
	 * @validate:
	 *
	 * Optional.
	 *
	 * Used to check that the current set of parameters can be
	 * handled by the phy. Implementations are free to tune the
	 * parameters passed as arguments if needed by some
	 * implementation detail or constraints. It must not change
	 * any actual configuration of the PHY, so calling it as many
	 * times as deemed fit by the consumer must have no side
	 * effect.
	 *
	 * Returns: 0 if the configuration can be applied, an negative
	 * error code otherwise
	 */
	int	(*validate)(struct phy *phy, enum phy_mode mode, int submode,
			    union phy_configure_opts *opts);
};

/**
 * struct phy_attrs - represents phy attributes
 * @bus_width: Data path width implemented by PHY
 */
struct phy_attrs {
	u32			bus_width;
};

/**
 * struct phy - represents the phy device
 * @dev: phy device
 * @id: id of the phy device
 * @ops: function pointers for performing phy operations
 * @init_data: list of PHY consumers (non-dt only)
 * @mutex: mutex to protect phy_ops
 * @init_count: used to protect when the PHY is used by multiple consumers
 * @power_count: used to protect when the PHY is used by multiple consumers
 * @phy_attrs: used to specify PHY specific attributes
 */
struct phy {
	struct device		dev;
	int			id;
	const struct phy_ops	*ops;
	struct mutex		mutex;
	int			init_count;
	int			power_count;
	struct phy_attrs	attrs;
	struct regulator	*pwr;
};

/**
 * struct phy_provider - represents the phy provider
 * @dev: phy provider device
 * @owner: the module owner having of_xlate
 * @of_xlate: function pointer to obtain phy instance from phy pointer
 * @list: to maintain a linked list of PHY providers
 */
struct phy_provider {
	struct device		*dev;
	struct device_node	*children;
	struct module		*owner;
	struct list_head	list;
	struct phy * (*of_xlate)(struct device *dev,
		struct of_phandle_args *args);
};

struct phy_lookup {
	struct list_head node;
	const char *dev_id;
	const char *con_id;
	struct phy *phy;
};

#define	to_phy(a)	(container_of((a), struct phy, dev))

#define	of_phy_provider_register(dev, xlate)	\
	__of_phy_provider_register((dev), NULL, THIS_MODULE, (xlate))

#define	devm_of_phy_provider_register(dev, xlate)	\
	__devm_of_phy_provider_register((dev), NULL, THIS_MODULE, (xlate))

#define of_phy_provider_register_full(dev, children, xlate) \
	__of_phy_provider_register(dev, children, THIS_MODULE, xlate)

#define devm_of_phy_provider_register_full(dev, children, xlate) \
	__devm_of_phy_provider_register(dev, children, THIS_MODULE, xlate)

static inline void phy_set_drvdata(struct phy *phy, void *data)
{
	dev_set_drvdata(&phy->dev, data);
}

static inline void *phy_get_drvdata(struct phy *phy)
{
	return dev_get_drvdata(&phy->dev);
}

#if IS_ENABLED(CONFIG_GENERIC_PHY)
int phy_pm_runtime_get(struct phy *phy);
int phy_pm_runtime_get_sync(struct phy *phy);
int phy_pm_runtime_put(struct phy *phy);
int phy_pm_runtime_put_sync(struct phy *phy);
void phy_pm_runtime_allow(struct phy *phy);
void phy_pm_runtime_forbid(struct phy *phy);
int phy_init(struct phy *phy);
int phy_exit(struct phy *phy);
int phy_power_on(struct phy *phy);
int phy_power_off(struct phy *phy);
int phy_set_mode(struct phy *phy, enum phy_mode mode);
int phy_reset(struct phy *phy);
int phy_calibrate(struct phy *phy);
static inline int phy_get_bus_width(struct phy *phy)
{
	return phy->attrs.bus_width;
}
static inline void phy_set_bus_width(struct phy *phy, int bus_width)
{
	phy->attrs.bus_width = bus_width;
}
struct phy *phy_get(struct device *dev, const char *string);
struct phy *phy_optional_get(struct device *dev, const char *string);
struct phy *devm_phy_get(struct device *dev, const char *string);
struct phy *devm_phy_optional_get(struct device *dev, const char *string);
struct phy *devm_of_phy_get(struct device *dev, struct device_node *np,
			    const char *con_id);
struct phy *devm_of_phy_get_by_index(struct device *dev, struct device_node *np,
				     int index);
void phy_put(struct phy *phy);
void devm_phy_put(struct device *dev, struct phy *phy);
struct phy *of_phy_get(struct device_node *np, const char *con_id);
struct phy *of_phy_simple_xlate(struct device *dev,
	struct of_phandle_args *args);
struct phy *phy_create(struct device *dev, struct device_node *node,
		       const struct phy_ops *ops);
struct phy *devm_phy_create(struct device *dev, struct device_node *node,
			    const struct phy_ops *ops);
void phy_destroy(struct phy *phy);
void devm_phy_destroy(struct device *dev, struct phy *phy);
struct phy_provider *__of_phy_provider_register(struct device *dev,
	struct device_node *children, struct module *owner,
	struct phy * (*of_xlate)(struct device *dev,
				 struct of_phandle_args *args));
struct phy_provider *__devm_of_phy_provider_register(struct device *dev,
	struct device_node *children, struct module *owner,
	struct phy * (*of_xlate)(struct device *dev,
				 struct of_phandle_args *args));
void of_phy_provider_unregister(struct phy_provider *phy_provider);
void devm_of_phy_provider_unregister(struct device *dev,
	struct phy_provider *phy_provider);
int phy_create_lookup(struct phy *phy, const char *con_id, const char *dev_id);
void phy_remove_lookup(struct phy *phy, const char *con_id, const char *dev_id);
#else
static inline int phy_pm_runtime_get(struct phy *phy)
{
	if (!phy)
		return 0;
	return -ENOSYS;
}

static inline int phy_pm_runtime_get_sync(struct phy *phy)
{
	if (!phy)
		return 0;
	return -ENOSYS;
}

static inline int phy_pm_runtime_put(struct phy *phy)
{
	if (!phy)
		return 0;
	return -ENOSYS;
}

static inline int phy_pm_runtime_put_sync(struct phy *phy)
{
	if (!phy)
		return 0;
	return -ENOSYS;
}

static inline void phy_pm_runtime_allow(struct phy *phy)
{
	return;
}

static inline void phy_pm_runtime_forbid(struct phy *phy)
{
	return;
}

static inline int phy_init(struct phy *phy)
{
	if (!phy)
		return 0;
	return -ENOSYS;
}

static inline int phy_exit(struct phy *phy)
{
	if (!phy)
		return 0;
	return -ENOSYS;
}

static inline int phy_power_on(struct phy *phy)
{
	if (!phy)
		return 0;
	return -ENOSYS;
}

static inline int phy_power_off(struct phy *phy)
{
	if (!phy)
		return 0;
	return -ENOSYS;
}

static inline int phy_set_mode(struct phy *phy, enum phy_mode mode)
{
	if (!phy)
		return 0;
	return -ENOSYS;
}

static inline int phy_reset(struct phy *phy)
{
	if (!phy)
		return 0;
	return -ENOSYS;
}

static inline int phy_calibrate(struct phy *phy)
{
	if (!phy)
		return 0;
	return -ENOSYS;
}

static inline int phy_get_bus_width(struct phy *phy)
{
	return -ENOSYS;
}

static inline void phy_set_bus_width(struct phy *phy, int bus_width)
{
	return;
}

static inline struct phy *phy_get(struct device *dev, const char *string)
{
	return ERR_PTR(-ENOSYS);
}

static inline struct phy *phy_optional_get(struct device *dev,
					   const char *string)
{
	return ERR_PTR(-ENOSYS);
}

static inline struct phy *devm_phy_get(struct device *dev, const char *string)
{
	return ERR_PTR(-ENOSYS);
}

static inline struct phy *devm_phy_optional_get(struct device *dev,
						const char *string)
{
	return ERR_PTR(-ENOSYS);
}

static inline struct phy *devm_of_phy_get(struct device *dev,
					  struct device_node *np,
					  const char *con_id)
{
	return ERR_PTR(-ENOSYS);
}

static inline struct phy *devm_of_phy_get_by_index(struct device *dev,
						   struct device_node *np,
						   int index)
{
	return ERR_PTR(-ENOSYS);
}

static inline void phy_put(struct phy *phy)
{
}

static inline void devm_phy_put(struct device *dev, struct phy *phy)
{
}

static inline struct phy *of_phy_get(struct device_node *np, const char *con_id)
{
	return ERR_PTR(-ENOSYS);
}

static inline struct phy *of_phy_simple_xlate(struct device *dev,
	struct of_phandle_args *args)
{
	return ERR_PTR(-ENOSYS);
}

static inline struct phy *phy_create(struct device *dev,
				     struct device_node *node,
				     const struct phy_ops *ops)
{
	return ERR_PTR(-ENOSYS);
}

static inline struct phy *devm_phy_create(struct device *dev,
					  struct device_node *node,
					  const struct phy_ops *ops)
{
	return ERR_PTR(-ENOSYS);
}

static inline void phy_destroy(struct phy *phy)
{
}

static inline void devm_phy_destroy(struct device *dev, struct phy *phy)
{
}

static inline struct phy_provider *__of_phy_provider_register(
	struct device *dev, struct device_node *children, struct module *owner,
	struct phy * (*of_xlate)(struct device *dev,
				 struct of_phandle_args *args))
{
	return ERR_PTR(-ENOSYS);
}

static inline struct phy_provider *__devm_of_phy_provider_register(struct device
	*dev, struct device_node *children, struct module *owner,
	struct phy * (*of_xlate)(struct device *dev,
				 struct of_phandle_args *args))
{
	return ERR_PTR(-ENOSYS);
}

static inline void of_phy_provider_unregister(struct phy_provider *phy_provider)
{
}

static inline void devm_of_phy_provider_unregister(struct device *dev,
	struct phy_provider *phy_provider)
{
}
static inline int
phy_create_lookup(struct phy *phy, const char *con_id, const char *dev_id)
{
	return 0;
}
static inline void phy_remove_lookup(struct phy *phy, const char *con_id,
				     const char *dev_id) { }
#endif

#endif /* __DRIVERS_PHY_H */

ifeq ($(CONFIG_ARCH_MXC_ARM64), y)
    DTC_FLAGS ?= -@ -H epapr
endif

dtb-$(CONFIG_ARCH_FSL_IMX8MQ) += nd108t-hdmi.dtb \
                                 nd108t-dual-display-edp.dtb \
                                 nd108t-dual-display-lvds-1gb.dtb \
                                 nd108t-hdmi-1gb.dtb \
                                 nd108t-dual-display-lvds.dtb \
                                 nd108t-lvds-only.dtb \
                                 nd108t-lvds-only-1gb.dtb \
                                 nd108t-dual-display-lvds-only-1gb.dtb \
                                 nd108t-dual-display-lvds-only.dtb \

dts-dirs += overlays

always      := $(dtb-y)
subdir-y    := $(dts-dirs)
clean-files := *.dtb

#!/bin/bash

# Toolchain
toolchain=../srf-imx-tools/gcc-linaro-7.5.0-2019.12-x86_64_aarch64-linux-gnu/bin
export PATH=${toolchain}:$PATH

# Kernel param
export ARCH=arm64
export CROSS_COMPILE=aarch64-linux-gnu-

# Build
make thermofisher_dragonball_defconfig
make -j`nproc` Image modules dtbs

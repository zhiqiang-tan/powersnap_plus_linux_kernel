#!/bin/bash
############################################################################################
### FILE:       debgen.sh
### PURPOSE:    Generate deb package for imx8 kernel
############################################################################################

project=dragonball
project_Uppercase=Dragonball

########################################################################
### FUNCTION:   usage()
### PURPOSE:    Display usage information
function usage()
{
    [ "${1}" ] && echo "${0}: $*" >&2
    echo "Usage: $0 <version>"
    echo "Create an i.MX 8 kernel distribution package/archive"
    echo ""
    exit 1
}

### Validate arguments
    version=$1

    [ "${version}" ] || usage "Version number missing"


########################################################################
# Toolchain
toolchain=../srf-imx-tools/gcc-linaro-7.5.0-2019.12-x86_64_aarch64-linux-gnu/bin
export PATH=${toolchain}:$PATH

# outdir
topdir=${PWD}
outdir=${PWD}/.output
moddir=${outdir}/modules
knldir=${outdir}/kernel
dtbdir=${outdir}/dtb

echo "Preparing directory"
rm -rf      ${outdir}
mkdir -p    ${moddir}
mkdir -p    ${knldir}
mkdir -p    ${dtbdir}

# Kernel param
export ARCH=arm64
export CROSS_COMPILE=aarch64-linux-gnu-
export INSTALL_MOD_PATH=${moddir}
export INSTALL_DTBS_PATH=${dtbdir}

echo "Copy modules"
make modules_install
kernelversionstring=$(ls ${moddir}/lib/modules/)  # example: 4.14.98-gab7ada625a30
kernelversion=$(echo $kernelversionstring | awk -F "-" '{print $1}')  # example: 4.14.98
rm -rf ${moddir}/lib/modules/${kernelversionstring}/build  # remove soft link to build directory on build machine
rm -rf ${moddir}/lib/modules/${kernelversionstring}/source  # remove soft link to source directory on build machine
sync

echo "Copy kernel"
cp arch/arm64/boot/Image ${knldir}/Image
sync

echo "Copy dtb"
cp arch/arm64/boot/dts/mitac/*.dtb              ${dtbdir}
cp arch/arm64/boot/dts/mitac/overlays/*.dtbo    ${dtbdir}
sync

########################################################################
echo "Generate preinst"
preinstfile="${outdir}"/preinst

function preinst_item()
{
    echo "dpkg-divert --package dpkgkernelhack --divert /usr/share/dpkgkernelhack/$1 --rename /boot/$1"  >> $2
}

> ${preinstfile}
echo "#!/bin/sh" >> ${preinstfile}
echo "mkdir -p /usr/share/dpkgkernelhack" >> ${preinstfile}

for entry in "${dtbdir}"/*.dtb*
do
    preinst_item "$(basename "$entry")" "${preinstfile}"
done
preinst_item "Image" "${preinstfile}"
preinst_item "COPYING.linux" "${preinstfile}"
preinst_item "config.txt" "${preinstfile}"

chmod +x ${preinstfile}

########################################################################
echo "Generate postinst"
postinstfile="${outdir}"/postinst

function postinst_item()
{
    echo "if [ -f /usr/share/dpkgkernelhack/$1 ]; then" >> ${postinstfile}
    echo "    rm -f /boot/$1" >> ${postinstfile}
    echo "    dpkg-divert --package dpkgkernelhack --remove --rename /boot/$1" >> ${postinstfile}
    echo "    sync" >> ${postinstfile}
    echo "fi" >> ${postinstfile}
}

> ${postinstfile}
echo "#!/bin/sh" >> ${postinstfile}

for entry in "${dtbdir}"/*.dtb*
do
    postinst_item "$(basename "$entry")" "${postinstfile}"
done
postinst_item "Image" "${postinstfile}"
postinst_item "COPYING.linux" "${postinstfile}"
postinst_item "config.txt" "${postinstfile}"

echo "cd /lib/modules" >> ${postinstfile}
echo "for item in *; do if [ \$(echo \"\${item}\" | awk -F \"-\" '{print \$1}') = ${kernelversion} ] && [ \"\${item}\" != ${kernelversionstring} ]; then rm -rf \"\${item}\"; fi; done" >> ${postinstfile}

chmod +x ${postinstfile}

########################################################################
echo "Generate debian package "
### Define build/source settings and file locations
    moddir="${moddir}"
    knlfile="${knldir}"/Image
    dtbdir="${dtbdir}"
    licensefile=${topdir}/COPYING
    configfile=${topdir}/dragonball_config.txt

### Define target settings and file locations
    targetdir="${outdir}"/debian
    controldir="${targetdir}"/DEBIAN
    controlfile="${controldir}"/control

    modinstalldir="${targetdir}"/
    knlinstalldir="${targetdir}"/boot/
    dtbinstalldir="${targetdir}"/boot/

    targetdeb="${topdir}"/${project}-imx8-kernel-${1}.deb

### Generate control file
    [ -d "${targetdir}" ] || rm -rf "${targetdir}"
    mkdir -p "${controldir}"
    echo "Package: ${project}-imx8-kernel" > "${controlfile}"
    echo "Version: ${1}" >> "${controlfile}"
    echo "Section: base" >> "${controlfile}"
    echo "Priority: optional" >> "${controlfile}"
    echo "Architecture: arm64" >> "${controlfile}"
    echo "Depends:" >> "${controlfile}"
    echo "Maintainer: Dinh Quoc Cuong <quoccuong.dinh@thermofisher.com>" >> "${controlfile}"
    echo "Description: Custom Linux kernel build for ${project_Uppercase} instrument" >> "${controlfile}"
    echo " Provide custom imx8 kernel in debian package Version: ${1}" >> "${controlfile}"

### Generate data structure and copy required files
    mkdir -p "${modinstalldir}"
    mkdir -p "${knlinstalldir}"
    mkdir -p "${dtbinstalldir}"

    [ -d "${moddir}" ]  || { echo "Missing modules directory."; exit 1; }
    [ -f "${knlfile}" ] || { echo "Missing kernel file."; exit 1; }
    [ -d "${dtbdir}" ]  || { echo "Missing dtb directory."; exit 1; }

    cp -r "${moddir}"/*     "${modinstalldir}"
    cp -r "${knlfile}"      "${knlinstalldir}"
    cp -r "${licensefile}"  "${knlinstalldir}"COPYING.linux
    cp -r "${configfile}"   "${knlinstalldir}"config.txt
    cp -r "${dtbdir}"/*     "${dtbinstalldir}"

    cp "${preinstfile}" "${controldir}"
    cp "${postinstfile}" "${controldir}"

### Build debian package
    fakeroot dpkg --build "${targetdir}" "${targetdeb}"
    rm -rf "${targetdir}"
    rm -rf "${outdir}"

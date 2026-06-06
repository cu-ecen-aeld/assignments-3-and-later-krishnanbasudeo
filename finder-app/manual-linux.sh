#!/bin/bash
# Script outline to install and build kernel.
# Author: Siddhant Jajoo.

set -e
set -u

OUTDIR=/tmp/aeld
KERNEL_REPO=https://git.kernel.org/pub/scm/linux/kernel/git/stable/linux.git
KERNEL_VERSION=v5.15.163
BUSYBOX_VERSION=1_33_1
BUSYBOX_VERSION_DOT=1.33.1
FINDER_APP_DIR=$(realpath $(dirname $0))
REPO_ROOT=$(realpath "${FINDER_APP_DIR}/..")
ARCH=arm64

# Prefer course toolchain name, fallback to Ubuntu package name
CROSS_COMPILE=${CROSS_COMPILE:-aarch64-none-linux-gnu-}
if ! command -v "${CROSS_COMPILE}gcc" >/dev/null 2>&1; then
    if command -v aarch64-linux-gnu-gcc >/dev/null 2>&1; then
        CROSS_COMPILE=aarch64-linux-gnu-
    else
        echo "ERROR: No aarch64 cross compiler found"
        echo "Install one using: sudo apt install gcc-aarch64-linux-gnu"
        exit 1
    fi
fi

if [ $# -lt 1 ]; then
    echo "Using default directory ${OUTDIR} for output"
else
    OUTDIR=$1
    echo "Using passed directory ${OUTDIR} for output"
fi

mkdir -p "${OUTDIR}"
OUTDIR=$(realpath "${OUTDIR}")

KERNEL_DIR="${OUTDIR}/linux-stable"
ROOTFS="${OUTDIR}/rootfs"
BUSYBOX_DIR="${OUTDIR}/busybox-${BUSYBOX_VERSION_DOT}"
BUSYBOX_TARBALL="${OUTDIR}/busybox-${BUSYBOX_VERSION_DOT}.tar.bz2"

echo "Output directory: ${OUTDIR}"
echo "Cross compiler: ${CROSS_COMPILE}"

cd "${OUTDIR}"

# --------------------------------------------------------------------
# Build Linux Kernel
# --------------------------------------------------------------------
if [ ! -d "${KERNEL_DIR}" ]; then
    echo "CLONING LINUX STABLE VERSION ${KERNEL_VERSION} IN ${OUTDIR}"
    git clone "${KERNEL_REPO}" --depth 1 --single-branch --branch "${KERNEL_VERSION}" "${KERNEL_DIR}"
fi

if [ ! -e "${KERNEL_DIR}/arch/${ARCH}/boot/Image" ]; then
    echo "Building Linux kernel ${KERNEL_VERSION}"
    cd "${KERNEL_DIR}"
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} defconfig
    make -j"$(nproc)" ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} all
else
    echo "Kernel already built. Skipping kernel build."
fi

cp "${KERNEL_DIR}/arch/${ARCH}/boot/Image" "${OUTDIR}/Image"

# --------------------------------------------------------------------
# Build BusyBox
# --------------------------------------------------------------------
cd "${OUTDIR}"

if [ ! -d "${BUSYBOX_DIR}" ]; then
    echo "Downloading BusyBox ${BUSYBOX_VERSION_DOT}"
    if [ ! -f "${BUSYBOX_TARBALL}" ]; then
        wget -O "${BUSYBOX_TARBALL}" "https://busybox.net/downloads/busybox-${BUSYBOX_VERSION_DOT}.tar.bz2"
    fi
    tar xjf "${BUSYBOX_TARBALL}" -C "${OUTDIR}"
fi

echo "Cleaning and creating rootfs"
sudo rm -rf "${ROOTFS}"
mkdir -p "${ROOTFS}"

cd "${BUSYBOX_DIR}"
make distclean
make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} defconfig

# Build BusyBox static so it does not need shared libraries in rootfs
sed -i 's/# CONFIG_STATIC is not set/CONFIG_STATIC=y/' .config
grep -q "CONFIG_STATIC=y" .config || echo "CONFIG_STATIC=y" >> .config

make -j"$(nproc)" ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE}
make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} CONFIG_PREFIX="${ROOTFS}" install

# --------------------------------------------------------------------
# Create root filesystem directories
# --------------------------------------------------------------------
cd "${ROOTFS}"

mkdir -p bin dev etc home lib proc sbin sys tmp usr/bin usr/sbin var/log
chmod 1777 tmp

# Device nodes required for initramfs
sudo mknod -m 666 "${ROOTFS}/dev/null" c 1 3 || true
sudo mknod -m 600 "${ROOTFS}/dev/console" c 5 1 || true

# --------------------------------------------------------------------
# Create init script
# --------------------------------------------------------------------
cat > "${ROOTFS}/init" << 'EOF'
#!/bin/sh

mount -t proc none /proc
mount -t sysfs none /sys

echo "Welcome to AESD Assignment 3 rootfs"
echo "Boot successful"

cd /home
exec /bin/sh

EOF

chmod +x "${ROOTFS}/init"

# --------------------------------------------------------------------
# Build and copy writer application
# --------------------------------------------------------------------
echo "Cross-compiling writer application"

make -C "${FINDER_APP_DIR}" clean
make -C "${FINDER_APP_DIR}" CROSS_COMPILE=${CROSS_COMPILE} CFLAGS="-Wall -Werror -static"

cp "${FINDER_APP_DIR}/writer" "${ROOTFS}/home/writer"

# --------------------------------------------------------------------
# Copy finder scripts and configuration
# --------------------------------------------------------------------
cp "${FINDER_APP_DIR}/finder.sh" "${ROOTFS}/home/finder.sh"
cp "${FINDER_APP_DIR}/finder-test.sh" "${ROOTFS}/home/finder-test.sh"

mkdir -p "${ROOTFS}/home/conf"
cp "${REPO_ROOT}/conf/username.txt" "${ROOTFS}/home/conf/username.txt"
cp "${REPO_ROOT}/conf/assignment.txt" "${ROOTFS}/home/conf/assignment.txt"

# Copy autorun-qemu.sh if it exists
if [ -f "${FINDER_APP_DIR}/autorun-qemu.sh" ]; then
    cp "${FINDER_APP_DIR}/autorun-qemu.sh" "${ROOTFS}/home/autorun-qemu.sh"
    chmod +x "${ROOTFS}/home/autorun-qemu.sh"
fi

chmod +x "${ROOTFS}/home/finder.sh"
chmod +x "${ROOTFS}/home/finder-test.sh"
chmod +x "${ROOTFS}/home/writer"

# Ensure finder-test.sh uses local conf folder inside rootfs/home

# --------------------------------------------------------------------
# Create initramfs
# --------------------------------------------------------------------
echo "Creating initramfs.cpio.gz"

cd "${ROOTFS}"
find . | cpio -H newc -ov --owner root:root | gzip -f > "${OUTDIR}/initramfs.cpio.gz"

echo "Build complete."
echo "Kernel image: ${OUTDIR}/Image"
echo "Initramfs:    ${OUTDIR}/initramfs.cpio.gz"
echo "Rootfs:       ${ROOTFS}"

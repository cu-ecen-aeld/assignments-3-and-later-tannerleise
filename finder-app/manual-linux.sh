#!/bin/bash
# Script outline to install and build kernel.
# Author: Siddhant Jajoo.

set -e
set -u

OUTDIR=/tmp/aeld
KERNEL_REPO=git://git.kernel.org/pub/scm/linux/kernel/git/stable/linux-stable.git
KERNEL_VERSION=v5.1.10
BUSYBOX_VERSION=1_33_1
FINDER_APP_DIR=$(realpath $(dirname $0))
ARCH=arm64
CROSS_COMPILE=aarch64-none-linux-gnu-
#Added this for the busybox dependancies in the cross compile directory. Easier to read and debug with this as the base directory
CC_SYSROOT=$(${CROSS_COMPILE}gcc -print-sysroot)

#If arguments are less than 1, use the default OUTDIR, if not, used the one specified as an argument
if [ $# -lt 1 ]
then
	echo "Using default directory ${OUTDIR} for output"
else
	OUTDIR=$1
	echo "Using passed directory ${OUTDIR} for output"
fi

#Added additional code if the creation of the output directory fails
if mkdir -p "${OUTDIR}"; then
	echo "Directory created successfully at ${OUTDIR}. Moving on"
else
	echo "Directory Creation failed at ${OUTDIR}. Failing"
	exit 1
fi

#Clone the linux kernel source directory as stated in step i1 on assgnmt 3 pt 2. Copy the files to outdir
cd "$OUTDIR"
if [ ! -d "${OUTDIR}/linux-stable" ]; then
    #Clone only if the repository does not exist.
	echo "CLONING GIT LINUX STABLE VERSION ${KERNEL_VERSION} IN ${OUTDIR}"
	git clone ${KERNEL_REPO} --depth 1 --single-branch --branch ${KERNEL_VERSION}
fi
if [ ! -e ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ]; then
    cd linux-stable
    echo "Checking out version ${KERNEL_VERSION}"
    git checkout ${KERNEL_VERSION}

    # TODO: Add your kernel build steps here
    #-j4 just tells us how many cpu cores we can use to build (so 4 in our case)
    #clean step
    echo "do I make it first-----------------------------------------------------------------------------------------------------------"
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} mrproper
    echo "do I make it second-----------------------------------------------------------------------------------------------------------"
    #defconfig
    make -j4 ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} defconfig
    echo "do I make it third-----------------------------------------------------------------------------------------------------------"
    #vmlinux (build the kernel image for booting with QEMU)
    make -j4 ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} all
    echo "FINISHED COMPILATION-----------------------------------------------------------------------------------------------------------"
    
    #modules (skipped because its too big)
    #make -j4 ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} modules
    #device tree (skipped because its too big)
    #make -j4 ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} dtbs
    
    
fi

echo "Adding the Image in outdir"
cp ${OUTDIR}/linux-stable/arch/arm64/boot/Image ${OUTDIR}

echo "Creating the staging directory for the root filesystem"
cd "$OUTDIR"
if [ -d "${OUTDIR}/rootfs" ]
then
	echo "Deleting rootfs directory at ${OUTDIR}/rootfs and starting over"
    sudo rm  -rf ${OUTDIR}/rootfs
fi

# TODO: Create necessary base directories
#The rootfs directory was deleted if it existed, so we gotta remake it
echo "CREATING BASE DIRECTORIES----------------------------------------------------------------------------------------------------------"
mkdir -p rootfs
#lets move there, providing absolute path as specified in instructions
cd "${OUTDIR}/rootfs" 
mkdir -p bin dev etc home lib lib64 proc sbin sys tmp usr var
mkdir -p usr/bin usr/lib usr/sbin
mkdir -p var/log

echo "BUSYBOX INSTALL-----------------------------------------------------------------------------------------------------------"
cd "$OUTDIR"
if [ ! -d "${OUTDIR}/busybox" ]
then
git clone git://busybox.net/busybox.git
    cd busybox
    git checkout ${BUSYBOX_VERSION}
    # TODO:  Configure busybox
    make distclean
    make defconfig
else
    cd busybox
fi

# TODO: Make and install busybox
make distclean
make defconfig
make -j4 ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE}
make -j4 CONFIG_PREFIX=${OUTDIR}/rootfs ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} install 
cd ${OUTDIR}/rootfs

echo "Library dependencies"
echo "Library dependencies-----------------------------------------------------------------------------------------------------------"
${CROSS_COMPILE}readelf -a bin/busybox | grep "program interpreter"
${CROSS_COMPILE}readelf -a bin/busybox | grep "Shared library"

# TODO: Add library dependencies to rootfs
echo "COPYING dependencies-----------------------------------------------------------------------------------------------------------"
cp ${CC_SYSROOT}/lib/ld-linux-aarch64.so.1 ${OUTDIR}/rootfs/lib
cp ${CC_SYSROOT}/lib64/libm.so.6 ${OUTDIR}/rootfs/lib64
cp ${CC_SYSROOT}/lib64/libresolv.so.2 ${OUTDIR}/rootfs/lib64
cp ${CC_SYSROOT}/lib64/libc.so.6 ${OUTDIR}/rootfs/lib64

# Appears the runner wasn't able to find these files, trying this approach as seen on the forums
#find / -type f -name "ld-linux-aarch64.so.1" -exec cp {} ${OUTDIR}/rootfs/lib/ \;
#find / -type f -name "libm.so.6" -exec cp {} ${OUTDIR}/rootfs/lib64/ \;
#find / -type f -name "libresolv.so.2" -exec cp {} ${OUTDIR}/rootfs/lib64/ \;
#find / -type f -name "libc.so.6" -exec cp {} ${OUTDIR}/rootfs/lib64/ \;

# TODO: Make device nodes
echo "DEVICE NODES-----------------------------------------------------------------------------------------------------------"
cd ${OUTDIR}/rootfs
sudo mknod -m 666 ${OUTDIR}/rootfs/dev/null c 1 3
sudo mknod -m 666 ${OUTDIR}/rootfs/dev/console c 5 1

# TODO: Clean and build the writer utility
#Moving to that repo
echo "WRITER UTIL-----------------------------------------------------------------------------------------------------------"
cd ${FINDER_APP_DIR}
make clean
make CROSS_COMPILE=${CROSS_COMPILE}

# TODO: Copy the finder related scripts and executables to the /home directory
# on the target rootfs
echo "COPYING DIRS-----------------------------------------------------------------------------------------------------------"
mkdir -p ${OUTDIR}/rootfs/home/conf
cp autorun-qemu.sh finder.sh finder-test.sh writer "${OUTDIR}/rootfs/home"
cp conf/* "${OUTDIR}/rootfs/home/conf"

# TODO: Chown the root directory
echo "CHOWN AND CREATE ROOT-----------------------------------------------------------------------------------------------------------"
cd "${OUTDIR}/rootfs"
#recursively change the owner of all files in the current directory to root:root
sudo chown -R root:root *

#Create a cpio file using an owner of root:root
find . | cpio -H newc -ov --owner root:root > ${OUTDIR}/initramfs.cpio

# TODO: Create initramfs.cpio.gz
echo "CREATING TAR-----------------------------------------------------------------------------------------------------------"
cd ${OUTDIR}
gzip -f initramfs.cpio

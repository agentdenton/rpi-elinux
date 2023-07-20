#!/bin/bash

set -e

WORKDIR="$(pwd)"
if [[ $(basename $WORKDIR) != "rpi0w_manual" ]]; then
    echo "ERROR: Launch script from the rpi0w_manual folder"
    exit 1
fi

OUT_DIR="$WORKDIR/out"
ROOTFS_DIR="$OUT_DIR/rootfs"
BOOT_FILES_DIR="$OUT_DIR/boot_files"
BOOT_MOUNT="$OUT_DIR/boot"
ROOT_MOUNT="$OUT_DIR/root"

BUSYBOX_DIR="$WORKDIR/busybox"
UBOOT_DIR="$WORKDIR/u-boot"
LINUX_DIR="$WORKDIR/linux"
FIRMWARE_DIR="$WORKDIR/firmware"
FILES_DIR="$WORKDIR/files"

device=$(ls -l /dev | grep '\ssd[a-z]' | awk '{print $NF}' | head -n1)

function show_help {
    echo "Usage: $0 [OPTIONS]"
    echo
    echo "Set up a boot environment for Raspberry Pi Zero W."
    echo
    echo "Options:"
    echo "  -h, --help        Show this help message and exit"
    echo "  --setup           Setup the directories"
    echo "  --flash           Flash the generated files to the sd card"
    echo "  --mount           Mount the partitions to the sd card"
    echo "  --umount          Unmount the partitions"
    echo "  --prep_boot       Copy boot files to boot_files dir"
    echo "  --make_rootfs     Create a root filesystem and install busybox"
    echo "  --make_ramdisk    Generate a ramdisk"
    echo "  --make_all        Generate a ready to use system"
    echo "  --run_qemu        Run qemu"
    echo
}

function setup() {
    rm -rf busybox
    rm -rf linux
    rm -rf u-boot
    rm -rf crosstool-ng
    rm -rf firmware

    sudo rm -rf $OUT_DIR

    mkdir -p $FILES_DIR

    mkdir -p $OUT_DIR
    mkdir -p $ROOT_MOUNT $BOOT_MOUNT

    mkdir -p $ROOTFS_DIR
    mkdir -p $BOOT_FILES_DIR

    git clone --depth=1 https://github.com/mirror/busybox.git
    git clone --depth=1 https://github.com/raspberrypi/linux.git
    git clone --depth=1 https://github.com/u-boot/u-boot.git
    git clone --depth=1 https://github.com/crosstool-ng/crosstool-ng.git
    svn checkout https://github.com/raspberrypi/firmware/trunk/boot firmware
    rm -rf $FIRMWARE_DIR/.svn
}

function prep_boot() {
    cp $FILES_DIR/config.txt $BOOT_FILES_DIR

    cp $FIRMWARE_DIR/bootcode.bin $BOOT_FILES_DIR
    cp $FIRMWARE_DIR/start.elf $BOOT_FILES_DIR

    local uboot_file="u-boot.bin"
    if [[ ! -e $UBOOT_DIR/$uboot_file ]]; then
        echo "ERROR: The $uboot_file does not exist. Build u-boot first"
        exit 1
    else
        cp $UBOOT_DIR/u-boot.bin $BOOT_FILES_DIR
    fi

    local linux_image="zImage"
    if [[ ! -e $LINUX_DIR/arch/arm/boot/$linux_image ]]; then
        echo "ERROR: The $linux_image does not exist. Build linux first"
        exit 1
    else
        cp $LINUX_DIR/arch/arm/boot/$linux_image $BOOT_FILES_DIR
    fi

    local dtb_file="bcm2835-rpi-zero-w.dtb"
    if [[ ! -e $LINUX_DIR/arch/arm/boot/dts/$dtb_file ]]; then
        echo "ERROR: The $dtb_file does not exist. Build linux first"
        exit 1
    else
        cp $LINUX_DIR/arch/arm/boot/dts/$dtb_file $BOOT_FILES_DIR
    fi

    $UBOOT_DIR/tools/mkimage \
        -A arm \
        -T script \
        -O linux \
        -C none \
        -n "boot cmd" \
        -d $FILES_DIR/boot.cmd $BOOT_FILES_DIR/boot.scr > /dev/null
}

function do_mount() {
    if [[ -n $device ]]; then
        echo "Found the /dev/$device"
        if sudo mount -o sync /dev/${device}1 out/boot; then
            echo "Mounted /dev/${device}1 on out/boot"
        fi
        if sudo mount -o sync /dev/${device}2 out/root; then
            echo "Mounted /dev/${device}2 on out/root"
        fi
    else
        echo "ERROR: No 'sd*' devices found"
        exit 1
    fi
}

function do_umount() {
    sudo umount $BOOT_MOUNT || echo "Unmounted /dev/${device}1 from out/boot"
    sudo umount $ROOT_MOUNT || echo "Unmounted /dev/${device}2 from out/root"
}

function flash() {
    do_mount

    sudo rm -rf $BOOT_MOUNT/*
    sudo rm -rf $ROOT_MOUNT/*

    sudo cp -r $BOOT_FILES_DIR/* $BOOT_MOUNT
    sudo cp -r $ROOTFS_DIR/* $ROOT_MOUNT

    do_umount
}

function make_rootfs() {
    sudo rm -rf $ROOTFS_DIR/*

    pushd $ROOTFS_DIR > /dev/null
        mkdir -p dev etc home mnt opt proc run srv sys tmp usr var
        mkdir -p usr/bin usr/lib usr/sbin
        mkdir -p var/log

        sudo mknod -m 666 dev/null c 1 3
        sudo mknod -m 600 dev/console c 5 1

        # Install busybox
        cp -r $BUSYBOX_DIR/_install/* .

        cp $FILES_DIR/inittab $ROOTFS_DIR/etc

        mkdir -p $ROOTFS_DIR/etc/init.d
        cp $FILES_DIR/rcS $ROOTFS_DIR/etc/init.d

        sudo chown -R root:root *
    popd > /dev/null
}

function make_ramdisk() {
    pushd $ROOTFS_DIR > /dev/null
        find . | cpio \
            -H newc \
            -o \
            --owner root:root -F $BOOT_FILES_DIR/initramfs.cpio > /dev/null
    popd > /dev/null
    gzip $BOOT_FILES_DIR/initramfs.cpio
    $UBOOT_DIR/tools/mkimage \
        -A arm \
        -O linux \
        -T ramdisk \
        -d $BOOT_FILES_DIR/initramfs.cpio.gz $BOOT_FILES_DIR/uRamdisk > /dev/null
}

function make_all() {
    setup
    prep_boot
    make_rootfs
    make_ramdisk
}

# FIXME: Does not work as expected
function run_qemu() {
    qemu-system-arm \
        -m 512M \
        -M raspi0 \
        -kernel $BOOT_FILES_DIR/zImage \
        -append "8250.nr_uarts=1 console=serial0,115200 console=tty1 \
            rdinit=/bin/sh rootfstype=ext4 rootwait" \
        -dtb $BOOT_FILES_DIR/bcm2708-rpi-zero-w.dtb \
        -initrd $ROOTFS_DIR/initramfs.cpio.gz
}

while (( "$#" )); do
    case "$1" in
        -h|--help)
            show_help
            exit 0
        ;;
        --setup)
            setup
            shift
        ;;
        --flash)
            flash
            shift
        ;;
        --mount)
            do_mount
            shift
        ;;
        --umount)
            do_umount
            shift
        ;;
        --prep_boot)
            prep_boot
            shift
        ;;
        --make_rootfs)
            make_rootfs
            shift
        ;;
        --make_ramdisk)
            make_ramdisk
            shift
        ;;
        --make_all)
            make_all
            shift
        ;;
        --run_qemu)
            run_qemu
            shift
        ;;
        *)
            echo "Unknown option: $1"
            show_help
            exit 1
        ;;
    esac
done

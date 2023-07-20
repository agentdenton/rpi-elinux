# Raspberry Pi Zero W ELinux manual setup

## Overview

This is a tutorial on how to setup minimalistic Linux system on Raspberry Pi
Zero W from scratch with u-boot and busybox.

It's based on this awesome tutorial:
https://hechao.li/2021/12/20/Boot-Raspberry-Pi-4-Using-uboot-and-Initramfs/

## Docker Setup

Setup the docker container: https://github.com/agentdenton/edev.git

## Setup Work Directory

Setup the work directories:

```bash
./cli.sh --setup
```

* `out` - This holds all the necessary files.

* `boot` - The boot partition of the SD card is mounted here.

* `root` - The root partition of the SD card is mounted here.

* `rootfs` - This contains the root file system that will be used to generate
             the initrd.

* `boot_files` - This stores all files related to booting. Files from this
                 directory will be copied over to the boot partition.

The command also downloads from GitHub all the necessary projects, such as
busybox, u-boot, Linux kernel, etc.

Result:
```
.
├── u-boot
├── out
├── linux
├── files
├── firmware
├── busybox
├── README.md
└── cli.sh
```

```
out
├── rootfs
├── boot
├── root
└── boot_files
```

## Choose correct toolchain

Select `armv6-unknown-linux-gnueabihf` toolchain. Raspberry Pi Zero W uses a
32-bit Broadcom SoC.

```bash
ct-ng armv6-unknown-linux-gnueabihf && ct-ng build
```

Also, consider adding the following lines to your `~/.bashrc` file, this will
prevent you from needing to export any variables each time you build other
projects.

```bash
export PATH="$HOME/x-tools/armv6-rpi-linux-gnueabihf/bin:$PATH"
export CROSS_COMPILE=armv6-rpi-linux-gnueabihf-
export ARCH=arm
```

It's fine to modify the `.bashrc` because the container is only for
cross-compiling.

## Building Busybox

Before building busybox, enable static build option, so we don't need to manage
any shared libraries.

Run the command below to enter the menuconfig:

```bash
make defconfig && make menuconfig
```

Navigate to `Settings -> Build static binary (no shared libs)` and enable it.

Build the busybox:

```bash
make -j$(nproc)
```

Finally, run `make install` to install Busybox into the `_install` directory.

## Building the Kernel

First, list available configuration files:

```bash
ls -la arch/arm/configs/ | grep bcm
```

Despite the rpi0-w using the Broadcom 2835 SoC, the `bcm2835_defconfig` did not
work as expected and I had issues with the MMC driver. But the
`bcmrpi_defconfig` worked fine, so I used it instead.

```bash
make bcmrpi_defconfig
```

Build the kernel:

```bash
make -j$(nproc)
```

The kernel generates `zImage` and `bcm2835-rpi-zero-w.dtb`, which we'll use to
boot our system later.

## Build U-boot

Building u-boot is basically the same, just choose the `rpi_0_w_defconfig`
config:

`make rpi_0_w_defconfig && make -j$(nproc)`

## Prepare the Output Files

Copy all of the boot files to the `boot_files` directory using the command
below:

```bash
./cli.sh --prep_boot
```

Now, generate the root filesystem:

```bash
./cli.sh --make_rootfs
```

And then, create the initrd:

```bash
./cli.sh --make_ramdisk
```

Before we proceed to flashing the SD card, let's take a moment to go over the
modifications I have made to the `cmdline`. It's important to understand
these changes and their implications for the setup process.

```
# files/boot.cmd
setenv bootargs "8250.nr_uarts=1 console=ttyS0,115200 rdinit=/sbin/init"
```

* Setting the baudrate is important, just the `console=ttyS0` setting won't
work.

* Without setting the console to `ttyS0` there won't be any output in the
console. The UART on pins 14-15 is managed by the `ttyS0` kernel driver.

* At first, I had trouble getting the console to work properly. Even though the
configuration seemed fine and the board booted correctly without the booloader,
it took me a lot of time and guesswork to find the `8250.nr_uarts=1` option.
I still don't really understand why it's necessary, but it's defined in the
device tree under `bootargs`. After adding it to the `cmdline`, the console
magically started working, and I still have no idea why. My guess is that it's
somehow related to the firmware.

## Flash the SD Card

Insert the SD card into your PC and run the `./cli.sh --flash`
command.

First, it mounts the boot partition into the `boot` directory, and `root`
partition into the `root` directory.

Then, it copies everything from the `boot_files` directory to the `boot`
directory and everything from `rootfs` to `root` directory.

Next, exit the container, and enter the dev directory from the host. Then,
execute `cli.sh --mount` command to examine the partitions of the SD card.

```
boot
 ├── config.txt
 ├── boot.scr
 ├── bcm2835-rpi-zero-w.dtb
 ├── bootcode.bin
 ├── u-boot.bin
 ├── initramfs.cpio.gz
 ├── uRamdisk
 ├── start.elf
 ├── zImage
```

```
root
├── linuxrc -> bin/busybox
├── home
├── mnt
├── var
├── tmp
├── opt
├── sys
├── usr
├── etc
├── sbin
├── srv
├── dev
├── proc
├── bin
└── run
```

If everything looks correct, execute `./cli.sh --umount` and insert the SD card
into raspberry pi. Next, verify if there are logs in the console after power on.

## Additional resources

* https://elinux.org/RPi_U-Boot
* https://hechao.li/2021/12/20/Boot-Raspberry-Pi-4-Using-uboot-and-Initramfs

# Buildroot with U-Boot on Raspberry Pi Zero W

This document is a small tutorial on how to setup **Raspberry Pi Zero W** with
Buildroot using U-boot as bootloader.

## Acquire Sources

Fetch Buildroot source code from Github:

```bash
git clone https://github.com/buildroot/buildroot.git
```

## Set Up U-Boot

First, run `make nconfig`, navigate to `Bootloaders`, then enable U-Boot.
Input the appropriate name in `Board defconfig` field without `_defconfig`
suffix. The correct entry for Raspberry Pi Zero W is `rpi_0_w`.

Under `Host utilities`, enable `host u-boot tools` to gain access to
`mkimage` utility. We need it to generate `boot.scr` later.

Next, compile the Buildroot:

```bash
make -j$(nproc)
```

Buildroot, by default, generates a `config.txt` using `zImage`, despite
enabling U-Boot in `nconfig`, so we need to change `config.txt` manually.

Navigate to `board/raspberrypi` and modify `config.txt` to use `u-boot.bin`
instead of `zImage`:

```bash
kernel=u-boot.bin
```

## Create Boot Script for U-boot

Create `boot.cmd` file with following content inside the `board/raspberrypi`:

```bash
setenv bootargs "8250.nr_uarts=1 root=/dev/mmcblk0p2 rootwait console=tty1 console=ttyS0,115200"
fatload mmc 0:1 ${kernel_addr_r} zImage
bootz ${kernel_addr_r} - ${fdt_addr}
```

- The configuration `8250.nr_uarts=1` tells the system to use a single UART.
**Without this option you won't get any logs from U-boot!**

- Switching from `ttyAMA0` to `ttyS0` is an important step because in this case,
`ttyS0` is the interface the console output is connected to.

Next, we need to generate `boot.scr` from `boot.cmd` using the `mkimage`
command. To do that, modify `post-build.sh`. This script is run after each
compilation. This is not very efficient, but it works.

```diff
diff --git a/board/raspberrypi/post-build.sh b/board/raspberrypi/post-build.sh
index 5e5eb71100..3be7f4b0a5 100755
--- a/board/raspberrypi/post-build.sh
+++ b/board/raspberrypi/post-build.sh
@@ -3,9 +3,13 @@
 set -u
 set -e

+BOARD_DIR="$BR2_EXTERNAL/board/rpi0w"
+MKIMAGE="$HOST_DIR/bin/mkimage"
+
 if [ -e ${TARGET_DIR}/etc/inittab ]; then
     grep -qE '^tty1::' ${TARGET_DIR}/etc/inittab || \
 	sed -i '/GENERIC_SERIAL/a\
 tty1::respawn:/sbin/getty -L  tty1 0 vt100 # HDMI console'
${TARGET_DIR}/etc/inittab
 fi
+
+$MKIMAGE -C none -A arm -T script -d $BOARD_DIR/boot.cmd
$BINARIES_DIR/boot.scr
```

Modify the `genimage-raspberrypi0w.cfg` to put `u-boot.bin` and `boot.scr`
files on the boot partition:

```diff
diff --git a/board/raspberrypi/genimage-raspberrypi0w.cfg
b/board/raspberrypi/genimage-raspberrypi0w.cfg
index de7652f99e..63ad64a616 100644
--- a/board/raspberrypi/genimage-raspberrypi0w.cfg
+++ b/board/raspberrypi/genimage-raspberrypi0w.cfg
@@ -8,6 +8,8 @@ image boot.vfat {
 			"rpi-firmware/fixup.dat",
 			"rpi-firmware/start.elf",
 			"rpi-firmware/overlays",
+			"u-boot.bin",
+			"boot.scr",
 			"zImage"
 		}
 	}
```

## Final steps

Update the files inside `output/image/rpi-firmware` directory, where modified
`config.txt` file resides. Running `make` won't be sufficient; execute:

```bash
make rpi-firmware-reconfigure -j$(nproc)
```

Then reassemble the image:

```bash
make -j$(nproc)
```

After successful compilation, mount `boot.vfat` and inspect the boot
partition:

```bash
sudo mount boot.vfat /mnt
```

You should see:

```
/mnt
├── cmdline.txt
├── boot.scr
├── config.txt
├── fixup.dat
├── overlays
├── bcm2708-rpi-zero-w.dtb
├── bootcode.bin
├── u-boot.bin
├── start.elf
└── zImage
```

If everything is correct, write `sdcard.img` image to the SD card:

```bash
sudo dd if=sdcard.img of=/dev/sda bs=4M status=progress
```

After powering on the Raspberry Pi Zero W, you should see the U-Boot logs
if everything is set up correctly.

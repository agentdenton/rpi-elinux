# How to configure networking:

## 1. Enable WIFI Firmware

To start configuring the wireless interface, the first step is to enable the
WIFI firmware inside Buildroot. Here's how:

- Navigate to **Target packages** -> **Hardware handling** -> **Firmware**
- Enable **brcmfmac-sdio-firmware-rpi**

## 2. Install Necessary Packages

You will also need to install some additional packages. These can be found under
**Target packages** -> **Networking applications**. Be sure to install:

- **dhcpcd**
- **wpa_supplicant**
- **openssh**

It may also be useful to install the **nano text editor** in case you need to
adjust any configurations.

## 3. Create a Root Filesystem Overlay

Next, create a root filesystem overlay in the `board/raspberrypi` directory.
Use these commands:

```
mkdir board/raspberrypi/overlay
touch etc/inittab etc/wpa_supplicant.conf
```

Afterwards, enable the overlay under **System configuration** ->
**Root filesystem overlay directories**. You need to provide the ** full path**
to your overlay directory.

## 4. Load the Firmware

Instead of manually loading the kernel WIFI modules at each startup, we can
automate this process. Add the following lines to your `etc/inittab`:

```
::sysinit:/sbin/modprobe brcmutil.ko
::sysinit:/sbin/modprobe brcmfmac.ko
```

Remember, `brcmutil` will be loaded automatically when you load `brcmfmac`.
We're explicitly loading both here for the sake of clarity.

## 5. Configure wpa_supplicant

In your `wpa_supplicant.conf` file, add the following lines:

```
ctrl_interface=DIR=/var/run/wpa_supplicant GROUP=netdev
update_config=1

network={
    ssid="xxxxxxx"
    psk="xxxxxxx"
}
```

Next, autostart the `wlan0` interface and wpa_supplicant at each boot by
adding the following commands to the `inittab`:

```
::once:/sbin/ip link set wlan0 up
::once:/usr/sbin/wpa_supplicant -B -D nl80211 -i wlan0 -c /etc/wpa_supplicant.conf
```

Try pinging your router. If it doesn't respond, attempt to acquire an IP
address with `dhcpcd wlan0` command.

## 6. Set Static IP Address

If you'd want to use a static IP address instead of relying on `dhcpcd`, use
the following commands:

```
ip addr add 192.168.0.50/24 dev wlan0
ip link set wlan0 up
ip route add default via 192.168.0.1
```

## 7. Set Up SSH

SSH setup involves setting a root password and configuring `sshd_config`. Here
are the steps:

- Set a root password in Buildroot: **System configuration** ->
**Enable root login with password**.

- Create an `ssh` directory inside the overlay:
`mkdir board/raspberrypi/overlay/etc/ssh`

- Copy `sshd_config` from your host machine to the new directory:
`cp /etc/ssh/sshd_config buildroot/board/raspberrypi/overlay/etc/ssh`

- Add `PermitRootLogin yes` and change the default path for the **sftp-server**
in `sshd_config`.

- Generate the key-pair:
	```
	ssh-keygen
	ssh-copy-id -i ~/.ssh/rpi0w.pub root@192.168.0.50
	```

    Or you can keep the key inside the overlay, so you don't need to copy it
    each time you flash image to the SD card:

	`cp ~/.ssh/id_dsa.pub buildroot/board/raspberrypi/overlay/root/.ssh`

### Additional Steps:

- **Provide a hostname for the router:** To assign a **hostname** other than
"Unknown", create `etc/dhcpcd.conf` inside the overlay and add the line
`hostname rpi0w`. To assign a hostname to the machine locally modify the
/etc/hostname: `rpi0w`

- **Decrease the boot speed:** Install `rng-tools` to reduce boot time, although
this might slightly decrease security.

- **Increase the filesystem size:** If the default filesystem size provided by
Buildroot is insufficient, manually increase the size under **Filesystem
images** > **ext2/3/4 root filesystem**

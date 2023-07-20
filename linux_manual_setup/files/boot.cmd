setenv bootargs "8250.nr_uarts=1 console=ttyS0,115200 rdinit=/sbin/init"

fatload mmc 0:1 ${kernel_addr_r} zImage
fatload mmc 0:1 ${ramdisk_addr_r} uRamdisk

bootz ${kernel_addr_r} ${ramdisk_addr_r} ${fdt_addr}

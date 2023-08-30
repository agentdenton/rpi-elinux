setenv bootargs "8250.nr_uarts=1 root=/dev/mmcblk0p2 rootwait earlyprintk=serial,ttyS0,115200 console=tty1 console=ttyS0,115200 loglevel=7 printk.devkmsg=on"
fatload mmc 0:1 ${kernel_addr_r} zImage
bootz ${kernel_addr_r} - ${fdt_addr}

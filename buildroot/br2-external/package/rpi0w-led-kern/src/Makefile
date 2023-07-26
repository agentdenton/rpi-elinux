obj-m += rpi0w-led-kern.o

all:
	make -C $(LINUX_DIR) M=$(CURDIR) modules

clean:
	make -C $(LINUX_DIR) M=$(CURDIR) clean

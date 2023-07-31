RPI0W_LED_KERN_VERSION = 0.1
RPI0W_LED_KERN_SITE = $(BR2_EXTERNAL_RPI0W_PACKAGES_PATH)/package/rpi0w-led-kern/src
RPI0W_LED_KERN_SITE_METHOD = local
RPI0W_LED_KERN_LICENSE = GPL
RPI0W_LED_KERN_LICENSE_FILES = COPYING

define RPI0W_LED_KERN_BUILD_CMDS
	$(MAKE) -C $(@D) $(LINUX_MAKE_FLAGS) KDIR=$(LINUX_DIR)
endef

define RPI0W_LED_KERN_INSTALL_CMDS
	$(INSTALL) -D -m 0755 \
		$(@D)/rpi0w-led-kern.ko \
		$(TARGET_DIR)/lib/modules/$(LINUX_VERSION)/extra/rpi0w-led-kern.ko
endef

$(eval $(kernel-module))
$(eval $(generic-package))

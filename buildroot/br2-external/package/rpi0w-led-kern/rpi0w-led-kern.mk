RPI0W_LED_KERN_VERSION = 0.1
RPI0W_LED_KERN_SITE = $(BR2_EXTERNAL_RPI0W_PACKAGES_PATH)/package/rpi0w-led-kern
RPI0W_LED_KERN_SITE_METHOD = local
RPI0W_LED_KERN_LICENSE = MIT
RPI0W_LED_KERN_LICENSE_FILES = COPYING

define RPI0W_LED_KERN_BUILD_CMDS
	$(TARGET_MAKE_ENV) $(MAKE) -C $(@D)/src LINUX_DIR="$(LINUX_DIR)"
endef

define RPI0W_LED_KERN_INSTALL_TARGET_CMDS
    $(INSTALL) -D -m 0755 \
		$(@D)/src/rpi0w-led-kern.ko \
		$(TARGET_DIR)/lib/modules/$(LINUX_VERSION)/extra/rpi0w-led-kern.ko
endef

$(eval $(kernel-module))
$(eval $(generic-package))

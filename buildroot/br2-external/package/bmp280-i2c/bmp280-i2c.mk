BMP280_I2C_VERSION = 0.1
BMP280_I2C_SITE = $(BR2_EXTERNAL_RPI0W_PACKAGES_PATH)/package/bmp280-i2c/src
BMP280_I2C_SITE_METHOD = local
BMP280_I2C_LICENSE = GPL
BMP280_I2C_LICENSE_FILES = COPYING

define BMP280_I2C_BUILD_CMDS
        $(MAKE) -C $(@D) $(LINUX_MAKE_FLAGS) KDIR=$(LINUX_DIR)
endef

define BMP280_I2C_INSTALL_CMDS
	$(INSTALL) -D -m 0755 \
		$(@D)/bmp280-i2c.ko \
		$(TARGET_DIR)/lib/modules/$(LINUX_VERSION)/extra/bmp280-i2c.ko
endef

$(eval $(kernel-module))
$(eval $(generic-package))

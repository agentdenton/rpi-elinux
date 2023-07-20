# How to Add a Local Package to Buildroot

This guide demonstrates how to add a package which creates a binary; this binary
is used to blink an LED connected to pin 4 of the Raspberry Pi Zero W.

First, create a package directory:

```bash
mkdir -p buildroot/package/rpi0w-led
```

The package should have the following structure:

```
.
├── Config.in
├── rpi0w-led.mk
└── src
   ├── Makefile
   └── main.cpp
```

The `Config.in` file tells Buildroot about your package and allows enabling it
from menuconfig. Modify `buildroot/package/Config.in` to add it to a specific
menu entry:

```bash
menu "Hardware handling"
	source "package/rpi0w-led/Config.in"
```

This adds a new menu entry in the **Hardware handling** section of menuconfig.

**Config.in:**
```
config BR2_PACKAGE_RPI0W_LED
	bool "rpi0w-led"
	help
		The program to blink an external LED
```

Now, add the `rpi0w-led.mk` file. It allows to configure and build the custom
package:

**rpi0w-led.mk:**
```
RPI0W_LED_VERSION = 0.1
RPI0W_LED_LICENSE = MIT
RPI0W_LED_SITE = $(TOPDIR)/package/rpi0w-led/src
RPI0W_LED_SITE_METHOD = local
RPI0W_LED_LICENSE_FILES = COPYING
RPI0W_LED_INSTALL_TARGET = YES

define RPI0W_LED_BUILD_CMDS
	$(MAKE) -C $(@D) CXX="$(TARGET_CXX)" CFLAGS="$(TARGET_CFLAGS)" LDFLAGS="$(TARGET_LDFLAGS)"
endef

define RPI0W_LED_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/rpi0w-led $(TARGET_DIR)/usr/bin/rpi0w-led
endef

$(eval $(generic-package))
```

Some important things to point out:

- All variables are prefixed with the package name in all uppercase letters.

- Despite `RPI0W_LED_SITE = $(TOPDIR)/package/rpi0w-led/src` specifying a local
project, Buildroot will still attempt to fetch source from the internet.
To prevent this, we need `RPI0W_LED_SITE_METHOD = local` as well.

- `RPI0W_LED_INSTALL_TARGET = YES` indicates that we want to install the binary
onto the target system.

- The `RPI0W_LED_BUILD_CMDS` variable holds the build commands for our project.
It begins by fetching the default settings for the compiler from `TARGET_CXX`,
`TARGET_CFLAGS`, and `TARGET_LDFLAGS`. These settings are then handed over to
the `Makefile` in our program under the equivalent variables `CXX`, `CFLAGS`,
and `LDFLAGS`. These settings can be used directly, or you can append to them
or modify them as necessary. For instance, you can use the **override** keyword
to add extra flags:
    ```
    override CFLAGS += -std=c++20 -Wall
    override LDFLAGS +=
    ```

Here's a generic `Makefile` to create a binary:

```
override CFLAGS += -std=c++20 -Wall
override LDFLAGS +=

BUILDDIR := build
TARGET := rpi0w-led

SOURCES := main.cpp
OBJECTS := $(addprefix $(BUILDDIR)/, $(SOURCES:.cpp=.o))

all: $(TARGET)

$(TARGET): $(OBJECTS)
	@mkdir -p $(dir $@)
	@echo "Linking $@..."
	@$(CXX) $^ -o $(TARGET) $(LDFLAGS)

$(BUILDDIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	@echo "Compiling $<..."
	@$(CXX) $(CFLAGS) -c $< -o $@

clean:
	$(RM) -r $(BUILDDIR) $(TARGET)

.PHONY: clean all
```

The `main.cpp` source code is not included in this guide. I'll provide it in
another note.

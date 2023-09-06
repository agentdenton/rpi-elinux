SUMMARY = "Control LED pin from the userspace"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

FILESEXTRAPATHS:prepend := "${RPI_ELINUX_ROOT}/shared:"

SRC_URI = "file://rpi-led-user"

S = "${WORKDIR}/rpi-led-user"

do_compile () {
	oe_runmake
}

do_install () {
    install -d ${D}${bindir}
    install -m 0755 ${S}/rpi-led ${D}${bindir}
}

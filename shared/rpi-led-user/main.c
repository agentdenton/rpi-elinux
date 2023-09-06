#include "stdio.h"
#include "stdbool.h"
#include "errno.h"
#include "unistd.h"
#include "string.h"
#include "fcntl.h"

#include "sys/ioctl.h"

#include "linux/gpio.h"

#define LED_PIN_CHIP_PATH "/dev/gpiochip0"
#define GPIO_PIN_LINE_NUM 4U

int main() {
    struct gpio_v2_line_attribute attr = {
        .id = GPIO_V2_LINE_ATTR_ID_FLAGS,
        .flags = GPIO_V2_LINE_FLAG_OUTPUT,
    };
    struct gpio_v2_line_config_attribute attr_conf = {
        .mask = 1,
        .attr = attr,
    };
    struct gpio_v2_line_config line_conf = {
        .flags = GPIO_V2_LINE_FLAG_OUTPUT,
        .num_attrs = 1,
        .attrs[0] = attr_conf,
    };
    struct gpio_v2_line_request req = {
        .offsets[0] = 4,
        .num_lines = 1,
        .config = line_conf,
    };

    int fd = open(LED_PIN_CHIP_PATH, O_RDWR);
    if (fd < 0) {
        perror("Failed to open the file desc");
        return -1;
    }

    if (ioctl(fd, GPIO_V2_GET_LINE_IOCTL, &req) < 0) {
		fprintf(stderr, "Failed to get line. %s\n", strerror(errno));
        return -1;
    }

    struct gpio_v2_line_values vals = {
        .bits = 1,
        .mask = 1,
    };
    if (ioctl(req.fd, GPIO_V2_LINE_SET_VALUES_IOCTL, &vals) < 0) {
        fprintf(stderr, "Failed to set values. %s\n", strerror(errno));
        return -1;
    }

    while (true) {
        printf("Sleeping...");
        sleep(1);
    }

    return 0;
}

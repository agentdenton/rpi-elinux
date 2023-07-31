#include "linux/module.h"
#include "linux/init.h"
#include "linux/gpio/consumer.h"
#include "linux/of_device.h"
#include "linux/timer.h"

#define BLINK_INTERVAL_MS 500U

static struct timer_list tim;
static struct gpio_desc *led_pin_desc;

static void blink_timer_cb(struct timer_list __maybe_unused *list)
{
	int val = gpiod_get_value(led_pin_desc);
	gpiod_set_value(led_pin_desc, !val);
	mod_timer(&tim, jiffies + msecs_to_jiffies(BLINK_INTERVAL_MS));
}

static int rpi0w_led_probe(struct platform_device *pdev)
{
	led_pin_desc = devm_gpiod_get(&pdev->dev, NULL, GPIOD_OUT_HIGH);
	if (IS_ERR(led_pin_desc)) {
		dev_err(&pdev->dev, "Failed to acquire the gpio descriptor\n");
		return PTR_ERR(led_pin_desc);
	}

	// set the default value for the LED
	gpiod_set_value(led_pin_desc, 0);

	timer_setup(&tim, blink_timer_cb, 0);
	mod_timer(&tim, jiffies + msecs_to_jiffies(BLINK_INTERVAL_MS));

	dev_notice(&pdev->dev, "Configured the rpi0w-led gpio\n");
	return 0;
}

static int rpi0w_led_remove(struct platform_device *pdev)
{
	if (led_pin_desc != NULL) {
		del_timer_sync(&tim);
	}
	return 0;
}

static const struct of_device_id rpi0w_led_of_match[] = {
	{.compatible = "rpi0w-led"},
	{ }
};

static struct platform_driver rpi0w_led_driver = {
	.driver = {
		.name = "rpi0w-led",
		.of_match_table = rpi0w_led_of_match,
	},
	.probe = rpi0w_led_probe,
	.remove = rpi0w_led_remove,
};

module_platform_driver(rpi0w_led_driver);

MODULE_LICENSE("GPL");

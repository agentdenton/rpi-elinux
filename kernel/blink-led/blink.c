#include "linux/module.h"
#include "linux/init.h"
#include "linux/gpio/consumer.h"
#include "linux/of_device.h"
#include "linux/delay.h"
#include "linux/timer.h"
#include "linux/workqueue.h"

#define LED_PIN_IDX 0U

#define BLINK_INTERVAL_MS 500U

static struct timer_list tim;
static struct gpio_desc *led_pin_desc;

static void blink_timer_cb(struct timer_list __maybe_unused *list)
{
    pr_debug("agent_denton: blink_timer_cb start");
    if (gpiod_get_value(led_pin_desc)) {
        gpiod_set_value(led_pin_desc, 0);
    } else {
        gpiod_set_value(led_pin_desc, 1);
    }
    if (mod_timer(&tim, jiffies + msecs_to_jiffies(BLINK_INTERVAL_MS)) < 0) {
        pr_err("agent_denton: Failed to mod_timer");
    }
    pr_debug("agent_denton: blink_timer_cb end");
}

static int blink_probe(struct platform_device *pdev)
{
    int value;
    int ret;

    pr_notice("agent_denton: blink probe start");

    led_pin_desc = devm_gpiod_get_index(
                        &pdev->dev,
                        NULL,
                        LED_PIN_IDX,
                        GPIOD_OUT_HIGH
                    );
    if (IS_ERR(led_pin_desc)) {
        pr_err(
            "agent_denton: Failed to acquire gpio number: %u. Error code: %ld",
            LED_PIN_IDX,
            PTR_ERR(led_pin_desc)
        );
        return -1;
    }

    gpiod_set_value(led_pin_desc, 1);

    timer_setup(&tim, blink_timer_cb, 0);
    if (mod_timer(&tim, jiffies + msecs_to_jiffies(BLINK_INTERVAL_MS)) < 0) {
        pr_err("agent_denton: Failed to mod_timer");
        return -1;
    }

    INIT_DELAYED_WORK(&blink_work, blink_work_cb);
    schedule_delayed_work(&blink_work, msecs_to_jiffies(BLINK_INTERVAL_MS));

    pr_notice("agent_denton: blink probe exit");
    return 0;
}

static const struct of_device_id blink_of_match[] = {
    {.compatible = "blink-led"},
    { }
};

static struct platform_driver blink_driver = {
    .driver = {
        .name = "rpi0w-blink",
        .of_match_table = blink_of_match,
    },
    .probe = blink_probe,
};

static int __init blink_init(void)
{
    pr_notice("agent_denton: blink init");
    if (enable_module) {
        pr_notice("agent_denton: blink platform driver register");
        return platform_driver_register(&blink_driver);
    } else {
        return 0;
    }
}

static void __exit blink_exit(void)
{
    pr_notice("agent_denton: blink exit");
    if (enable_module) {
        pr_notice("agent_denton: blink platform driver unregister");
        del_timer(&tim);
        cancel_delayed_work_sync(&blink_work);
        platform_driver_unregister(&blink_driver);
    }
}

module_init(blink_init);
module_exit(blink_exit);

MODULE_LICENSE("GPL");

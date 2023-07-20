#include "linux/init.h"
#include "linux/module.h"
#include "linux/of_device.h"
#include "linux/i2c.h"
#include "linux/delay.h"
#include "linux/timer.h"
#include "linux/ftrace.h"

#define BMP280_ID_REG_ADDR 0xD0

#define BMP280_RESET_REG_ADDR 0xE0
#define BMP280_RESET 0xB6

#define BMP280_CTRL_MEAS_REG_ADDR 0xF4
#define BMP280_CTRL_MEAS_MODE 0x03

#define BMP280_STATUS_REG_ADDR 0xF3
#define BMP280_STATUS_MEASURING (1 << 3U)
#define BMP280_STATUS_IM_UPDATE (1 << 0U)

#define BMP280_TEMP_MSB_REG_ADDR 0xFA
#define BMP280_TEMP_LSB_REG_ADDR 0xFB
#define BMP280_TEMP_XSLB_REG_ADDR 0xFC
#define BMP280_TEMP_XSLB 0x0F // mask for the temperature bits

#define BMP280_ID 0x58

#define READ_TEMP_INTERVAL_MS 500U

static bool enable_module = false;
module_param(enable_module, bool, 0660);
MODULE_PARM_DESC(enable_module, "Enable module");

typedef enum {
    BMP280_MODE_SLEEP = 0x0,
    BMP280_MODE_FORCED = 0x1,
    BMP280_MODE_NORMAL = 0x3,
} BMP280_Mode;

typedef struct {
    struct i2c_client *client;
    struct timer_list tim;
    struct work_struct work;
    u32 raw_temp;
} BMP280_Data;

static int bmp280_read_reg(struct i2c_client *client, u8 reg_addr)
{
    u8 rd, wr;
    int ret;

    wr = reg_addr;
    ret = i2c_master_send(client, &wr, 1);
    if (ret < 0) {
        dev_err(&client->dev, "Failed to select 0x%x register", reg_addr);
        return ret;
    }

    ret = i2c_master_recv(client, &rd, 1);
    if (ret < 0) {
        dev_err(&client->dev, "Failed to read 0x%x register", reg_addr);
        return ret;
    }

    return rd;
}

static int bmp280_read_burst(
    struct i2c_client *client,
    u8 start_reg_addr,
    u8 *rd_vals,
    u8 len
) {
    u8 reg = start_reg_addr;
    struct i2c_msg msgs[] = {
        {
            .addr = client->addr,
            .flags = 0, // write
            .len = 1,
            .buf = &reg,
        },
        {
            .addr = client->addr,
            .flags = I2C_M_RD, // read
            .len = len,
            .buf = rd_vals,
        }
    };
    int ret;

    ret = i2c_transfer(client->adapter, msgs, 2);
    if (ret != 2) {
        dev_err(&client->dev, "Failed to range read");
        return -EIO;
    }

    return 0;
}

static int bmp280_write_reg(struct i2c_client *client, u8 reg_addr, u8 val)
{
    u8 wr[2] = {reg_addr, val};
    int ret;

    ret = i2c_master_send(client, wr, 2);
    if (ret < 0) {
        dev_err(&client->dev, "Failed to write 0x%x register", reg_addr);
        return ret;
    }

    return 0;
}

static int __maybe_unused bmp280_write_burst(
    struct i2c_client *client,
    u8 start_reg_addr,
    u8 *wr_vals,
    u8 len
) {
    u8 reg = start_reg_addr;
    struct i2c_msg msgs[] = {
        {
            .addr = client->addr,
            .flags = 0, // write
            .len = 1,
            .buf = &reg,
        },
        {
            .addr = client->addr,
            .flags = 0, // write
            .len = len,
            .buf = wr_vals,
        }
    };
    int ret;

    ret = i2c_transfer(client->adapter, msgs, 2);
    if (ret != 2) {
        dev_err(&client->dev, "Failed to range write");
        return -EIO;
    }
}

static int bmp280_modify_reg(
    struct i2c_client *client,
    u8 reg_addr,
    u8 mask,
    u8 val
) {
    u8 reg_val;
    int ret;

    ret = bmp280_read_reg(client, reg_addr);
    if (ret < 0) {
        return ret;
    } else {
        reg_val = ret;
    }

    reg_val &= ~mask;
    reg_val |= (val & mask);
    return bmp280_write_reg(client, reg_addr, reg_val);
}

static int bmp280_get_id(struct i2c_client *client)
{
    int ret;

    ret = bmp280_read_reg(client, BMP280_ID_REG_ADDR);
    if (ret < 0) {
        dev_err(&client->dev, "Failed read device id");
        return ret;
    }
    dev_notice(&client->dev, "bmp280 device id 0x%x", ret);

    return ret; // id
}

static int bmp280_set_mode(struct i2c_client *client, BMP280_Mode mode)
{
    return bmp280_modify_reg(
        client,
        BMP280_CTRL_MEAS_REG_ADDR,
        BMP280_CTRL_MEAS_MODE,
        mode
    );
}

static int bmp280_reset(struct i2c_client *client)
{
    return bmp280_write_reg(client, BMP280_RESET_REG_ADDR, BMP280_RESET);
}

static int bmp280_read_temp_blocking(struct i2c_client *client)
{
#define TEMP_DATA_LEN 3
    u8 temp_data[TEMP_DATA_LEN];
    u32 temperature;
    const u32 temp_mask = 0xFFFFF;
    u8 status;
    int ret;

    // start the temperature conversion
    dev_notice(&client->dev, "Set mode to FORCED");
    bmp280_set_mode(client, BMP280_MODE_FORCED);

    // wait for conversion to finish
    do {
        status = bmp280_read_reg(client, BMP280_STATUS_REG_ADDR);
        if (status < 0) {
            dev_err(&client->dev, "Failed to read status");
            return -1;
        } else {
            msleep(10);
        }
    } while (status & BMP280_STATUS_MEASURING);

    // read data from the temperature registers
    ret = bmp280_read_burst(
        client,
        BMP280_TEMP_MSB_REG_ADDR,
        temp_data,
        TEMP_DATA_LEN
    );
    if (ret < 0) {
        dev_err(&client->dev, "Failed read temperature registers");
        return ret;
    }

    temperature = (temp_data[0] << 12) // msb
                | (temp_data[1] << 4) // lsb
                | (temp_data[2] & BMP280_TEMP_XSLB); // xlsb
    temperature &= temp_mask;
    return temperature;
}

static void bmp280_work_cb(struct work_struct *w)
{
    BMP280_Data *data = container_of(w, BMP280_Data, work);
    int ret;

    dev_notice(&data->client->dev, "Work START");
    ret = bmp280_read_temp_blocking(data->client);
    if (ret < 0) {
        dev_notice(
            &data->client->dev,
            "Failed to read temperature. Error %d",
            ret
        );
        goto err;
    } else {
        data->raw_temp = ret;
        dev_notice(
            &data->client->dev,
            "Raw Temperature: %d",
            data->raw_temp
        );
    }
    mod_timer(&data->tim, jiffies + msecs_to_jiffies(READ_TEMP_INTERVAL_MS));
    dev_notice(&data->client->dev, "Work END");
    return;
err:
    dev_err(&data->client->dev, "Work ERROR"); // don't restart the timer
}

static void bmp280_timer_cb(struct timer_list *t)
{
    BMP280_Data *data = container_of(t, BMP280_Data, tim);
    schedule_work(&data->work);
}

static int bmp280_probe(struct i2c_client *client)
{
    BMP280_Data *data;
    int ret __maybe_unused;

    dev_notice(&client->dev, "probe start");

    data = kzalloc(sizeof(BMP280_Data), GFP_KERNEL);
    if (data == NULL) {
        dev_err(&client->dev, "Failed to allocate device data structure");
        return -ENOMEM;
    }

    i2c_set_clientdata(client, data);

    data->client = client;

    if (bmp280_get_id(client) != BMP280_ID) {
        dev_err(&client->dev, "Invalid ID");
        goto err;
    }

    dev_notice(&client->dev, "Reset sensor before initialization");
    bmp280_reset(client);

    timer_setup(&data->tim, bmp280_timer_cb, 0);
    mod_timer(&data->tim, jiffies + msecs_to_jiffies(READ_TEMP_INTERVAL_MS));

    INIT_WORK(&data->work, bmp280_work_cb);

    dev_notice(&client->dev, "bmp280 probe end");
    return 0;
err:
    kfree(data);
    return -1;
}

static int bmp280_remove(struct i2c_client *client)
{
    BMP280_Data *data = i2c_get_clientdata(client);

    dev_notice(&client->dev, "remove start");

    del_timer_sync(&data->tim);
    kfree(data);

    dev_notice(&client->dev, "bmp280 remove end");
    return 0;
}

struct of_device_id bmp280_of_match[] = {
    {.compatible = "bmp280-i2c"},
    { },
};

static struct i2c_driver bmp280_driver = {
    .driver = {
        .name = "bmp280",
        .of_match_table = bmp280_of_match,
    },
    .probe_new = bmp280_probe,
    .remove = bmp280_remove,
};

static int __init bmp280_init(void)
{
    if (enable_module) {
        pr_notice("Register bmp280 i2c driver");
        return i2c_add_driver(&bmp280_driver);
    } else {
        pr_notice("Empty init");
        return 0;
    }
}

static void __exit bmp280_exit(void)
{
    if (enable_module) {
        pr_notice("Unregister bmp280 i2c driver");
        i2c_del_driver(&bmp280_driver);
    } else {
        pr_notice("Empty module exit");
    }
}

module_init(bmp280_init);
module_exit(bmp280_exit);

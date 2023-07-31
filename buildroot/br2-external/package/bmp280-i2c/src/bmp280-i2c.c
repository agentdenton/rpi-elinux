#include "linux/init.h"
#include "linux/module.h"
#include "linux/of_device.h"
#include "linux/i2c.h"
#include "linux/delay.h"
#include "linux/timer.h"

#define BMP280_ID_REG_ADDR 0xD0

#define BMP280_RESET_REG_ADDR 0xE0
#define BMP280_RESET 0xB6

#define BMP280_CTRL_MEAS_REG_ADDR 0xF4
#define BMP280_CTRL_MEAS_TEMP_OVERSAMPLING 0x1C
#define BMP280_CTRL_MEAS_MODE 0x03

#define BMP280_CONFIG_REG_ADDR 0xF5
#define BMP280_CONFIG_FILTER 0x1C

#define BMP280_STATUS_REG_ADDR 0xF3
#define BMP280_STATUS_MEASURING (1 << 3U)
#define BMP280_STATUS_IM_UPDATE (1 << 0U)

#define BMP280_TEMP_MSB_REG_ADDR 0xFA
#define BMP280_TEMP_LSB_REG_ADDR 0xFB
#define BMP280_TEMP_XSLB_REG_ADDR 0xFC
#define BMP280_TEMP_XSLB 0xF0 // mask for the temperature bits

#define BMP280_ID 0x58

#define READ_TEMP_INTERVAL_MS 500U


typedef enum {
    BMP280_MODE_SLEEP   = 0x0,
    BMP280_MODE_FORCED  = 0x1,
    BMP280_MODE_NORMAL  = 0x3,
} BMP280_Mode;

typedef enum {
    BMP280_OVERSAMPLING_SKIPPED = 0b000,
    BMP280_OVERSAMPLING_1       = 0b001,
    BMP280_OVERSAMPLING_2       = 0b010,
    BMP280_OVERSAMPLING_4       = 0b011,
    BMP280_OVERSAMPLING_8       = 0b100,
    BMP280_OVERSAMPLING_16      = 0b101,
} BMP280_Ovrsmpl;

typedef enum {
    BMP280_FILTER_OFF       = 0b000,
    BMP280_FILTER_COEFF_2   = 0b001,
    BMP280_FILTER_COEFF_4   = 0b010,
    BMP280_FILTER_COEFF_8   = 0b011,
    BMP280_FILTER_COEFF_16  = 0b100,
} BMP280_FiltCoeff;

typedef struct {
    BMP280_Ovrsmpl ovrsmpl;
    BMP280_FiltCoeff coeff;
} BMP280_Config;

typedef struct {
    struct i2c_client *client;
    struct timer_list tim;
    struct work_struct work;
    BMP280_Config config;
    u32 raw_temp;
} BMP280_Device;


static int bmp280_read_reg(struct i2c_client *client, u8 reg_addr)
{
    u8 rd, wr;
    int ret;

    wr = reg_addr;
    ret = i2c_master_send(client, &wr, 1);
    if (ret < 0) {
        dev_err(&client->dev, "Failed to select 0x%x register\n", reg_addr);
        return ret;
    }

    ret = i2c_master_recv(client, &rd, 1);
    if (ret < 0) {
        dev_err(&client->dev, "Failed to read 0x%x register\n", reg_addr);
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
        dev_err(&client->dev, "Failed to range read\n");
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
        dev_err(&client->dev, "Failed to write 0x%x register\n", reg_addr);
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
        dev_err(&client->dev, "Failed to range write\n");
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
        dev_err(&client->dev, "Failed read device id\n");
        return ret;
    }
    dev_notice(&client->dev, "bmp280 device id 0x%x\n", ret);

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

static int bmp280_set_oversampling(
    struct i2c_client *client,
    BMP280_Ovrsmpl ovrsmpl
) {
    if (ovrsmpl < BMP280_OVERSAMPLING_SKIPPED
        || ovrsmpl > BMP280_OVERSAMPLING_16) {
        dev_err(
            &client->dev,
            "Invalid oversampling value: 0x%x\n",
            ovrsmpl
        );
        return -1;
    }
    return bmp280_modify_reg(
        client,
        BMP280_CTRL_MEAS_REG_ADDR,
        BMP280_CTRL_MEAS_TEMP_OVERSAMPLING,
        BMP280_OVERSAMPLING_1
    );
}

static int bmp280_set_filter_coeff(
    struct i2c_client *client,
    BMP280_FiltCoeff coeff
) {
    if (coeff < BMP280_FILTER_OFF || coeff > BMP280_FILTER_COEFF_16) {
        dev_err(
            &client->dev,
            "Invalid filter coeff: 0x%x",
            coeff
        );
        return -1;
    }
    return bmp280_modify_reg(
        client,
        BMP280_CONFIG_REG_ADDR,
        BMP280_CONFIG_FILTER,
        BMP280_FILTER_OFF
    );
}

static int bmp280_configure(
    struct i2c_client *client,
    BMP280_Config config
) {
    if (bmp280_set_oversampling(client, config.ovrsmpl)) {
        dev_err(&client->dev, "Failed to set oversampling value\n");
        return -1;
    }
    if (bmp280_set_filter_coeff(client, config.coeff)) {
        dev_err(&client->dev, "Failed to set filter coefficient\n");
        return -1;
    }
    return 0;
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
    dev_dbg(&client->dev, "Set mode to FORCED\n");
    bmp280_set_mode(client, BMP280_MODE_FORCED);

    // wait for conversion to finish
    do {
        status = bmp280_read_reg(client, BMP280_STATUS_REG_ADDR);
        if (status < 0) {
            dev_err(&client->dev, "Failed to read device status\n");
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
        dev_err(&client->dev, "Failed read temperature registers\n");
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
    BMP280_Device *bdev = container_of(w, BMP280_Device, work);
    int raw_temp;

    raw_temp = bmp280_read_temp_blocking(bdev->client);
    if (raw_temp < 0) {
        dev_notice(
            &bdev->client->dev,
            "Failed to read temperature.\n",
            raw_temp
        );
        goto err;
    } else {
        bdev->raw_temp = raw_temp;
        dev_debug(
            &bdev->client->dev,
            "Raw Temperature: %d\n",
            bdev->raw_temp
        );
    }
    mod_timer(&bdev->tim, jiffies + msecs_to_jiffies(READ_TEMP_INTERVAL_MS));
    return;

err:
    dev_err(&bdev->client->dev, "The bmp280 work failed\n");
}

static void bmp280_timer_cb(struct timer_list *t)
{
    BMP280_Device *bdev = container_of(t, BMP280_Device, tim);
    schedule_work(&bdev->work);
}

static int bmp280_probe(struct i2c_client *client)
{
    BMP280_Device *bdev;
    BMP280_Config config = {
        .ovrsmpl = BMP280_OVERSAMPLING_1,
        .coeff = BMP280_FILTER_OFF,
    };

    bdev = kzalloc(sizeof(BMP280_Device), GFP_KERNEL);
    if (bdev == NULL) {
        dev_err(&client->dev, "Failed to allocate bmp280 device structure\n");
        return -ENOMEM;
    }
    i2c_set_clientdata(client, bdev);

    bdev->client = client;
    bdev->config = config;

    if (bmp280_get_id(client) != BMP280_ID) {
        dev_err(&client->dev, "Invalid ID\n");
        goto err;
    }

    // Reset the sensor before initialization
    bmp280_reset(client);

    if (bmp280_configure(bdev->client, bdev->config)) {
        dev_err(&client->dev, "Failed to configure bmp280\n");
        goto err;
    }
    dev_info(&client->dev, "Configured the bmp280 sensor");

    timer_setup(&bdev->tim, bmp280_timer_cb, 0);
    mod_timer(&bdev->tim, jiffies + msecs_to_jiffies(READ_TEMP_INTERVAL_MS));

    INIT_WORK(&bdev->work, bmp280_work_cb);
    return 0;

err:
    kfree(bdev);
    return -1;
}

static void bmp280_remove(struct i2c_client *client)
{
    BMP280_Device *bdev = i2c_get_clientdata(client);

    cancel_work_sync(&bdev->work);
    del_timer_sync(&bdev->tim);
    kfree(bdev);
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

module_i2c_driver(bmp280_driver);

MODULE_LICENSE("GPL");

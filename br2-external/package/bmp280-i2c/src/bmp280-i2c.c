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
#define BMP280_CTRL_MEAS_TEMP_OVERSAMPLING 0xE0
#define BMP280_CTRL_MEAS_MODE 0x03

#define BMP280_CONFIG_REG_ADDR 0xF5
#define BMP280_CONFIG_FILTER 0x1C
#define BMP280_CONFIG_STANDBY_TIME 0xE0

#define BMP280_STATUS_REG_ADDR 0xF3
#define BMP280_STATUS_MEASURING (1 << 3U)
#define BMP280_STATUS_IM_UPDATE (1 << 0U)

#define BMP280_TEMP_MSB_REG_ADDR 0xFA
#define BMP280_TEMP_LSB_REG_ADDR 0xFB
#define BMP280_TEMP_XSLB_REG_ADDR 0xFC
#define BMP280_TEMP_XSLB 0xF0

#define BMP280_CALIB00_REG_ADDR 0x88

#define BMP280_ID 0x58

#define READ_TEMP_INTERVAL_MS 1000U


typedef enum {
    BMP280_STANDBY_0_5    = 0b000 << 5,
    BMP280_STANDBY_62_5   = 0b001 << 5,
    BMP280_STANDBY_125    = 0b010 << 5,
    BMP280_STANDBY_250    = 0b011 << 5,
    BMP280_STANDBY_500    = 0b100 << 5,
    BMP280_STANDBY_1000   = 0b101 << 5,
    BMP280_STANDBY_2000   = 0b110 << 5,
    BMP280_STANDBY_4000   = 0b111 << 5,
} BMP280StandbyTime;

typedef enum {
    BMP280_MODE_SLEEP   = 0b00,
    BMP280_MODE_FORCED  = 0b01,
    BMP280_MODE_NORMAL  = 0b11,
} BMP280Mode;

typedef enum {
    BMP280_OVERSAMPLING_SKIPPED = 0b000 << 5,
    BMP280_OVERSAMPLING_1       = 0b001 << 5,
    BMP280_OVERSAMPLING_2       = 0b010 << 5,
    BMP280_OVERSAMPLING_4       = 0b011 << 5,
    BMP280_OVERSAMPLING_8       = 0b100 << 5,
    BMP280_OVERSAMPLING_16      = 0b101 << 5,
} BMP280Ovrsmpl;

typedef enum {
    BMP280_FILTER_OFF       = 0b000 << 2,
    BMP280_FILTER_COEFF_2   = 0b001 << 2,
    BMP280_FILTER_COEFF_4   = 0b010 << 2,
    BMP280_FILTER_COEFF_8   = 0b011 << 2,
    BMP280_FILTER_COEFF_16  = 0b100 << 2,
} BMP280FiltCoeff;

typedef struct {
    u16 dig_t1;
    s16 dig_t2;
    s16 dig_t3;
} BMP280CalibParam;

typedef struct {
    struct i2c_client *client;
    struct timer_list tim;
    struct work_struct work;

    BMP280FiltCoeff filt_coeff;
    BMP280Ovrsmpl oversampling;
    BMP280CalibParam params;

    u32 raw_temp;
} BMP280Device;


static ssize_t bmp280_read(BMP280Device *bmp_dev, u8 reg_addr)
{
    struct i2c_client *client = bmp_dev->client;
    u8 recv;
    ssize_t ret;

    ret = i2c_master_send(client, &reg_addr, 1);
    if (ret < 0) {
        dev_err(&client->dev, "Failed to select the 0x%x register\n", reg_addr);
        return -1;
    }

    ret = i2c_master_recv(bmp_dev->client, &recv, 1);
    if (ret < 0) {
        dev_err(&client->dev, "Failed to read the 0x%x register\n", reg_addr);
        return -1;
    }

    return recv;
}

static ssize_t bmp280_read_range(
    BMP280Device *bmp_dev,
    u8 start_reg,
    u8 *dest,
    u8 len
) {
    struct i2c_client *client = bmp_dev->client;
    ssize_t ret;

    ret = i2c_master_send(client, &start_reg, 1);
    if (ret < 0) {
        dev_err(
            &client->dev,
            "Failed to select the starting register 0x%x\n",
            start_reg
        );
        return ret;
    }

    ret = i2c_master_recv(client, dest, len);
    if (ret < 0) {
        dev_err(
            &client->dev,
            "Failed to read range starting from 0x%x register\n",
            start_reg
        );
        return ret;
    }

    return 0;
}

static ssize_t bmp280_write(BMP280Device *bmp_dev, u8 reg_addr, u8 value)
{
    struct i2c_client *client = bmp_dev->client;
    u8 data[2] = {reg_addr, value};
    ssize_t ret;

    ret = i2c_master_send(client, data, 2);
    if (ret < 0) {
        dev_err(
            &client->dev,
            "Failed to write to the 0x%x register\n",
            reg_addr
        );
        return -1;
    }

    return 0;
}

static ssize_t bmp280_modify(
    BMP280Device *bmp_dev,
    u8 reg_addr,
    u8 mask,
    u8 val
) {
    struct i2c_client *client = bmp_dev->client;
    u8 orig_val, modified_val;
    ssize_t ret;

    ret = bmp280_read(bmp_dev, reg_addr);
    if (ret < 0) {
        dev_err(
            &bmp_dev->client->dev,
            "Failed to select 0x%x register for modification\n",
            reg_addr
        );
        return ret;
    }

    orig_val = (u8)ret;
    modified_val = (orig_val & ~mask) | (val & mask);

    ret = bmp280_write(bmp_dev, reg_addr, modified_val);
    if (ret < 0) {
        dev_err(
            &client->dev,
            "Failed to write modified value to 0x%x register\n",
            reg_addr
        );
        return ret;
    }

    return 0;
}

static ssize_t bmp280_get_id(BMP280Device *bmp_dev)
{
    int id;

    id = bmp280_read(bmp_dev, BMP280_ID_REG_ADDR);
    if (id < 0) {
        dev_err(&bmp_dev->client->dev, "Failed to read the device id\n");
        return -1;
    }
    return id;
}

static ssize_t bmp280_get_status(BMP280Device *bmp_dev)
{
    int status;

    status = bmp280_read(bmp_dev, BMP280_STATUS_REG_ADDR);
    if (status < 0) {
        dev_err(&bmp_dev->client->dev, "Failed to read the device status\n");
        return -1;
    }
    return status;
}

static int bmp280_set_standby_time(BMP280Device *bmp_dev, BMP280StandbyTime t)
{
    int ret;

    if (t < BMP280_STANDBY_0_5 || t > BMP280_STANDBY_4000) {
        dev_err(
            &bmp_dev->client->dev,
            "Invalid standby time value: 0x%x\n",
            t
        );
        return -1;
    }
    ret = bmp280_modify(
        bmp_dev,
        BMP280_CONFIG_REG_ADDR,
        BMP280_CONFIG_STANDBY_TIME,
        t
    );
    if (ret < 0) {
        dev_err(&bmp_dev->client->dev, "Failed to set standby time\n");
        return -1;
    }

    return 0;
}

static int bmp280_set_power_mode(BMP280Device *bmp_dev, BMP280Mode mode)
{
    int ret;

    ret = bmp280_modify(
        bmp_dev,
        BMP280_CTRL_MEAS_REG_ADDR,
        BMP280_CTRL_MEAS_MODE,
        mode
    );
    if (ret < 0) {
        dev_err(&bmp_dev->client->dev, "Failed to set power mode\n");
        return -1;
    }

    return 0;
}

static int bmp280_reset(BMP280Device *bmp_dev)
{
    int ret;

    ret = bmp280_write(bmp_dev, BMP280_RESET_REG_ADDR, BMP280_RESET);
    if (ret < 0) {
        dev_err(&bmp_dev->client->dev, "Failed to reset the device\n");
        return -1;
    }

    return 0;
}

static int bmp280_set_oversampling(BMP280Device *bmp_dev, BMP280Ovrsmpl ovrsmpl)
{
    int ret;

    if (ovrsmpl < BMP280_OVERSAMPLING_SKIPPED
        || ovrsmpl > BMP280_OVERSAMPLING_16)
    {
        dev_err(
            &bmp_dev->client->dev,
            "Invalid oversampling value: 0x%x\n",
            ovrsmpl
        );
        return -1;
    }
    ret = bmp280_modify(
        bmp_dev,
        BMP280_CTRL_MEAS_REG_ADDR,
        BMP280_CTRL_MEAS_TEMP_OVERSAMPLING,
        ovrsmpl
    );
    if (ret < 0) {
        dev_err(
            &bmp_dev->client->dev,
            "Failed to set oversampling to 0x%x\n",
            ovrsmpl
        );
    }

    return 0;
}

static int bmp280_set_filter_coeff(
    BMP280Device *bmp_dev,
    BMP280FiltCoeff coeff
) {
    int ret;

    if (coeff < BMP280_FILTER_OFF || coeff > BMP280_FILTER_COEFF_16) {
        dev_err(
            &bmp_dev->client->dev,
            "Invalid filter coeff value: 0x%x\n",
            coeff
        );
        return -1;
    }

    ret = bmp280_modify(
        bmp_dev,
        BMP280_CONFIG_REG_ADDR,
        BMP280_CONFIG_FILTER,
        coeff
    );
    if (ret < 0) {
        dev_err(
            &bmp_dev->client->dev,
            "Failed to set oversampling to 0x%x\n",
            coeff
        );
        return -1;
    }

    return 0;
}

static ssize_t bmp280_populate_calib_params(BMP280Device *bmp_dev)
{
#define CALIB_PARAM_BUF_LEN 6
    BMP280CalibParam *params = &bmp_dev->params;
    uint8_t buf[CALIB_PARAM_BUF_LEN];
    int ret;

    ret = bmp280_read_range(
        bmp_dev,
        BMP280_CALIB00_REG_ADDR,
        buf,
        CALIB_PARAM_BUF_LEN
    );
    if (ret < 0) {
        dev_err(
            &bmp_dev->client->dev,
            "Failed to read temperature calibration parameters\n"
        );
        return -1;
    }

    params->dig_t1 = (uint16_t)(buf[1] << 8) | buf[0];
    params->dig_t2 = (int16_t)(buf[3] << 8) | buf[2];
    params->dig_t3 = (int16_t)(buf[5] << 8) | buf[4];
    return 0;
}

static int bmp280_init(BMP280Device *bmp_dev) {
    ssize_t id;

    // Reset the sensor before initialization
    bmp280_reset(bmp_dev);

    id = bmp280_get_id(bmp_dev);
    if (id != BMP280_ID) {
        dev_err(&bmp_dev->client->dev, "Invalid ID\n");
        return -1;
    }
    dev_info(&bmp_dev->client->dev, "Found device. Device id: 0x%x\n", id);

    bmp280_set_oversampling(bmp_dev, BMP280_OVERSAMPLING_1);
    bmp280_set_filter_coeff(bmp_dev, BMP280_FILTER_COEFF_16);
    bmp280_set_standby_time(bmp_dev, BMP280_STANDBY_500);
    bmp280_set_power_mode(bmp_dev, BMP280_MODE_NORMAL);

    bmp280_populate_calib_params(bmp_dev);
    return 0;
}

static ssize_t bmp280_read_temp_blocking(BMP280Device *bmp_dev)
{
#define TEMP_DATA_LEN 3
    u8 temp_data[TEMP_DATA_LEN];
    const u32 temp_mask = 0xFFFFF;
    u32 temp;
    ssize_t ret;
    struct i2c_client *client = bmp_dev->client;

    bmp280_set_power_mode(bmp_dev, BMP280_MODE_FORCED);

    // TODO: add a timeout
    for (;;) {
        if (bmp280_get_status(bmp_dev) & BMP280_STATUS_MEASURING) {
            msleep(10);
        } else {
            break;
        }
    }

    // read data from the temperature registers
    ret = bmp280_read_range(
        bmp_dev,
        BMP280_TEMP_MSB_REG_ADDR,
        temp_data,
        TEMP_DATA_LEN
    );
    if (ret < 0) {
        dev_err(&client->dev, "Failed read the temperature registers\n");
        return -1;
    }

    temp = (temp_data[0] << 12) // msb
            | (temp_data[1] << 4) // lsb
            | (temp_data[2] >> 4); // xlsb
    temp &= temp_mask;
    return temp;
}

static void bmp280_work_cb(struct work_struct *w)
{
    BMP280Device *bmp_dev = container_of(w, BMP280Device, work);
    ssize_t raw_temp = 0;

    raw_temp = bmp280_read_temp_blocking(bmp_dev);
    if (raw_temp < 0) {
        dev_err(&bmp_dev->client->dev, "Failed to read temperature.\n");
        goto err;
    }

    bmp_dev->raw_temp = raw_temp;

    mod_timer(&bmp_dev->tim, jiffies + msecs_to_jiffies(READ_TEMP_INTERVAL_MS));
    return;

err:
    dev_err(&bmp_dev->client->dev, "The %s callback failed\n", __func__);
}

static void bmp280_timer_cb(struct timer_list *t)
{
    BMP280Device *bmp_dev = container_of(t, BMP280Device, tim);
    schedule_work(&bmp_dev->work);
}

ssize_t raw_temp_show(
    struct device *dev,
    struct device_attribute *attr,
    char *buf
) {
    struct i2c_client *client = to_i2c_client(dev);
    BMP280Device *bmp_dev = i2c_get_clientdata(client);
    return sysfs_emit(buf, "%d\n", bmp_dev->raw_temp);
}

ssize_t dig_t1_show(
    struct device *dev,
    struct device_attribute *attr,
    char *buf
) {
    struct i2c_client *client = to_i2c_client(dev);
    BMP280Device *bmp_dev = i2c_get_clientdata(client);
    return sysfs_emit(buf, "%d\n", bmp_dev->params.dig_t1);
}
ssize_t dig_t2_show(
    struct device *dev,
    struct device_attribute *attr,
    char *buf
) {
    struct i2c_client *client = to_i2c_client(dev);
    BMP280Device *bmp_dev = i2c_get_clientdata(client);
    return sysfs_emit(buf, "%d\n", bmp_dev->params.dig_t2);
}

ssize_t dig_t3_show(
    struct device *dev,
    struct device_attribute *attr,
    char *buf
) {
    struct i2c_client *client = to_i2c_client(dev);
    BMP280Device *bmp_dev = i2c_get_clientdata(client);
    return sysfs_emit(buf, "%d\n", bmp_dev->params.dig_t3);
}

DEVICE_ATTR_RO(raw_temp);
DEVICE_ATTR_RO(dig_t1);
DEVICE_ATTR_RO(dig_t2);
DEVICE_ATTR_RO(dig_t3);

static struct attribute *bmp280_temp_attrs[] = {
    &dev_attr_raw_temp.attr,
    NULL,
};

static struct attribute *bmp280_calib_attrs[] = {
    &dev_attr_dig_t1.attr,
    &dev_attr_dig_t2.attr,
    &dev_attr_dig_t3.attr,
    NULL,
};

static struct attribute_group bmp280_temp_group = {
    .attrs = bmp280_temp_attrs,
};

static struct attribute_group bmp280_calib_group = {
    .attrs = bmp280_calib_attrs,
};

static int bmp280_probe(struct i2c_client *client)
{
    BMP280Device *bmp_dev;

    bmp_dev = kzalloc(sizeof(BMP280Device), GFP_KERNEL);
    if (bmp_dev == NULL) {
        dev_err(&client->dev, "Failed to allocate the device\n");
        return -ENOMEM;
    }

    i2c_set_clientdata(client, bmp_dev);
    bmp_dev->client = client;

    if (bmp280_init(bmp_dev) < 0) {
        dev_err(&bmp_dev->client->dev, "Failed to initialized the device\n");
    } else {
        dev_info(&bmp_dev->client->dev, "Initialized the device\n");
    }

    timer_setup(&bmp_dev->tim, bmp280_timer_cb, 0);
    mod_timer(
        &bmp_dev->tim,
        jiffies + msecs_to_jiffies(READ_TEMP_INTERVAL_MS)
    );

    INIT_WORK(&bmp_dev->work, bmp280_work_cb);

    if (sysfs_create_group(&bmp_dev->client->dev.kobj, &bmp280_temp_group)) {
        dev_err(&bmp_dev->client->dev, "Failed to create a sysfs group\n");
        goto free_device;
    }

    // TODO: create a separate directory for the calibration parameters
    if (sysfs_create_group(&bmp_dev->client->dev.kobj, &bmp280_calib_group)) {
        dev_err(&bmp_dev->client->dev, "Failed to create a sysfs group\n");
        goto free_temp_group;
    }

    return 0;

free_calib_group:
    sysfs_remove_group(&client->dev.kobj, &bmp280_calib_group);
free_temp_group:
    sysfs_remove_group(&client->dev.kobj, &bmp280_temp_group);
free_device:
    kfree(bmp_dev);
    return -1;
}

static void bmp280_remove(struct i2c_client *client)
{
    BMP280Device *bmp_dev = i2c_get_clientdata(client);

    sysfs_remove_group(&client->dev.kobj, &bmp280_temp_group);
    sysfs_remove_group(&client->dev.kobj, &bmp280_calib_group);

    cancel_work_sync(&bmp_dev->work);
    del_timer_sync(&bmp_dev->tim);

    kfree(bmp_dev);
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

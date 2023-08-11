#include "stdio.h"
#include "stdlib.h"
#include "errno.h"
#include "fcntl.h"
#include "string.h"
#include "stdint.h"
#include "unistd.h"
#include "getopt.h"
#include "stdbool.h"

#define BMP280_OPTION_COEFF        (1 << 0)
#define BMP280_OPTION_OVERSAMPLING (1 << 1)
#define BMP280_OPTION_INTERVAL     (1 << 2)
#define BMP280_OPTION_MODE         (1 << 3)
#define BMP280_OPTION_GET_TEMP     (1 << 4)
#define BMP280_OPTION_GET_DATA     (1 << 5)

#define ARRAY_LEN(x) (sizeof(x) / sizeof(x[0]))

typedef enum {
    BMP280_MODE_SLEEP   = 0b00,
    BMP280_MODE_FORCED  = 0b01,
    BMP280_MODE_NORMAL  = 0b11,
} BMP280Mode;

typedef struct {
    BMP280Mode mode;

    uint32_t raw_temp;

    uint16_t dig_t1;
    int16_t dig_t2;
    int16_t dig_t3;

    uint32_t interval_ms;
    uint8_t coeff;
    uint8_t oversampling;
} BMP280DrvData;

typedef struct {
    BMP280DrvData data;
    float curr_temp;
} BMP280;

static void show_help()
{
    printf(
        "Usage: bmp280-cli [OPTIONS]\n\n"
        "The CLI to control the bmp280-i2c driver.\n\n"
        "OPTIONS:\n"
        "  --mode               Set power mode\n"
        "  --coeff              Set filter coefficient\n"
        "  --interval           Set read time interval (ms)\n"
        "  --oversampling       Set oversampling mode\n"
        "  --get_temp           Get current temperature\n"
        "  --get_data           Get current temperature\n"
        "  --help               Display this help message\n\n"
    );
    exit(EXIT_SUCCESS);
}

// TODO: Fix interval and coeff values, they are incorrect in the output
static void bmp280_print_drv_data(BMP280DrvData *data)
{
    printf("BMP280 Driver Data:\n");
    printf("Mode: %u\n", data->mode);
    printf("Raw Temp: %u\n", data->raw_temp);
    printf("dig_t1: %u\n", data->dig_t1);
    printf("dig_t2: %d\n", data->dig_t2);
    printf("dig_t3: %d\n", data->dig_t3);
    printf("Interval (ms): %d\n", data->interval_ms);
    printf("Coeff: %u\n", data->coeff);
    printf("Oversampling: %u\n", data->oversampling);
}

typedef enum {
    BMP280_PATH_MODE_FD_IDX = 0,
    BMP280_PATH_COEFF_FD_IDX,
    BMP280_PATH_INTERVAL_FD_IDX,
    BMP280_PATH_OVERSAMPLING_FD_IDX,
    BMP280_PATH_RAW_TEMP_FD_IDX,
    BMP280_PATH_DIG_T1_FD_IDX,
    BMP280_PATH_DIG_T2_FD_IDX,
    BMP280_PATH_DIG_T3_FD_IDX,
} BMP280PathFdIdx;

static int bmp280_get_drv_data(BMP280DrvData *data)
{
    // TODO: Read standby_time
    const char *paths[] = {
        "/sys/class/i2c-adapter/i2c-1/1-0076/mode",
        "/sys/class/i2c-adapter/i2c-1/1-0076/coeff",
        "/sys/class/i2c-adapter/i2c-1/1-0076/interval",
        "/sys/class/i2c-adapter/i2c-1/1-0076/oversampling",
        "/sys/class/i2c-adapter/i2c-1/1-0076/raw_temp",
        "/sys/class/i2c-adapter/i2c-1/1-0076/dig_t1",
        "/sys/class/i2c-adapter/i2c-1/1-0076/dig_t2",
        "/sys/class/i2c-adapter/i2c-1/1-0076/dig_t3",
    };
    int fds[ARRAY_LEN(paths)] = {-1};
    int ret = 0;

    const uint8_t buf_size = 255;
    char contents[ARRAY_LEN(paths)][buf_size];
    for (size_t i = 0; i < ARRAY_LEN(paths); i++) {
        fds[i] = open(paths[i], O_RDONLY);
        if (fds[i] < 0 ) {
            printf( "Failed to open %s. Err: %s\n", paths[i], strerror(errno));
            ret = -1;
            goto cleanup;
        }
        ssize_t rd = read(fds[i], contents[i], buf_size - 1);
        if (rd < 0) {
            printf(
                "Failed to read from %s. Err: %s\n",
                paths[i],
                strerror(errno)
            );
            ret = -1;
            goto cleanup;
        }
        contents[i][rd] = '\0';
    }

    data->mode = (BMP280Mode)atoi(contents[BMP280_PATH_MODE_FD_IDX]);
    data->coeff = (uint8_t)atoi(contents[BMP280_PATH_COEFF_FD_IDX]);
    data->interval_ms = (uint8_t)atoi(contents[BMP280_PATH_INTERVAL_FD_IDX]);
    data->oversampling =
        (uint8_t)atoi(contents[BMP280_PATH_OVERSAMPLING_FD_IDX]);
    data->raw_temp = (uint32_t)atoi(contents[BMP280_PATH_RAW_TEMP_FD_IDX]);
    data->dig_t1 = (uint16_t)atoi(contents[BMP280_PATH_DIG_T1_FD_IDX]);
    data->dig_t2 = (int16_t)atoi(contents[BMP280_PATH_DIG_T2_FD_IDX]);
    data->dig_t3 = (int16_t)atoi(contents[BMP280_PATH_DIG_T3_FD_IDX]);

cleanup:
    for (size_t i = 0; i < ARRAY_LEN(paths); i++) {
        if (fds[i] > 0) {
            close(fds[i]);
        }
    }
    return ret;
}

static float bmp280_convert_raw_temp(
    uint32_t raw_temp,
    uint16_t dig_t1,
    int16_t dig_t2,
    int16_t dig_t3
) {
    int32_t t1_adjusted = (raw_temp >> 3) - ((int32_t)dig_t1 << 1);
    int32_t t2_adjusted = (raw_temp >> 4) - (int32_t)dig_t1;

    int32_t var1 = (t1_adjusted * (int32_t)dig_t2) >> 11;
    int32_t var2 = (((t2_adjusted * t2_adjusted) >> 12)
                    * (int32_t)dig_t3) >> 14;

    int32_t tfine = var1 + var2;

    return (float)((tfine * 5 + 128) >> 8) / 100.0;
}

static float bmp280_get_temp(BMP280 *bmp)
{
    return bmp280_convert_raw_temp(
        bmp->data.raw_temp,
        bmp->data.dig_t1,
        bmp->data.dig_t2,
        bmp->data.dig_t3
    );
}

static void parse_args(
    int argc,
    char *argv[],
    uint32_t *flags,
    BMP280DrvData *data
) {
    if (argc == 1) {
        show_help();
        exit(EXIT_FAILURE);
    }

    static struct option long_options[] = {
        {"coeff", required_argument, 0, 'c'},
        {"oversampling", required_argument, 0, 'o'},
        {"interval", required_argument, 0, 'i'},
        {"mode", required_argument, 0, 'm'},
        {"get_temp", no_argument, 0, 'g'},
        {"get_data", no_argument, 0, 'd'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };
    int opt;
    while ((opt = getopt_long(argc, argv, "i:c:o:m:h", long_options, NULL)) != -1) {
        switch (opt) {
            case 'c':
                *flags |= BMP280_OPTION_COEFF;
                data->coeff = (uint8_t)atoi(optarg);
                break;
            case 'o':
                *flags |= BMP280_OPTION_OVERSAMPLING;
                data->oversampling = (uint8_t)atoi(optarg);
                break;
            case 'i':
                *flags |= BMP280_OPTION_INTERVAL;
                data->interval_ms = (uint32_t)atoi(optarg);
                break;
            case 'm':
                *flags |= BMP280_OPTION_MODE;
                data->mode = (BMP280Mode)atoi(optarg);
                break;
            case 'd':
                *flags |= BMP280_OPTION_GET_DATA;
                break;
            case 'g':
                *flags |= BMP280_OPTION_GET_TEMP;
                break;
            case 'h':
                show_help();
                break;
            default:
                printf("Unknown option. Type --help for more information.\n");
                exit(EXIT_FAILURE);
        }
    }
}

int main(int argc, char *argv[])
{
    uint32_t flags = 0;

    BMP280DrvData new = {0};
    parse_args(argc, argv, &flags, &new);

    BMP280DrvData curr;
    if (bmp280_get_drv_data(&curr) < 0) {
        printf("Failed to get driver data\n");
        return -1;
    }

    BMP280 bmp;
    bmp.data = curr;

    // TODO: Add guargs against the incorrect values
    if (flags & BMP280_OPTION_COEFF) {
        bmp.data.coeff = new.coeff;
    }
    if (flags & BMP280_OPTION_OVERSAMPLING) {
        bmp.data.oversampling = new.oversampling;
    }
    if (flags & BMP280_OPTION_INTERVAL) {
        bmp.data.interval_ms = new.interval_ms;
    }
    if (flags & BMP280_OPTION_MODE) {
        bmp.data.mode = new.mode;
    }

    if (flags & BMP280_OPTION_GET_TEMP) {
        float temp = bmp280_get_temp(&bmp);
        bmp.curr_temp = temp;
        printf("Temp: %f\n\n", temp);
    }

    if (flags & BMP280_OPTION_GET_DATA) {
        bmp280_print_drv_data(&bmp.data);
    }

    return 0;
}

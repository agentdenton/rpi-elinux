#include "stdio.h"
#include "stdlib.h"
#include "errno.h"
#include "fcntl.h"
#include "string.h"
#include "stdint.h"
#include "unistd.h"
#include "getopt.h"
#include "stdbool.h"

#define ARRAY_LEN(x) (sizeof(x) / sizeof(x[0]))

typedef struct {
    uint32_t raw_temp;
    uint16_t dig_t1;
    int16_t dig_t2;
    int16_t dig_t3;
} BMP280DrvData;

typedef struct {
    BMP280DrvData data;
    float curr_temp;
} BMP280;

static bool bmp280_get_temp_flag = 0;


static void show_help()
{
    printf(
        "Usage: bmp280-cli [OPTIONS]\n\n"
        "The CLI to control the bmp280-i2c driver.\n\n"
        "OPTIONS:\n"
        "  --get_temp    Get current temperature from them bmp280\n"
        "  --help        Display this help message\n\n"
    );
    exit(EXIT_SUCCESS);
}

static int bmp280_init(BMP280 *bmp)
{
    const char *paths[] = {
        "/sys/class/i2c-adapter/i2c-1/1-0076/dig_t1",
        "/sys/class/i2c-adapter/i2c-1/1-0076/dig_t2",
        "/sys/class/i2c-adapter/i2c-1/1-0076/dig_t3",
    };
    int fds[3] = {0, 0, 0};
    int vals[3] = {0, 0, 0};
    int ret = 0;

    const uint8_t buf_size = 255;
    uint8_t buf[buf_size];
    for (size_t i = 0; i < ARRAY_LEN(paths); i++) {
        fds[i] = open(paths[i], O_RDONLY);
        if (fds[i] < 0 ) {
            printf( "Failed to open %s. Err: %s\n", paths[i], strerror(errno));
            ret = -1;
            goto cleanup;
        }
        ssize_t rd = read(fds[i], buf, buf_size);
        if (rd < 0) {
            printf(
                "Failed to read from %s. Err: %s\n",
                paths[i],
                strerror(errno)
            );
            ret = -1;
            goto cleanup;
        }
        buf[rd] = '\0';
        vals[i] = atoi((const char *)buf);
    }

    bmp->data.dig_t1 = vals[0];
    bmp->data.dig_t2 = (int16_t)vals[1];
    bmp->data.dig_t3 = (int16_t)vals[2];

cleanup:
    for (size_t i = 0; i < ARRAY_LEN(paths); i++) {
        if (fds[i] != 0) {
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
    const char *path = "/sys/class/i2c-adapter/i2c-1/1-0076/raw_temp";
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        printf( "Failed to open %s. Err: %s\n", path, strerror(errno));
        return -1;
    }

    const uint8_t buf_size = 255;
    uint8_t buf[buf_size];
    ssize_t rd = read(fd, buf, buf_size);
    if (rd < 0) {
        printf( "Failed to read from %s. Err: %s\n", path, strerror(errno));
        return -1;
    }

    buf[rd] = '\0';
    uint32_t raw_temp = atoi((const char *)buf);
    return bmp280_convert_raw_temp(
        raw_temp,
        bmp->data.dig_t1,
        bmp->data.dig_t2,
        bmp->data.dig_t3
    );
}

static void parse_args(int argc, char *argv[])
{
    if (argc == 1) {
        show_help();
        return;
    }

    static struct option long_options[] = {
        {"get_temp", no_argument, 0, 'g'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };
    int opt;

    while ((opt = getopt_long(argc, argv, "h", long_options, NULL)) != -1) {
        switch (opt) {
            case 'g':
                bmp280_get_temp_flag = true;
                break;
            case 'h':
                show_help();
                break;
            default:
                printf("Unknown option. Type --help for more information.\n");
        }
    }
}

int main(int argc, char *argv[])
{
    parse_args(argc, argv);

    BMP280 bmp;
    if (bmp280_init(&bmp) < 0) {
        printf("Failed to initialize BMP280\n");
        return -1;
    }

    if (bmp280_get_temp_flag) {
        float temp = bmp280_get_temp(&bmp);
        printf("Temp: %f\n", temp);
    }

    return 0;
}

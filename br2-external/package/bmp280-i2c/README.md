### I2C pins on the board

SDA - 3
SCL - 5

### BMP280
ID register addr: 0xD0

### Reference driver
hwmon/max1619.c

### Todos

- [] read temperature using forced mode
- [] Provide multiple temperature reading modes
- [] Divide temperature reading into a separate tasks
- [] I2C interrupts
- [] Export temperature to sysfs
- [] Write a userspace program to read temperature from sysfs
- [] Overview
- [] Using ints for `read_reg` and `write_reg` potentially a bad idea

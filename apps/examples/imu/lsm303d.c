#include <nuttx/config.h>
#include <nuttx/i2c.h>
#include <stdint.h>
#include "lis302dlh.h"

#define ACCEL_ADDR  LIS302_I2C_ADDRESS_1

/* Register Addresses */
#define REG    0x0F // LIS302_WHO_AM_I

#define I2C_PORT 1
#define FREQUENCY 100000 /* 100 KHz */
#define ADDRESS_BITS 7

struct i2c_dev_s *i2c_init(int port, uint32_t frequency, int addr, int nbits)
{
        uint32_t result;
        struct i2c_dev_s *i2c;

        i2c = up_i2cinitialize(I2C_PORT);
        if (!i2c) {
                printf("nuttx-i2c: Error initializing I2C: %d\n", i2c);
                exit(0);
        }
        printf("nuttx-i2c: I2C Successfully initialized\n");

        I2C_SETFREQUENCY(i2c, frequency);
        result = I2C_SETADDRESS(i2c, addr, nbits);
        printf("Result %d\n", result);
        if (result < 0) {
                printf("nuttx-i2c: Wrong I2C Address.\n");
                exit(0);
        }
        return i2c;
}

int get_who_am_i(void)
{
        int result;
        uint8_t lthb_reg = REG;
        uint8_t temp;
        struct i2c_dev_s *i2c;

        i2c = i2c_init(I2C_PORT, FREQUENCY, ACCEL_ADDR, ADDRESS_BITS);

        result = I2C_WRITE(i2c, &lthb_reg, 1);
        if (result < 0) {
                printf("nuttx-i2c: Error Writing. Terminating\n");
                return 1;
        }

        result = I2C_READ(i2c, &temp, 1);
        // result = I2C_READ(i2c, &temp, 1);
        if (result < 0) {
                printf("nuttx-i2c: Error Reading. Terminating\n");
                return 1;
        }

        printf("WHO_AM_I register = %d\n", temp);
}

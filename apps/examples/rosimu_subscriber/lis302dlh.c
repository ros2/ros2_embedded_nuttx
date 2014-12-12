#include <nuttx/config.h>
#include <nuttx/i2c.h>
#include <stdint.h>
#include "lis302dlh.h"

#define ACCEL_ADDR  LIS302_I2C_ADDRESS_1

/* Register Addresses */
#define REG    LIS302_WHO_AM_I

#define I2C_PORT 1
#define FREQUENCY 100000 /* 100 KHz */
#define ADDRESS_BITS 7

struct i2c_dev_s *i2c;

struct i2c_dev_s *i2c_init(int port, uint32_t frequency, int addr, int nbits)
{
        uint32_t result;
        struct i2c_dev_s *i2c;

        i2c = up_i2cinitialize(I2C_PORT);
        if (!i2c) {
                printf("Error initializing I2C: %d\n", i2c);
                exit(0);
        }
        printf("I2C Successfully initialized\n");

        I2C_SETFREQUENCY(i2c, frequency);
        result = I2C_SETADDRESS(i2c, addr, nbits);
        // printf("Result %d\n", result);
        if (result < 0) {
                printf("Wrong I2C Address.\n");
                exit(0);
        }
        return i2c;
}


int write_register(uint8_t reg_val, uint8_t value)
{
  int result;
  uint8_t reg_aux;
  uint8_t data[2];

  data[0] = reg_val;
  data[1] = value;
  result = I2C_WRITE(i2c, data, 2);
  if (result < 0) {
    printf("Error Writing. Terminating\n");
    return 1;
  }

  //printf("writting 0x%02x (%d) in register 0x%02x\n", value, value, reg_val);
  return 0;
}


uint8_t read_register(uint8_t reg_val)
{
  int result;
  uint8_t temp;

  result = I2C_WRITEREAD(i2c, &reg_val, 1, &temp, 1);
  if (result < 0) {
    printf("Error Reading. Terminating\n");
    return 1;
  }
  // printf("register 0x%02x = 0x%02x (%d)\n", reg_val, temp, temp);
  return temp;
}

int get_who_am_i(void)
{
        int result;
        uint8_t lthb_reg = REG;
        uint8_t temp;

        i2c = i2c_init(I2C_PORT, FREQUENCY, ACCEL_ADDR, ADDRESS_BITS);

        result = I2C_WRITEREAD(i2c, &lthb_reg, 1, &temp, 1);
        if (result < 0) {
                printf("Error Reading. Terminating\n");
                return 1;
        }
        printf("WHO_AM_I register = 0x%02x (%d)\n", temp, temp);
}

void print_config_i2c(void)
{
  int result;
  uint8_t reg_aux = LIS302_CTRL_REG1;
  uint8_t temp;

  printf("***********************************\n");
  printf("REGISTERS:\n");
  printf("***********************************\n");
  /* Read the LIS302_CTRL_REG1 register */
  result = read_register(LIS302_CTRL_REG1); 
  printf("LIS302_CTRL_REG1 (0x%02x) register = 0x%02x (%d)\n", LIS302_CTRL_REG1, result, result);

  /* Read the LIS302_CTRL_REG2 register */
  result = read_register(LIS302_CTRL_REG2); 
  printf("LIS302_CTRL_REG2 (0x%02x) register = 0x%02x (%d)\n", LIS302_CTRL_REG2, result, result);

  /* Read the LIS302_CTRL_REG3 register */
  result = read_register(LIS302_CTRL_REG3); 
  printf("LIS302_CTRL_REG3 (0x%02x) register = 0x%02x (%d)\n", LIS302_CTRL_REG3, result, result);

  /* Read the LIS302_CTRL_REG4 register */
  result = read_register(LIS302_CTRL_REG4); 
  printf("LIS302_CTRL_REG4 (0x%02x) register = 0x%02x (%d)\n", LIS302_CTRL_REG4, result, result);

  /* Read the LIS302_CTRL_REG5 register */
  result = read_register(LIS302_CTRL_REG5); 
  printf("LIS302_CTRL_REG5 (0x%02x) register = 0x%02x (%d)\n", LIS302_CTRL_REG5, result, result);

  /* Read the LIS302_OUT_X_L register */
  result = read_register(LIS302_OUT_X_L); 
  printf("LIS302_OUT_X_L (0x%02x) register = 0x%02x (%d)\n", LIS302_OUT_X_L, result, result);

  /* Read the LIS302_OUT_X_H register */
  result = read_register(LIS302_OUT_X_H); 
  printf("LIS302_OUT_X_H (0x%02x) register = 0x%02x (%d)\n", LIS302_OUT_X_H, result, result);


  printf("***********************************\n");

}

int setup_i2c(void)
{
  int result;
  uint8_t temp;

  /* Init the i2c bus */
  i2c = i2c_init(I2C_PORT, FREQUENCY, ACCEL_ADDR, ADDRESS_BITS);
  
  // temp = (uint8_t)((1 << LIS302_PM0) | (1 << LIS302_Zen) | (1 << LIS302_Yen) | (1 << LIS302_Xen)); /* All axes, full data rate */
  temp = 0x7F; /* All axes, full data rate, power on */
  write_register(LIS302_CTRL_REG1, temp);

  temp = (uint8_t)(0);
  write_register(LIS302_CTRL_REG2, temp);
  write_register(LIS302_CTRL_REG3, temp);  

  temp = (uint8_t)(1 << LIS302_BDU);
  write_register(LIS302_CTRL_REG4, temp);
}


#define SENSITIVITY 18 

/* return value in mg */
int16_t read_accel_x(void)
{
  uint8_t result[2];

  result[0] = read_register(LIS302_OUT_X_L); 
  result[1] = read_register(LIS302_OUT_X_H); 

  // uint16_t accel_val = ((uint16_t)result[1] << 8) | result[0];
  int16_t accel_val = (int16_t)(SENSITIVITY*(int8_t)result[1]);
  // printf("Accelerometer x value = 0x%02x (%d)\n", accel_val, accel_val);
  return accel_val;  
}

/* return value in mg */
int16_t read_accel_y(void)
{
  uint8_t result[2];

  result[0] = read_register(LIS302_OUT_Y_L); 
  result[1] = read_register(LIS302_OUT_Y_H); 

  // uint16_t accel_val = ((uint16_t)result[1] << 8) | result[0];
  int16_t accel_val = (int16_t)(SENSITIVITY*(int8_t)result[1]);
  // printf("Accelerometer x value = 0x%02x (%d)\n", accel_val, accel_val);
  return accel_val;  
}

/* return value in mg */
int16_t read_accel_z(void)
{
  uint8_t result[2];

  result[0] = read_register(LIS302_OUT_Z_L); 
  result[1] = read_register(LIS302_OUT_Z_H); 

  // int16_t accel_val = ((uint16_t)result[1] << 8) | result[0];
  int16_t accel_val = (int16_t)(SENSITIVITY*(int8_t)result[1]);
  // printf("Accelerometer x value = 0x%02x (%d)\n", accel_val, accel_val);
  return accel_val;  
}

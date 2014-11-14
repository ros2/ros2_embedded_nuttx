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

void print_config(void)
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

  printf("***********************************\n");

}

int setup(void)
{
  int result;
  uint8_t temp;

  /* Init the i2c bus */
  i2c = i2c_init(I2C_PORT, FREQUENCY, ACCEL_ADDR, ADDRESS_BITS);
  
  // temp = read_register(LIS302_CTRL_REG1);
  temp = (uint8_t)((1 << LIS302_PM0) | (1 << LIS302_Zen) | (1 << LIS302_Yen) | (1 << LIS302_Xen)); /* All axes, full data rate */
  write_register(LIS302_CTRL_REG1, temp);
}


#if 0
void setup()
{
  Serial.begin(9600);
  Wire.begin();
  delay(100);
  Serial.println(lis320dlh_detect(ACCEL_ADDR) ? "LIS302DLH not found :-(" :  "LIS302DLH found :-)");
  lis320dlh_basic_setup(ACCEL_ADDR);
  lis320dlh_print_config(ACCEL_ADDR);
}

void loop()
{
  delay(500);
  
  int16_t x, y, z;
  uint8_t st_reg;
  int rc = lis320dlh_read_all(ACCEL_ADDR, &x, &y, &z, &st_reg);
  Serial.print("x= ");
  Serial.print(x);
  Serial.print(", y= ");
  Serial.print(y);
  Serial.print(", z= ");
  Serial.print(z);
  Serial.println("");
  
}

int lis320dlh_detect(uint8_t i2c_addr)
{
  Wire.beginTransmission(i2c_addr);
  Wire.write((uint8_t)LIS302_WHO_AM_I);
  Wire.endTransmission();
  Wire.requestFrom((uint8_t)i2c_addr, (uint8_t)1);
  if (Wire.available())
  { 
    uint8_t hello = Wire.read();
    return hello != LIS302_WHO_AM_I_VALUE;
  }
  return -1;
}

#define LIS302_ALL_AXES_READY ((uint8_t)((1 << LIS302_ZOR) | (1 << LIS302_YOR) | (1 << LIS302_XOR)))

int lis320dlh_read_all(uint8_t i2c_addr, int16_t* x, int16_t* y, int16_t* z, uint8_t* st_reg_out)
{
  uint8_t st_reg;
  do
  {
    Wire.beginTransmission(i2c_addr);
    Wire.write((uint8_t)LIS302_STATUS_REG);
    Wire.endTransmission();
    Wire.requestFrom(i2c_addr, (uint8_t)1);
    st_reg = Wire.read();
  } while (Wire.available() && ((st_reg & LIS302_ALL_AXES_READY) != LIS302_ALL_AXES_READY));//TODO timeout

  if ( st_reg_out != NULL )
  {
    *st_reg_out = st_reg;
  }

  Wire.beginTransmission(i2c_addr);
  Wire.write((uint8_t)(LIS302_OUT_X_L | LIS302_AUTO_INCREMENT));
  Wire.endTransmission();
  Wire.requestFrom(i2c_addr, (uint8_t)6);

  uint8_t mem[6];
  for (int i = 0; (i < 6) && Wire.available(); i++)
  {
    mem[i] = Wire.read();
//    Serial.print("0x");
//    Serial.print(mem[i], HEX);
//    Serial.print(", ");
  }
//  Serial.println("");
  *x = ((int16_t*)mem)[0];
  *y = ((int16_t*)mem)[1];
  *z = ((int16_t*)mem)[2];
  return 0;
}

int lis320dlh_basic_setup(uint8_t i2c_addr)
{
  Wire.beginTransmission(i2c_addr);
  Wire.write((uint8_t)(LIS302_CTRL_REG1 | LIS302_AUTO_INCREMENT));
  Wire.write((uint8_t)((1 << LIS302_PM0) | (1 << LIS302_Zen) | (1 << LIS302_Yen) | (1 << LIS302_Xen)));//all axes on, full data rate
  Wire.write((uint8_t)0);//reg 2
  Wire.write((uint8_t)0);//reg 3
  Wire.write((uint8_t)(1 << LIS302_BDU));//block data update
  Wire.endTransmission();
  return 0;
}

int lis320dlh_print_config(uint8_t i2c_addr)
{
  Wire.beginTransmission(i2c_addr);
  Wire.write((uint8_t)(LIS302_CTRL_REG1 | LIS302_AUTO_INCREMENT));//auto increment
  Wire.endTransmission();
  Wire.requestFrom((uint8_t)i2c_addr, (uint8_t)5);
  Serial.print("LIS302DLH CTRL_REG1-5: ");
  for (int i = 0; (i < 5) && Wire.available(); i++)
  {
    uint8_t r = Wire.read();
    Serial.print("0x");
    Serial.print(r, HEX);
    Serial.print(", ");
  }
  Serial.println("");
  return 0;
}

#endif
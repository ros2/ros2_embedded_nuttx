
//interface
#define LIS302_I2C_ADDRESS_0	0x18
#define LIS302_I2C_ADDRESS_1	0x19

#define LIS302_AUTO_INCREMENT	0x80

//registers
#define LIS302_WHO_AM_I	0x0F

#define LIS302_CTRL_REG1	0x20
#define LIS302_CTRL_REG2	0x21
#define LIS302_CTRL_REG3	0x22
#define LIS302_CTRL_REG4	0x23
#define LIS302_CTRL_REG5	0x24

#define LIS302_HP_FILTER_RESET	0x25

#define LIS302_REFERENCE	0x26

#define LIS302_STATUS_REG	0x27

#define LIS302_OUT_X_L		0x28
#define LIS302_OUT_X_H		0x29
#define LIS302_OUT_Y_L		0x2A
#define LIS302_OUT_Y_H		0x2B
#define LIS302_OUT_Z_L		0x2C
#define LIS302_OUT_Z_H		0x2D

#define LIS302_WHO_AM_I_VALUE	50

//CTRL_REG1
#define LIS302_PM2		7
#define LIS302_PM1		6
#define LIS302_PM0		5
#define LIS302_DR1		4
#define LIS302_DR0		3
#define LIS302_Zen		2
#define LIS302_Yen		1
#define LIS302_Xen		0

//CTRL_REG2
#define LIS302_BOOT		7
#define LIS302_HPM1		6
#define LIS302_HPM0		5
#define LIS302_FDS		4
#define LIS302_HPen2		3
#define LIS302_HPen1		2
#define LIS302_HPCF1		1
#define LIS302_HPCF0		0

//CTRL_REG3
#define LIS302_IHL		7
#define LIS302_PP_OD		6
#define LIS302_LIR2		5
#define LIS302_I2_CFG1		4
#define LIS302_I2_CFG0		3
#define LIS302_LIR1		2
#define LIS302_I1_CFG1		1
#define LIS302_I1_CFG0		0

//CTRL_REG4
#define LIS302_BDU		7
#define LIS302_BLE		6
#define LIS302_FS1		5
#define LIS302_FS0		4
#define LIS302_STsign		3
#define LIS302_ST		1
#define LIS302_SIM		0

//CTRL_REG5
#define LIS302_TurnOn1		1
#define LIS302_TurnOn0		0

//STATUS_REG
#define LIS302_ZYXOR		7
#define LIS302_ZOR		6
#define LIS302_YOR		5
#define LIS302_XOR		4
#define LIS302_ZYXDA		3
#define LIS302_ZDA		2
#define LIS302_YDA		1
#define LIS302_XDA		0

// Functions
int get_who_am_i(void);
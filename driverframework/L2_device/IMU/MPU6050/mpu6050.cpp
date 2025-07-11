#include "mpu6050.hpp"
#include "mpu6050_regs.h"  // 添加寄存器定义头文件
#include <cstring>

// 通用延时函数，需要外部实现
extern "C" void delay_ms(unsigned int ms);

MPU6050::MPU6050() {
  // 替换为通用初始化，不依赖RTThread
  device_.user_data = nullptr;  // 用户可以在此存储I2C设备句柄
  dev_addr_ = MPU6500_ADDRESS_AD0_HIGH;
  is_init_ = false;
}

MPU6050::~MPU6050() {
  // 不再依赖RTThread关闭设备
}

// 设置延时函数指针
void MPU6050::setDelayMs(delay_ms_func_t func) { delay_ms_ = func; }

// 内部延时函数
void MPU6050::delayMs(unsigned int ms) {
  if (delay_ms_ != nullptr) {
    delay_ms_(ms);
  }
}

int MPU6050::init() {
  if (is_init_) return MPU_EOK;

  // 重写init函数，模拟原mpu6050_init流程
  reset();  // 复位MPU6500
  delayMs(20);

  uint8_t temp = getDeviceID();        // 使用公共函数whoAmI替代内部函数getDeviceID
  if (temp == 0x68 || temp == 0x69) {  // MPU6050的正确设备ID
    // 设备ID正确
  } else {
    // 设备连接失败
    return MPU_ERROR;
  }

  setSleepEnabled(false);  // 唤醒MPU6500

  delayMs(10);
  setClockSource(MPU6500_CLOCK_PLL_XGYRO);  // 设置X轴陀螺作为时钟
  delayMs(10);

  setTempSensorEnabled(true);                    // 使能温度传感器
  setIntEnabled(false);                          // 关闭中断
  setI2CBypassEnabled(true);                     // 旁路模式，磁力计和气压连接到主IIC
  setFullScaleGyroRange(SENSORS_GYRO_FS_CFG);    // 设置陀螺量程
  setFullScaleAccelRange(SENSORS_ACCEL_FS_CFG);  // 设置加速计量程
  setAccelDLPF(MPU6500_ACCEL_DLPF_BW_41);        // 设置加速计数字低通滤波

  setRate(0);                       // 设置采样速率: 1000 / (1 + 0) = 1000Hz
  setDLPFMode(MPU6500_DLPF_BW_98);  // 设置陀螺数字低通滤波

  is_init_ = true;
  return MPU_EOK;
}

// 实现read_data函数，用于读取传感器数据
int MPU6050::read_data(int pos, void* data, int size) {
  if (data == nullptr) {
    return 0;
  }

  if (!is_init_) {
    // 如果设备未初始化，返回错误
    return MPU_ERROR;
  }

  // 这里假设pos是传感器寄存器地址，从该地址开始读取size个字节
  int8_t result =
      i2cBusRead(dev_addr_, static_cast<uint8_t>(pos), static_cast<uint8_t>(size), static_cast<uint8_t*>(data));

  if (result != 0) {
    // I2C读取出错
    return MPU_ERROR;
  }

  return size;  // 返回实际读取的字节数
}

int8_t MPU6050::setI2cBusWrite(int8_t (*func)(uint8_t, uint8_t, uint8_t*, uint8_t)) {
  if (func == nullptr) {
    return -1;
  } else {
    i2c_bus_write_ = func;
  }
  return 0;
}

int8_t MPU6050::setI2cBusRead(int8_t (*func)(uint8_t, uint8_t, uint8_t*, uint8_t)) {
  if (func == nullptr) {
    return -1;
  } else {
    i2c_bus_read_ = func;
  }
  return 0;
}

int8_t MPU6050::i2cBusRead(uint8_t devAddress, uint8_t memAddress, uint8_t len, uint8_t* data) {
  if (i2c_bus_read_) {
    return i2c_bus_read_(devAddress, memAddress, data, len);
  }
  return -1;
}

int8_t MPU6050::i2cBusWrite(uint8_t devAddress, uint8_t memAddress, uint8_t len, uint8_t* data) {
  if (i2c_bus_write_) {
    return i2c_bus_write_(devAddress, memAddress, data, len);
  }
  return -1;
}

int8_t MPU6050::i2cBusReadByte(uint8_t devAddress, uint8_t memAddress, uint8_t* data) {
  return i2cBusRead(devAddress, memAddress, 1, data);
}

int8_t MPU6050::i2cBusWriteByte(uint8_t devAddress, uint8_t memAddress, uint8_t data) {
  return i2cBusWrite(devAddress, memAddress, 1, &data);
}

void MPU6050::setI2cAddr(uint8_t addr) { dev_addr_ = addr; }

// 新增位操作函数
int8_t MPU6050::i2cdevReadBit(uint8_t devAddr, uint8_t regAddr, uint8_t bitNum, uint8_t* data) {
  uint8_t b;
  int8_t status = i2cBusReadByte(devAddr, regAddr, &b);
  if (status == 0) {
    *data = (b >> bitNum) & 0x01;
  }
  return status;
}

int8_t MPU6050::i2cdevReadBits(uint8_t devAddr, uint8_t regAddr, uint8_t bitStart, uint8_t length, uint8_t* data) {
  uint8_t b;
  uint8_t mask;
  int8_t status = i2cBusReadByte(devAddr, regAddr, &b);
  if (status == 0) {
    mask = ((1 << length) - 1) << (bitStart - length + 1);
    b &= mask;
    b >>= (bitStart - length + 1);
    *data = b;
  }
  return status;
}

int8_t MPU6050::i2cdevWriteBit(uint8_t devAddr, uint8_t regAddr, uint8_t bitNum, uint8_t data) {
  uint8_t b;
  int8_t status = i2cBusReadByte(devAddr, regAddr, &b);
  if (status == 0) {
    b = (data != 0) ? (b | (1 << bitNum)) : (b & ~(1 << bitNum));
    status = i2cBusWriteByte(devAddr, regAddr, b);
  }
  return status;
}

int8_t MPU6050::i2cdevWriteBits(uint8_t devAddr, uint8_t regAddr, uint8_t bitStart, uint8_t length, uint8_t data) {
  uint8_t b;
  uint8_t mask;
  int8_t status = i2cBusReadByte(devAddr, regAddr, &b);
  if (status == 0) {
    mask = ((1 << length) - 1) << (bitStart - length + 1);
    data <<= (bitStart - length + 1);
    data &= mask;
    b &= ~mask;
    b |= data;
    status = i2cBusWriteByte(devAddr, regAddr, b);
  }
  return status;
}

// 公开的数据获取函数
uint8_t MPU6050::getAccelerometer(int16_t* ax, int16_t* ay, int16_t* az) {
  uint8_t buf[6];
  uint8_t res = i2cBusRead(dev_addr_, MPU6500_RA_ACCEL_XOUT_H, 6, buf);
  if (res == 0) {
    *ax = ((uint16_t)buf[0] << 8) | buf[1];
    *ay = ((uint16_t)buf[2] << 8) | buf[3];
    *az = ((uint16_t)buf[4] << 8) | buf[5];
  }
  return res;
}

uint8_t MPU6050::getGyroscope(int16_t* gx, int16_t* gy, int16_t* gz) {
  uint8_t buf[6];
  uint8_t res = i2cBusRead(dev_addr_, MPU6500_RA_GYRO_XOUT_H, 6, buf);
  if (res == 0) {
    *gx = ((uint16_t)buf[0] << 8) | buf[1];
    *gy = ((uint16_t)buf[2] << 8) | buf[3];
    *gz = ((uint16_t)buf[4] << 8) | buf[5];
  }
  return res;
}

uint8_t MPU6050::getTemperature(float* temp) {
  uint8_t buf[2];
  uint8_t res = i2cBusRead(dev_addr_, MPU6500_RA_TEMP_OUT_H, 2, buf);
  if (res == 0) {
    int16_t raw = ((uint16_t)buf[0] << 8) | buf[1];
    *temp = 36.53f + ((float)raw) / 340.0f;
  }
  return res;
}

uint8_t MPU6050::selfTest() {
  uint8_t data;
  uint8_t res = i2cBusReadByte(dev_addr_, MPU6500_RA_WHO_AM_I, &data);
  if (res == 0) {
    return (data == 0x68 || data == 0x69) ? 0 : 1;  // 检查设备ID是否正确
  }
  return 1;
}

// 内部配置函数实现
void MPU6050::reset() {
  i2cdevWriteBit(dev_addr_, MPU6500_RA_PWR_MGMT_1, MPU6500_PWR1_DEVICE_RESET_BIT, 1);
  delayMs(100);  // 等待复位完成
}

uint8_t MPU6050::getDeviceID() {
  uint8_t data;
  i2cBusReadByte(dev_addr_, MPU6500_RA_WHO_AM_I, &data);
  return data;
}

void MPU6050::setSleepEnabled(bool enabled) {
  i2cdevWriteBit(dev_addr_, MPU6500_RA_PWR_MGMT_1, MPU6500_PWR1_SLEEP_BIT, enabled ? 1 : 0);
}

void MPU6050::setClockSource(uint8_t source) {
  i2cdevWriteBits(dev_addr_, MPU6500_RA_PWR_MGMT_1, MPU6500_PWR1_CLKSEL_BIT, MPU6500_PWR1_CLKSEL_LENGTH, source);
}

void MPU6050::setTempSensorEnabled(bool enabled) {
  i2cdevWriteBit(dev_addr_, MPU6500_RA_PWR_MGMT_1, MPU6500_PWR1_TEMP_DIS_BIT, enabled ? 0 : 1);
}

void MPU6050::setIntEnabled(bool enabled) { i2cBusWriteByte(dev_addr_, MPU6500_RA_INT_ENABLE, enabled ? 0x01 : 0x00); }

void MPU6050::setI2CBypassEnabled(bool enabled) {
  i2cdevWriteBit(dev_addr_, MPU6500_RA_INT_PIN_CFG, MPU6500_INTCFG_I2C_BYPASS_EN_BIT, enabled ? 1 : 0);
}

void MPU6050::setFullScaleGyroRange(uint8_t range) {
  i2cdevWriteBits(dev_addr_, MPU6500_RA_GYRO_CONFIG, MPU6500_GCONFIG_FS_SEL_BIT, 2, range);
}

void MPU6050::setFullScaleAccelRange(uint8_t range) {
  i2cdevWriteBits(dev_addr_, MPU6500_RA_ACCEL_CONFIG, MPU6500_ACONFIG_AFS_SEL_BIT, 2, range);
}

void MPU6050::setAccelDLPF(uint8_t bandwidth) {
  i2cBusWriteByte(dev_addr_, MPU6500_RA_ACCEL_CONFIG_2, bandwidth & 0x07);
}

void MPU6050::setDLPFMode(uint8_t mode) {
  i2cdevWriteBits(dev_addr_, MPU6500_RA_CONFIG, MPU6500_CFG_DLPF_CFG_BIT, 3, mode);
}

uint8_t MPU6050::setRate(uint16_t rate) {
  uint8_t data;
  if (rate > 1000) return 1;
  if (rate < 4) return 1;
  data = 1000 / rate - 1;
  return i2cBusWriteByte(dev_addr_, MPU6500_RA_SMPLRT_DIV, data);
}

// 兼容C接口
extern "C" int drv_mpu6050_i2c_init(const char* i2c_device_name, const char* device_name) {
  static MPU6050 mpu;
  return mpu.init();
}

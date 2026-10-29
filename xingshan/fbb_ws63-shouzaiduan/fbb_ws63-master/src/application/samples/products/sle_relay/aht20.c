#include "aht20.h"

#include <stddef.h>
#include <stdint.h>

#include "errcode.h"
#include "i2c.h"
#include "soc_osal.h"

#define AHT20_I2C_BUS 1
#define AHT20_I2C_ADDR 0x38
#define AHT20_STATUS_BUSY 0x80
#define AHT20_STATUS_CALIBRATED 0x08
#define AHT20_RAW_SCALE 1048576.0f

static uint32_t aht20_write(uint8_t *buffer, uint32_t length)
{
    i2c_data_t data = {0};

    data.send_buf = buffer;
    data.send_len = length;
    return uapi_i2c_master_write(AHT20_I2C_BUS, AHT20_I2C_ADDR, &data);
}

static uint32_t aht20_read(uint8_t *buffer, uint32_t length)
{
    i2c_data_t data = {0};

    data.receive_buf = buffer;
    data.receive_len = length;
    return uapi_i2c_master_read(AHT20_I2C_BUS, AHT20_I2C_ADDR, &data);
}

static uint32_t aht20_read_status(uint8_t *status)
{
    uint8_t command = 0x71;
    uint32_t ret = aht20_write(&command, sizeof(command));

    if (ret != ERRCODE_SUCC) {
        return ret;
    }
    return aht20_read(status, 1U);
}

uint32_t AHT20_Calibrate(void)
{
    uint8_t status = 0;
    uint8_t reset = 0xBA;
    uint8_t calibrate[] = {0xBE, 0x08, 0x00};
    uint32_t ret = aht20_read_status(&status);

    if (ret == ERRCODE_SUCC && (status & AHT20_STATUS_BUSY) == 0U &&
        (status & AHT20_STATUS_CALIBRATED) != 0U) {
        return ERRCODE_SUCC;
    }
    ret = aht20_write(&reset, sizeof(reset));
    if (ret != ERRCODE_SUCC) {
        return ret;
    }
    osal_msleep(20);
    ret = aht20_write(calibrate, sizeof(calibrate));
    if (ret != ERRCODE_SUCC) {
        return ret;
    }
    osal_msleep(40);
    ret = aht20_read_status(&status);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }
    return ((status & AHT20_STATUS_BUSY) == 0U &&
        (status & AHT20_STATUS_CALIBRATED) != 0U) ? ERRCODE_SUCC : ERRCODE_FAIL;
}

uint32_t AHT20_StartMeasure(void)
{
    uint8_t command[] = {0xAC, 0x33, 0x00};
    return aht20_write(command, sizeof(command));
}

uint32_t AHT20_GetMeasureResult(float *temperature, float *humidity)
{
    uint8_t data[6] = {0};
    uint32_t humidity_raw;
    uint32_t temperature_raw;
    uint32_t ret;

    if (temperature == NULL || humidity == NULL) {
        return ERRCODE_INVALID_PARAM;
    }
    ret = aht20_read(data, sizeof(data));
    if (ret != ERRCODE_SUCC || (data[0] & AHT20_STATUS_BUSY) != 0U) {
        return ret == ERRCODE_SUCC ? ERRCODE_FAIL : ret;
    }
    humidity_raw = ((uint32_t)data[1] << 12) | ((uint32_t)data[2] << 4) |
        ((uint32_t)data[3] >> 4);
    temperature_raw = (((uint32_t)data[3] & 0x0FU) << 16) |
        ((uint32_t)data[4] << 8) | data[5];
    *humidity = ((float)humidity_raw * 100.0f) / AHT20_RAW_SCALE;
    *temperature = ((float)temperature_raw * 200.0f) / AHT20_RAW_SCALE - 50.0f;
    return ERRCODE_SUCC;
}

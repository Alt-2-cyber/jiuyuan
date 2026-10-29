#include "max30102.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "errcode.h"
#include "soc_osal.h"

#define MAX30102_I2C_ADDR                  0x57

#define MAX30102_REG_INT_STATUS1          0x00
#define MAX30102_REG_FIFO_WR_PTR          0x04
#define MAX30102_REG_OVF_COUNTER          0x05
#define MAX30102_REG_FIFO_RD_PTR          0x06
#define MAX30102_REG_FIFO_DATA            0x07
#define MAX30102_REG_FIFO_CONFIG          0x08
#define MAX30102_REG_MODE_CONFIG          0x09
#define MAX30102_REG_SPO2_CONFIG          0x0A
#define MAX30102_REG_LED1_PA              0x0C
#define MAX30102_REG_LED2_PA              0x0D
#define MAX30102_REG_PART_ID              0xFF

#define MAX30102_EXPECTED_PART_ID         0x15
#define MAX30102_LED_CURRENT              0x3F
#define MAX30102_FIFO_SAMPLE_BYTES        6
#define MAX30102_SAMPLE_PERIOD_MS         40
#define MAX30102_FINGER_IR_MIN            12000U
#define MAX30102_DEBUG_LOG_PERIOD_MS      1000U
#define MAX30102_PEAK_MIN_MS              320U
#define MAX30102_PEAK_MAX_MS              1800U

static i2c_bus_t g_max30102_bus;
static uint8_t g_max30102_ready;
static uint32_t g_tick_ms;
static uint32_t g_last_peak_ms;
static uint32_t g_ir_dc;
static uint8_t g_above_peak;
static int g_latest_spo2;
static int g_latest_hr;
static uint8_t g_finger_present;
static uint32_t g_next_debug_log_ms;

static int max30102_write_reg(uint8_t reg, uint8_t value)
{
    uint8_t buffer[2] = { reg, value };
    i2c_data_t data = {
        .send_buf = buffer,
        .send_len = sizeof(buffer),
        .receive_buf = NULL,
        .receive_len = 0,
    };

    return (uapi_i2c_master_write(g_max30102_bus, MAX30102_I2C_ADDR, &data) == ERRCODE_SUCC) ? 0 : -1;
}

static int max30102_read_reg(uint8_t reg, uint8_t *value)
{
    i2c_data_t data = {
        .send_buf = &reg,
        .send_len = 1,
        .receive_buf = value,
        .receive_len = 1,
    };

    if (value == NULL) {
        return -1;
    }
    return (uapi_i2c_master_writeread(g_max30102_bus, MAX30102_I2C_ADDR, &data) == ERRCODE_SUCC) ? 0 : -1;
}

static int max30102_read_fifo(uint8_t *buffer, uint32_t len)
{
    uint8_t reg = MAX30102_REG_FIFO_DATA;
    i2c_data_t data = {
        .send_buf = &reg,
        .send_len = 1,
        .receive_buf = buffer,
        .receive_len = len,
    };

    if (buffer == NULL || len == 0) {
        return -1;
    }
    return (uapi_i2c_master_writeread(g_max30102_bus, MAX30102_I2C_ADDR, &data) == ERRCODE_SUCC) ? 0 : -1;
}

static uint32_t max30102_unpack_sample(const uint8_t *buffer)
{
    return (((uint32_t)buffer[0] << 16) | ((uint32_t)buffer[1] << 8) | buffer[2]) & 0x03FFFF;
}

static void max30102_reset_state(void)
{
    g_tick_ms = 0;
    g_last_peak_ms = 0;
    g_ir_dc = 0;
    g_above_peak = 0;
    g_latest_spo2 = 0;
    g_latest_hr = 0;
    g_finger_present = 0;
    g_next_debug_log_ms = 0;
}

static int max30102_clamp_int(int value, int min_value, int max_value)
{
    if (value < min_value) {
        return min_value;
    }
    if (value > max_value) {
        return max_value;
    }
    return value;
}

static void max30102_update_pulse(uint32_t red, uint32_t ir)
{
    int32_t ac;
    int32_t threshold;
    uint32_t delta;

    if (g_ir_dc == 0) {
        g_ir_dc = ir;
    } else {
        g_ir_dc = ((g_ir_dc * 31U) + ir) / 32U;
    }

    ac = (int32_t)ir - (int32_t)g_ir_dc;
    threshold = (int32_t)g_ir_dc / 120;
    if (threshold < 600) {
        threshold = 600;
    }

    if (!g_above_peak && ac > threshold) {
        delta = g_tick_ms - g_last_peak_ms;
        if (g_last_peak_ms != 0 && delta >= MAX30102_PEAK_MIN_MS && delta <= MAX30102_PEAK_MAX_MS) {
            int bpm = (int)(60000U / delta);
            if (bpm >= 40 && bpm <= 200) {
                g_latest_hr = (g_latest_hr == 0) ? bpm : ((g_latest_hr * 3) + bpm) / 4;
            }
        }
        g_last_peak_ms = g_tick_ms;
        g_above_peak = 1;
    } else if (g_above_peak && ac < (threshold / 2)) {
        g_above_peak = 0;
    }

    if (ir > 0) {
        int ratio = (int)((red * 100U) / ir);
        g_latest_spo2 = max30102_clamp_int(110 - (ratio / 2), 85, 99);
    }
}

static void max30102_process_sample(uint32_t red, uint32_t ir)
{
    g_tick_ms += MAX30102_SAMPLE_PERIOD_MS;

    if (ir < MAX30102_FINGER_IR_MIN) {
        g_finger_present = 0;
        g_latest_spo2 = 0;
        g_latest_hr = 0;
        g_last_peak_ms = 0;
        g_above_peak = 0;
        g_ir_dc = 0;
        return;
    }

    g_finger_present = 1;
    max30102_update_pulse(red, ir);
}

int max30102_init(i2c_bus_t bus)
{
    uint8_t part_id = 0;
    uint32_t retry;

    g_max30102_bus = bus;
    max30102_reset_state();

    for (retry = 0; retry < 3; retry++) {
        if (max30102_read_reg(MAX30102_REG_PART_ID, &part_id) == 0) {
            break;
        }
        osal_msleep(30);
    }
    if (retry >= 3) {
        osal_printk("[MAX30102] not found, check VCC/GND/SDA/SCL addr=0x%x\r\n", MAX30102_I2C_ADDR);
        g_max30102_ready = 0;
        return -1;
    }
    osal_printk("[MAX30102] part id=0x%x, expect=0x%x\r\n", part_id, MAX30102_EXPECTED_PART_ID);
    if (part_id != MAX30102_EXPECTED_PART_ID) {
        osal_printk("[MAX30102] unexpected part id:0x%x\r\n", part_id);
    }

    if (max30102_write_reg(MAX30102_REG_MODE_CONFIG, 0x40) != 0) {
        osal_printk("[MAX30102] reset failed\r\n");
        g_max30102_ready = 0;
        return -1;
    }
    osal_msleep(20);

    if (max30102_write_reg(MAX30102_REG_FIFO_WR_PTR, 0x00) != 0 ||
        max30102_write_reg(MAX30102_REG_OVF_COUNTER, 0x00) != 0 ||
        max30102_write_reg(MAX30102_REG_FIFO_RD_PTR, 0x00) != 0 ||
        max30102_write_reg(MAX30102_REG_FIFO_CONFIG, 0x4F) != 0 ||
        max30102_write_reg(MAX30102_REG_MODE_CONFIG, 0x03) != 0 ||
        max30102_write_reg(MAX30102_REG_SPO2_CONFIG, 0x27) != 0 ||
        max30102_write_reg(MAX30102_REG_LED1_PA, MAX30102_LED_CURRENT) != 0 ||
        max30102_write_reg(MAX30102_REG_LED2_PA, MAX30102_LED_CURRENT) != 0) {
        osal_printk("[MAX30102] config failed\r\n");
        g_max30102_ready = 0;
        return -1;
    }

    g_max30102_ready = 1;
    osal_printk("[MAX30102] init ok, addr=0x%x led=0x%x\r\n",
        MAX30102_I2C_ADDR, MAX30102_LED_CURRENT);
    return 0;
}

int max30102_update(void)
{
    uint8_t fifo[MAX30102_FIFO_SAMPLE_BYTES] = {0};
    uint32_t red;
    uint32_t ir;

    if (!g_max30102_ready) {
        return -1;
    }
    if (max30102_read_fifo(fifo, sizeof(fifo)) != 0) {
        g_max30102_ready = 0;
        g_finger_present = 0;
        osal_printk("[MAX30102] read lost\r\n");
        return -1;
    }

    red = max30102_unpack_sample(fifo);
    ir = max30102_unpack_sample(fifo + 3);
    max30102_process_sample(red, ir);
    if (g_tick_ms >= g_next_debug_log_ms) {
        osal_printk("[MAX30102] raw red=%lu ir=%lu finger=%u hr=%d spo2=%d\r\n",
            (unsigned long)red, (unsigned long)ir, g_finger_present, g_latest_hr, g_latest_spo2);
        g_next_debug_log_ms = g_tick_ms + MAX30102_DEBUG_LOG_PERIOD_MS;
    }

    return 1;
}

int max30102_get_latest(int *spo2, int *heart_rate, int *finger_present)
{
    if (spo2 != NULL) {
        *spo2 = g_latest_spo2;
    }
    if (heart_rate != NULL) {
        *heart_rate = g_latest_hr;
    }
    if (finger_present != NULL) {
        *finger_present = g_finger_present;
    }
    return g_max30102_ready ? 0 : -1;
}

int max30102_is_ready(void)
{
    return g_max30102_ready ? 1 : 0;
}

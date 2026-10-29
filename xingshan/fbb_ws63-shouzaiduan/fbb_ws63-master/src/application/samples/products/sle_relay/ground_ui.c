#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "adc.h"
#include "adc_porting.h"
#include "aht20.h"
#include "ground_model.h"
#include "gpio.h"
#include "i2c.h"
#include "pinctrl.h"
#include "securec.h"
#include "soc_osal.h"
#include "ssd1306.h"
#include "ssd1306_fonts.h"
#include "systick.h"

#include "ground_ui.h"

#define GROUND_UI_I2C_BUS 1
#define GROUND_UI_I2C_SDA_PIN 15
#define GROUND_UI_I2C_SCL_PIN 16
#define GROUND_UI_I2C_BAUDRATE 400000
#define GROUND_UI_I2C_MASTER_ADDR 0
#define GROUND_UI_I2C_PIN_MODE 2
#define GROUND_UI_OLED_ADDR 0x3C
#define GROUND_UI_GAS_ADC_CHANNEL 5
#define GROUND_UI_KEY_ADC_CHANNEL 1
#define GROUND_UI_KEY_K1_MAX_MV 750
#define GROUND_UI_KEY_K2_MAX_MV 1400
#define GROUND_UI_GAS_HIGH_THRESHOLD 1800
#define GROUND_UI_GAS_LOW_THRESHOLD 1500
#define GROUND_UI_TASK_STACK_SIZE 0x3000
#define GROUND_UI_TASK_PRIO 29
#define GROUND_ENV_TASK_PRIO 28
#define GROUND_UI_REFRESH_MS 100
#define GROUND_UI_FORCE_REFRESH_MS 500
#define GROUND_ENV_GAS_PERIOD_MS 500
#define GROUND_ENV_AHT_PERIOD_MS 2000
#define GROUND_ENV_AHT_MEASURE_MS 80
#define GROUND_ENV_RETRY_MS 1000
#define GROUND_AHT_CALIBRATE_RETRIES 5
#define GROUND_LINE_COUNT 6
#define GROUND_LINE_CAP 22
#define GROUND_UI_Y_OFFSET 3
#define GROUND_UI_LINE_STEP 9
#define GROUND_KEY_DIAGNOSTIC 0
#define GROUND_KEY_ADC_CHANNEL_COUNT 6

typedef struct {
    bool temperature_valid;
    bool humidity_valid;
    bool gas_valid;
    bool gas_danger;
    int16_t temperature_tenths;
    uint16_t humidity_tenths;
    uint16_t gas_mv;
} ground_environment_t;

typedef struct {
    ground_model_t model;
    ground_environment_t environment;
} ground_ui_snapshot_t;

static ground_model_t g_ground_model;
static ground_environment_t g_ground_environment;
static osal_mutex g_ground_model_mutex;
static osal_mutex g_ground_i2c_mutex;
static osal_mutex g_ground_adc_mutex;
static uint8_t g_ground_mutexes_ready;
static uint8_t g_ground_tasks_started;
static uint8_t g_ground_i2c_ready;
static uint8_t g_ground_oled_ready;
static uint8_t g_ground_adc_ready;
#if GROUND_KEY_DIAGNOSTIC
static uint8_t g_ground_gpio_ready;
#endif

static uint64_t ground_now_ms(void)
{
    return uapi_systick_get_ms();
}

static int ground_i2c_prepare(void)
{
    errcode_t ret;

    osal_mutex_lock(&g_ground_i2c_mutex);
    if (g_ground_i2c_ready) {
        osal_mutex_unlock(&g_ground_i2c_mutex);
        return 0;
    }
    (void)uapi_pin_set_mode(GROUND_UI_I2C_SDA_PIN, GROUND_UI_I2C_PIN_MODE);
    (void)uapi_pin_set_mode(GROUND_UI_I2C_SCL_PIN, GROUND_UI_I2C_PIN_MODE);
    (void)uapi_pin_set_pull(GROUND_UI_I2C_SDA_PIN, PIN_PULL_TYPE_UP);
    (void)uapi_pin_set_pull(GROUND_UI_I2C_SCL_PIN, PIN_PULL_TYPE_UP);
    ret = uapi_i2c_master_init(GROUND_UI_I2C_BUS, GROUND_UI_I2C_BAUDRATE,
        GROUND_UI_I2C_MASTER_ADDR);
    if (ret == ERRCODE_I2C_ALREADY_INIT) {
        ret = uapi_i2c_set_baudrate(GROUND_UI_I2C_BUS, GROUND_UI_I2C_BAUDRATE);
    }
    if (ret == ERRCODE_SUCC) {
        g_ground_i2c_ready = 1;
    }
    osal_mutex_unlock(&g_ground_i2c_mutex);
    return ret == ERRCODE_SUCC ? 0 : -1;
}

static bool ground_oled_probe(void)
{
    uint8_t command[] = {0x00, 0xAE};
    i2c_data_t data = {0};
    errcode_t ret;

    data.send_buf = command;
    data.send_len = sizeof(command);
    osal_mutex_lock(&g_ground_i2c_mutex);
    ret = uapi_i2c_master_write(GROUND_UI_I2C_BUS, GROUND_UI_OLED_ADDR, &data);
    osal_mutex_unlock(&g_ground_i2c_mutex);
    return ret == ERRCODE_SUCC;
}

static bool ground_oled_prepare(void)
{
    if (g_ground_oled_ready) {
        return true;
    }
    if (ground_i2c_prepare() != 0 || !ground_oled_probe()) {
        return false;
    }
    osal_mutex_lock(&g_ground_i2c_mutex);
    ssd1306_Init();
    osal_mutex_unlock(&g_ground_i2c_mutex);
    g_ground_oled_ready = 1;
    osal_printk("[GROUND_UI] oled=READY\r\n");
    return true;
}

static errcode_t ground_adc_prepare_locked(void)
{
    if (!g_ground_adc_ready) {
        errcode_t ret = uapi_adc_init(ADC_CLOCK_NONE);
        if (ret != ERRCODE_SUCC) {
            return ret;
        }
        g_ground_adc_ready = 1;
    }
    return ERRCODE_SUCC;
}

static errcode_t ground_adc_read(uint8_t channel, uint16_t *millivolts)
{
    errcode_t ret;

    osal_mutex_lock(&g_ground_adc_mutex);
    ret = ground_adc_prepare_locked();
    if (ret == ERRCODE_SUCC) {
        ret = adc_port_read(channel, millivolts);
    }
    osal_mutex_unlock(&g_ground_adc_mutex);
    return ret;
}

#if GROUND_KEY_DIAGNOSTIC
static void ground_adc_scan(uint16_t millivolts[GROUND_KEY_ADC_CHANNEL_COUNT],
    bool valid[GROUND_KEY_ADC_CHANNEL_COUNT])
{
    osal_mutex_lock(&g_ground_adc_mutex);
    errcode_t init_status = ground_adc_prepare_locked();
    for (uint8_t channel = 0; channel < GROUND_KEY_ADC_CHANNEL_COUNT; channel++) {
        millivolts[channel] = 0;
        valid[channel] = init_status == ERRCODE_SUCC &&
            adc_port_read(channel, &millivolts[channel]) == ERRCODE_SUCC;
    }
    osal_mutex_unlock(&g_ground_adc_mutex);
}

static void ground_gpio_prepare(void)
{
    if (g_ground_gpio_ready) {
        return;
    }
    uapi_gpio_init();
#if defined(CONFIG_GPIO_SELECT_CORE)
    uapi_gpio_select_core(GPIO_13, CORES_APPS_CORE);
    uapi_gpio_select_core(GPIO_14, CORES_APPS_CORE);
#endif
    (void)uapi_pin_set_mode(GPIO_13, PIN_MODE_0);
    (void)uapi_pin_set_mode(GPIO_14, PIN_MODE_0);
    (void)uapi_pin_set_pull(GPIO_13, PIN_PULL_TYPE_DISABLE);
    (void)uapi_pin_set_pull(GPIO_14, PIN_PULL_TYPE_DISABLE);
    (void)uapi_gpio_set_dir(GPIO_13, GPIO_DIRECTION_INPUT);
    (void)uapi_gpio_set_dir(GPIO_14, GPIO_DIRECTION_INPUT);
    g_ground_gpio_ready = 1;
}
#endif

#if !GROUND_KEY_DIAGNOSTIC
static const char *ground_distance_label(uint32_t route_dm)
{
    if (route_dm <= 30U) {
        return "VNEAR";
    }
    if (route_dm <= 120U) {
        return "NEAR";
    }
    if (route_dm <= 350U) {
        return "MID";
    }
    return "FAR";
}

static void ground_format_victim(const ground_ui_snapshot_t *snapshot, uint64_t now_ms,
    char lines[GROUND_LINE_COUNT][GROUND_LINE_CAP])
{
    uint8_t index = snapshot->model.selected_victim;
    const ground_victim_t *victim = &snapshot->model.victims[index];
    ground_link_t link = ground_model_link(victim, now_ms);

    (void)snprintf_s(lines[0], GROUND_LINE_CAP, GROUND_LINE_CAP - 1,
        "VD-%03u %u/3", (unsigned int)(index + 1U), (unsigned int)(index + 1U));
    if (victim->valid && victim->route_ok) {
        (void)snprintf_s(lines[1], GROUND_LINE_CAP, GROUND_LINE_CAP - 1,
            "PATH ~%lu.%lum %s", (unsigned long)(victim->route_dm / 10U),
            (unsigned long)(victim->route_dm % 10U), ground_distance_label(victim->route_dm));
    } else {
        (void)snprintf_s(lines[1], GROUND_LINE_CAP, GROUND_LINE_CAP - 1,
            "PATH ~--.-m UNKNOWN");
    }
    if (victim->valid) {
        (void)snprintf_s(lines[2], GROUND_LINE_CAP, GROUND_LINE_CAP - 1,
            "SpO2 %d HR %d", victim->spo2, victim->heart_rate);
        (void)snprintf_s(lines[3], GROUND_LINE_CAP, GROUND_LINE_CAP - 1,
            "BAT %d SOS %s", victim->battery, victim->sos ? "YES" : "NO");
        (void)snprintf_s(lines[4], GROUND_LINE_CAP, GROUND_LINE_CAP - 1,
            "LINK %s SRC %s", link == GROUND_LINK_OK ? "OK" : "LOST",
            victim->source_max30102 ? "MAX" : "SIM");
    } else {
        (void)snprintf_s(lines[2], GROUND_LINE_CAP, GROUND_LINE_CAP - 1, "SpO2 -- HR --");
        (void)snprintf_s(lines[3], GROUND_LINE_CAP, GROUND_LINE_CAP - 1, "BAT -- SOS --");
        (void)snprintf_s(lines[4], GROUND_LINE_CAP, GROUND_LINE_CAP - 1, "LINK -- SRC --");
    }
}

static void ground_format_tenths(char *buffer, size_t cap, int32_t value)
{
    int32_t absolute = value < 0 ? -value : value;
    (void)snprintf_s(buffer, cap, cap - 1, "%s%ld.%ld", value < 0 ? "-" : "",
        (long)(absolute / 10), (long)(absolute % 10));
}

static void ground_format_environment(const ground_ui_snapshot_t *snapshot,
    char lines[GROUND_LINE_COUNT][GROUND_LINE_CAP])
{
    char value[12];
    const ground_environment_t *environment = &snapshot->environment;

    (void)snprintf_s(lines[0], GROUND_LINE_CAP, GROUND_LINE_CAP - 1, "LOCAL ENV GROUND");
    if (environment->temperature_valid) {
        ground_format_tenths(value, sizeof(value), environment->temperature_tenths);
        (void)snprintf_s(lines[1], GROUND_LINE_CAP, GROUND_LINE_CAP - 1, "TEMP %sC OK", value);
    } else {
        (void)snprintf_s(lines[1], GROUND_LINE_CAP, GROUND_LINE_CAP - 1, "TEMP --.-C ERR");
    }
    if (environment->humidity_valid) {
        ground_format_tenths(value, sizeof(value), environment->humidity_tenths);
        (void)snprintf_s(lines[2], GROUND_LINE_CAP, GROUND_LINE_CAP - 1, "HUM %s%% OK", value);
    } else {
        (void)snprintf_s(lines[2], GROUND_LINE_CAP, GROUND_LINE_CAP - 1, "HUM --.-%% ERR");
    }
    if (environment->gas_valid) {
        (void)snprintf_s(lines[3], GROUND_LINE_CAP, GROUND_LINE_CAP - 1,
            "GAS %umV %s", environment->gas_mv, environment->gas_danger ? "DANGER" : "SAFE");
    } else {
        (void)snprintf_s(lines[3], GROUND_LINE_CAP, GROUND_LINE_CAP - 1, "GAS --mV ERR");
    }
    (void)snprintf_s(lines[4], GROUND_LINE_CAP, GROUND_LINE_CAP - 1, "SENSOR %s ENV",
        environment->temperature_valid && environment->humidity_valid && environment->gas_valid ? "OK" : "ERR");
}
#endif

#if GROUND_KEY_DIAGNOSTIC
static void ground_format_key_diagnostic(char lines[GROUND_LINE_COUNT][GROUND_LINE_CAP])
{
    uint16_t mv[GROUND_KEY_ADC_CHANNEL_COUNT];
    bool valid[GROUND_KEY_ADC_CHANNEL_COUNT];
    char a0[5];
    char a1[5];
    char a2[5];
    char a3[5];
    char a4[5];
    char a5[5];

    ground_adc_scan(mv, valid);
    (void)snprintf_s(a0, sizeof(a0), sizeof(a0) - 1, valid[0] ? "%04u" : "----", mv[0]);
    (void)snprintf_s(a1, sizeof(a1), sizeof(a1) - 1, valid[1] ? "%04u" : "----", mv[1]);
    (void)snprintf_s(a2, sizeof(a2), sizeof(a2) - 1, valid[2] ? "%04u" : "----", mv[2]);
    (void)snprintf_s(a3, sizeof(a3), sizeof(a3) - 1, valid[3] ? "%04u" : "----", mv[3]);
    (void)snprintf_s(a4, sizeof(a4), sizeof(a4) - 1, valid[4] ? "%04u" : "----", mv[4]);
    (void)snprintf_s(a5, sizeof(a5), sizeof(a5) - 1, valid[5] ? "%04u" : "----", mv[5]);
    (void)snprintf_s(lines[0], GROUND_LINE_CAP, GROUND_LINE_CAP - 1, "KEY INPUT SCAN");
    (void)snprintf_s(lines[1], GROUND_LINE_CAP, GROUND_LINE_CAP - 1, "A0 %s A1 %s", a0, a1);
    (void)snprintf_s(lines[2], GROUND_LINE_CAP, GROUND_LINE_CAP - 1, "A2 %s A3 %s", a2, a3);
    (void)snprintf_s(lines[3], GROUND_LINE_CAP, GROUND_LINE_CAP - 1, "A4 %s A5 %s", a4, a5);
    (void)snprintf_s(lines[4], GROUND_LINE_CAP, GROUND_LINE_CAP - 1, "D13 %u D14 %u",
        (unsigned int)uapi_gpio_get_val(GPIO_13), (unsigned int)uapi_gpio_get_val(GPIO_14));
    (void)snprintf_s(lines[5], GROUND_LINE_CAP, GROUND_LINE_CAP - 1, "PRESS S1/S2");
}
#endif

static void ground_draw_lines(char lines[GROUND_LINE_COUNT][GROUND_LINE_CAP])
{
    osal_mutex_lock(&g_ground_i2c_mutex);
    ssd1306_Fill(Black);
    for (uint8_t i = 0; i < GROUND_LINE_COUNT; i++) {
        ssd1306_SetCursor(0, (uint8_t)(GROUND_UI_Y_OFFSET + i * GROUND_UI_LINE_STEP));
        (void)ssd1306_DrawString(lines[i], Font_6x8, White);
    }
    ssd1306_UpdateScreen();
    osal_mutex_unlock(&g_ground_i2c_mutex);
}

#if !GROUND_KEY_DIAGNOSTIC
static void ground_take_snapshot(ground_ui_snapshot_t *snapshot)
{
    osal_mutex_lock(&g_ground_model_mutex);
    snapshot->model = g_ground_model;
    snapshot->environment = g_ground_environment;
    osal_mutex_unlock(&g_ground_model_mutex);
}
#endif

static void *ground_ui_task(const char *arg)
{
    char lines[GROUND_LINE_COUNT][GROUND_LINE_CAP] = {{0}};
    char previous[GROUND_LINE_COUNT][GROUND_LINE_CAP] = {{0}};
    uint64_t last_draw_ms = 0;
    bool previous_valid = false;

    unused(arg);
#if GROUND_KEY_DIAGNOSTIC
    ground_gpio_prepare();
#endif
    for (;;) {
        uint64_t now_ms = ground_now_ms();
#if GROUND_KEY_DIAGNOSTIC
        (void)memset_s(lines, sizeof(lines), 0, sizeof(lines));
        ground_format_key_diagnostic(lines);
#else
        uint16_t key_mv = UINT16_MAX;
        ground_key_t key = GROUND_KEY_NONE;
        bool k1_pressed;
        bool k2_pressed;
        ground_ui_snapshot_t snapshot;

        if (ground_adc_read(GROUND_UI_KEY_ADC_CHANNEL, &key_mv) == ERRCODE_SUCC) {
            key = ground_model_decode_key_mv(key_mv);
        }
        k1_pressed = key == GROUND_KEY_K1;
        k2_pressed = key == GROUND_KEY_K2;

        osal_mutex_lock(&g_ground_model_mutex);
        ground_model_sample_buttons(&g_ground_model, k1_pressed, k2_pressed, now_ms);
        osal_mutex_unlock(&g_ground_model_mutex);
        ground_take_snapshot(&snapshot);
        (void)memset_s(lines, sizeof(lines), 0, sizeof(lines));
        if (snapshot.model.page == GROUND_PAGE_ENV) {
            ground_format_environment(&snapshot, lines);
        } else {
            ground_format_victim(&snapshot, now_ms, lines);
        }
        if (key_mv == UINT16_MAX) {
            (void)snprintf_s(lines[5], GROUND_LINE_CAP, GROUND_LINE_CAP - 1, "KEY ADC ERR");
        } else {
            const char *key_name = key == GROUND_KEY_K1 ? "K1" :
                (key == GROUND_KEY_K2 ? "K2" : "--");
            uint16_t display_mv = (uint16_t)((key_mv / 10U) * 10U);
            (void)snprintf_s(lines[5], GROUND_LINE_CAP, GROUND_LINE_CAP - 1,
                "KEY %s %umV", key_name, display_mv);
        }
#endif
        if (ground_oled_prepare() &&
            (!previous_valid || memcmp(previous, lines, sizeof(lines)) != 0 ||
             now_ms < last_draw_ms || now_ms - last_draw_ms >= GROUND_UI_FORCE_REFRESH_MS)) {
            ground_draw_lines(lines);
            (void)memcpy_s(previous, sizeof(previous), lines, sizeof(lines));
            previous_valid = true;
            last_draw_ms = now_ms;
        }
        osal_msleep(GROUND_UI_REFRESH_MS);
    }
    return NULL;
}

static bool ground_aht_prepare(void)
{
    if (ground_i2c_prepare() != 0) {
        return false;
    }
    for (uint8_t attempt = 0; attempt < GROUND_AHT_CALIBRATE_RETRIES; attempt++) {
        uint32_t ret;
        osal_mutex_lock(&g_ground_i2c_mutex);
        ret = AHT20_Calibrate();
        osal_mutex_unlock(&g_ground_i2c_mutex);
        if (ret == 0U) {
            osal_printk("[GROUND_ENV] aht20=READY\r\n");
            return true;
        }
        osal_msleep(GROUND_ENV_RETRY_MS);
    }
    osal_printk("[GROUND_ENV] aht20=NOT_FOUND\r\n");
    return false;
}

static void ground_store_gas(errcode_t status, uint16_t mv)
{
    osal_mutex_lock(&g_ground_model_mutex);
    g_ground_environment.gas_valid = status == ERRCODE_SUCC;
    if (status == ERRCODE_SUCC) {
        g_ground_environment.gas_mv = mv;
        if (mv >= GROUND_UI_GAS_HIGH_THRESHOLD) {
            g_ground_environment.gas_danger = true;
        } else if (mv <= GROUND_UI_GAS_LOW_THRESHOLD) {
            g_ground_environment.gas_danger = false;
        }
    }
    osal_mutex_unlock(&g_ground_model_mutex);
}

static void ground_store_aht(bool valid, float temperature, float humidity)
{
    osal_mutex_lock(&g_ground_model_mutex);
    g_ground_environment.temperature_valid = valid;
    g_ground_environment.humidity_valid = valid;
    if (valid) {
        g_ground_environment.temperature_tenths = (int16_t)(temperature * 10.0f +
            (temperature >= 0.0f ? 0.5f : -0.5f));
        g_ground_environment.humidity_tenths = (uint16_t)(humidity * 10.0f + 0.5f);
    }
    osal_mutex_unlock(&g_ground_model_mutex);
}

static void *ground_environment_task(const char *arg)
{
    bool aht_ready = false;
    uint64_t last_aht_ms = 0;

    unused(arg);
    for (;;) {
        uint64_t now_ms = ground_now_ms();
        uint16_t gas_mv = 0;
        errcode_t gas_status;

        gas_status = ground_adc_read(GROUND_UI_GAS_ADC_CHANNEL, &gas_mv);
        ground_store_gas(gas_status, gas_mv);

        if (!aht_ready) {
            aht_ready = ground_aht_prepare();
        }
        if (aht_ready && (last_aht_ms == 0U || now_ms < last_aht_ms ||
            now_ms - last_aht_ms >= GROUND_ENV_AHT_PERIOD_MS)) {
            float temperature = 0.0f;
            float humidity = 0.0f;
            uint32_t start_status;
            uint32_t result_status = 1U;

            osal_mutex_lock(&g_ground_i2c_mutex);
            start_status = AHT20_StartMeasure();
            osal_mutex_unlock(&g_ground_i2c_mutex);
            if (start_status == 0U) {
                osal_msleep(GROUND_ENV_AHT_MEASURE_MS);
                osal_mutex_lock(&g_ground_i2c_mutex);
                result_status = AHT20_GetMeasureResult(&temperature, &humidity);
                osal_mutex_unlock(&g_ground_i2c_mutex);
            }
            if (result_status == 0U && temperature >= -40.0f && temperature <= 85.0f &&
                humidity >= 0.0f && humidity <= 100.0f) {
                ground_store_aht(true, temperature, humidity);
            } else {
                ground_store_aht(false, 0.0f, 0.0f);
                aht_ready = false;
            }
            last_aht_ms = now_ms;
        }
        osal_msleep(GROUND_ENV_GAS_PERIOD_MS);
    }
    return NULL;
}

int ground_ui_init(void)
{
    if (g_ground_mutexes_ready) {
        return 0;
    }
    if (osal_mutex_init(&g_ground_model_mutex) != OSAL_SUCCESS) {
        return -1;
    }
    if (osal_mutex_init(&g_ground_i2c_mutex) != OSAL_SUCCESS) {
        return -1;
    }
    if (osal_mutex_init(&g_ground_adc_mutex) != OSAL_SUCCESS) {
        return -1;
    }
    ground_model_init(&g_ground_model);
    (void)memset_s(&g_ground_environment, sizeof(g_ground_environment), 0,
        sizeof(g_ground_environment));
    g_ground_mutexes_ready = 1;
    return 0;
}

int ground_ui_start(void)
{
    osal_task *ui_task;
    osal_task *environment_task;

    if (!g_ground_mutexes_ready || g_ground_tasks_started) {
        return g_ground_tasks_started ? 0 : -1;
    }
    ui_task = osal_kthread_create((osal_kthread_handler)ground_ui_task, 0,
        "GroundUiTask", GROUND_UI_TASK_STACK_SIZE);
    environment_task = osal_kthread_create((osal_kthread_handler)ground_environment_task, 0,
        "GroundEnvTask", GROUND_UI_TASK_STACK_SIZE);
    if (ui_task == NULL || environment_task == NULL) {
        return -1;
    }
    osal_kthread_set_priority(ui_task, GROUND_UI_TASK_PRIO);
    osal_kthread_set_priority(environment_task, GROUND_ENV_TASK_PRIO);
    g_ground_tasks_started = 1;
    osal_printk("[GROUND_UI] page=VICTIM controls=K1/K2\r\n");
    return 0;
}

void ground_ui_ingest_report(const uint8_t *data, uint16_t len)
{
    ground_report_t report;

    if (!g_ground_mutexes_ready ||
        !ground_model_parse_report(data, len, ground_now_ms(), &report)) {
        return;
    }
    osal_mutex_lock(&g_ground_model_mutex);
    (void)ground_model_apply_report(&g_ground_model, &report);
    osal_mutex_unlock(&g_ground_model_mutex);
}

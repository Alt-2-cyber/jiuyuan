/**
 * Copyright (c) HiSilicon (Shanghai) Technologies Co., Ltd. 2023-2023. All rights reserved.
 *
 * Description: SLE UART Sample Source. \n
 *
 * History: \n
 * 2023-07-17, Create file. \n
 */
#include "common_def.h"
#include "soc_osal.h"
#include "app_init.h"
#include "pinctrl.h"
#include "i2c.h"
#include "uart.h"
#include "rescue_cloud.h"
#include "max30102.h"
#include "securec.h"
// #include "pm_clock.h"
#include "sle_low_latency.h"
#if defined(CONFIG_SAMPLE_SUPPORT_SLE_UART_SERVER)
#include "sle_uart_server.h"
#include "sle_uart_server_adv.h"
#include "sle_device_discovery.h"
#include "sle_errcode.h"
#elif defined(CONFIG_SAMPLE_SUPPORT_SLE_UART_CLIENT)
#define SLE_UART_TASK_STACK_SIZE            0x600
#include "sle_connection_manager.h"
#include "sle_ssap_client.h"
#include "sle_uart_client.h"
#endif  /* CONFIG_SAMPLE_SUPPORT_SLE_UART_CLIENT */

#define SLE_UART_TASK_PRIO                  28
#define SLE_UART_TASK_DURATION_MS           2000
#define SLE_UART_BAUDRATE                   115200
#define SLE_UART_TRANSFER_SIZE              512
#define RESCUE_UART_BRIDGE_ENABLE           0

#if RESCUE_UART_BRIDGE_ENABLE
static uint8_t g_app_uart_rx_buff[SLE_UART_TRANSFER_SIZE] = { 0 };

static uart_buffer_config_t g_app_uart_buffer_config = {
    .rx_buffer = g_app_uart_rx_buff,
    .rx_buffer_size = SLE_UART_TRANSFER_SIZE
};
#endif

static void uart_init_pin(void)
{
#if RESCUE_UART_BRIDGE_ENABLE
    if (CONFIG_SLE_UART_BUS == 0) {
        uapi_pin_set_mode(CONFIG_UART_TXD_PIN, PIN_MODE_1);
        uapi_pin_set_mode(CONFIG_UART_RXD_PIN, PIN_MODE_1);       
    }else if (CONFIG_SLE_UART_BUS == 1) {
        uapi_pin_set_mode(CONFIG_UART_TXD_PIN, PIN_MODE_1);
        uapi_pin_set_mode(CONFIG_UART_RXD_PIN, PIN_MODE_1);       
    }
#endif
}

static void uart_init_config(void)
{
#if RESCUE_UART_BRIDGE_ENABLE
    uart_attr_t attr = {
        .baud_rate = SLE_UART_BAUDRATE,
        .data_bits = UART_DATA_BIT_8,
        .stop_bits = UART_STOP_BIT_1,
        .parity = UART_PARITY_NONE
    };

    uart_pin_config_t pin_config = {
        .tx_pin = CONFIG_UART_TXD_PIN,
        .rx_pin = CONFIG_UART_RXD_PIN,
        .cts_pin = PIN_NONE,
        .rts_pin = PIN_NONE
    };
    uapi_uart_deinit(CONFIG_SLE_UART_BUS);
    uapi_uart_init(CONFIG_SLE_UART_BUS, &pin_config, &attr, NULL, &g_app_uart_buffer_config);
#endif
}

#if defined(CONFIG_SAMPLE_SUPPORT_SLE_UART_SERVER)
#define SLE_UART_SERVER_DELAY_COUNT         5

#define SLE_UART_TASK_STACK_SIZE            0x1200
#define SLE_ADV_HANDLE_DEFAULT              1
#define SLE_UART_SERVER_MSG_QUEUE_LEN       5
#define SLE_UART_SERVER_MSG_QUEUE_MAX_SIZE  32
#define SLE_UART_SERVER_QUEUE_DELAY         0xFFFFFFFF
#define SLE_UART_SERVER_BUFF_MAX_SIZE       800
#ifdef CONFIG_SAMPLE_SUPPORT_PERFORMANCE_TYPE
#define SLE_UART_SERVER_SEND_BUFF_MAX_LEN   250
#else
#define SLE_UART_SERVER_SEND_BUFF_MAX_LEN   40
#endif
unsigned long g_sle_uart_server_msgqueue_id;
#define SLE_UART_SERVER_LOG                 "[sle uart server]"
static void ssaps_server_read_request_cbk(uint8_t server_id, uint16_t conn_id, ssaps_req_read_cb_t *read_cb_para,
    errcode_t status)
{
    osal_printk("%s ssaps read request cbk callback server_id:%x, conn_id:%x, handle:%x, status:%x\r\n",
        SLE_UART_SERVER_LOG, server_id, conn_id, read_cb_para->handle, status);
}
static void ssaps_server_write_request_cbk(uint8_t server_id, uint16_t conn_id, ssaps_req_write_cb_t *write_cb_para,
    errcode_t status)
{
    osal_printk("%s ssaps write request callback cbk server_id:%x, conn_id:%x, handle:%x, status:%x\r\n",
        SLE_UART_SERVER_LOG, server_id, conn_id, write_cb_para->handle, status);
    if ((write_cb_para->length > 0) && write_cb_para->value) {
        osal_printk("\n sle uart recived data : %s\r\n", write_cb_para->value);
        if (RESCUE_UART_BRIDGE_ENABLE) {
            uapi_uart_write(CONFIG_SLE_UART_BUS, (uint8_t *)write_cb_para->value, write_cb_para->length, 0);
        }
    }
}

#ifdef CONFIG_SAMPLE_SUPPORT_LOW_LATENCY_TYPE
uint8_t g_buff[SLE_UART_SERVER_BUFF_MAX_SIZE] = {0};
uint16_t g_uart_buff_len = 0;
uint8_t g_buff_data_valid = 0;
uint8_t g_mcs_flag = 0;
#endif
static void sle_uart_server_read_int_handler(const void *buffer, uint16_t length, bool error)
{
    unused(error);
    if (sle_uart_client_is_connected()) {
#ifdef CONFIG_SAMPLE_SUPPORT_LOW_LATENCY_TYPE
    g_buff_data_valid = 1;
    g_uart_buff_len = 0;
    (void)memcpy_s(g_buff, SLE_UART_SERVER_SEND_BUFF_MAX_LEN, buffer, length);
    g_uart_buff_len = length;
#else
    sle_uart_server_send_report_by_handle(buffer, length);
#endif
    } else {
        osal_printk("%s sle client is not connected! \r\n", SLE_UART_SERVER_LOG);
    }
}

#ifdef CONFIG_SAMPLE_SUPPORT_LOW_LATENCY_TYPE
uint8_t *sle_uart_low_latency_tx_cbk(uint16_t *len)
{
#ifdef CONFIG_SAMPLE_SUPPORT_PERFORMANCE_TYPE
    if (g_mcs_flag == 0) {
        sle_uart_server_sample_set_mcs(get_connect_id());
    }
    g_uart_buff_len = SLE_UART_SERVER_SEND_BUFF_MAX_LEN;
    g_buff_data_valid = 1;
    g_mcs_flag = 1;
#endif
    if (g_buff_data_valid == 0) {
        return NULL;
    }
    if (g_uart_buff_len == 0) {
        return NULL;
    }
    *len = g_uart_buff_len;
    g_buff_data_valid = 0;
    return g_buff;
}

void sle_uart_low_latency_tx_cbk_register(void)
{
    sle_low_latency_tx_callbacks_t cbk_func = {0};
    cbk_func.low_latency_tx_cb = sle_uart_low_latency_tx_cbk;
    sle_low_latency_tx_register_callbacks(&cbk_func);
}
#endif

static void sle_uart_server_create_msgqueue(void)
{
    if (osal_msg_queue_create("sle_uart_server_msgqueue", SLE_UART_SERVER_MSG_QUEUE_LEN, \
        (unsigned long *)&g_sle_uart_server_msgqueue_id, 0, SLE_UART_SERVER_MSG_QUEUE_MAX_SIZE) != OSAL_SUCCESS) {
        osal_printk("^%s sle_uart_server_create_msgqueue message queue create failed!\n", SLE_UART_SERVER_LOG);
    }
}

static void sle_uart_server_delete_msgqueue(void)
{
    osal_msg_queue_delete(g_sle_uart_server_msgqueue_id);
}

static void sle_uart_server_write_msgqueue(uint8_t *buffer_addr, uint16_t buffer_size)
{
    osal_msg_queue_write_copy(g_sle_uart_server_msgqueue_id, (void *)buffer_addr, \
                              (uint32_t)buffer_size, 0);
}

static int32_t sle_uart_server_receive_msgqueue(uint8_t *buffer_addr, uint32_t *buffer_size)
{
    return osal_msg_queue_read_copy(g_sle_uart_server_msgqueue_id, (void *)buffer_addr, \
                                    buffer_size, SLE_UART_SERVER_QUEUE_DELAY);
}
static void sle_uart_server_rx_buf_init(uint8_t *buffer_addr, uint32_t *buffer_size)
{
    *buffer_size = SLE_UART_SERVER_MSG_QUEUE_MAX_SIZE;
    (void)memset_s(buffer_addr, *buffer_size, 0, *buffer_size);
}

static void *sle_uart_server_task(const char *arg)
{
    unused(arg);
    uint8_t rx_buf[SLE_UART_SERVER_MSG_QUEUE_MAX_SIZE] = {0};
    uint32_t rx_length = SLE_UART_SERVER_MSG_QUEUE_MAX_SIZE;
    uint8_t sle_connect_state[] = "sle_dis_connect";

    sle_uart_server_create_msgqueue();
    sle_uart_server_register_msg(sle_uart_server_write_msgqueue);
    sle_uart_server_init(ssaps_server_read_request_cbk, ssaps_server_write_request_cbk);


#ifdef CONFIG_SAMPLE_SUPPORT_LOW_LATENCY_TYPE
    sle_uart_low_latency_tx_cbk_register();
#endif
    /* UART pinmux. */
    uart_init_pin();

    /* UART init config. */
    uart_init_config();

    errcode_t ret = ERRCODE_SUCC;
    if (RESCUE_UART_BRIDGE_ENABLE) {
        uapi_uart_unregister_rx_callback(CONFIG_SLE_UART_BUS);
        ret = uapi_uart_register_rx_callback(CONFIG_SLE_UART_BUS,
                                             UART_RX_CONDITION_FULL_OR_SUFFICIENT_DATA_OR_IDLE,
                                             1, sle_uart_server_read_int_handler);
        if (ret != ERRCODE_SUCC) {
            osal_printk("%s Register uart callback fail.[%x]\r\n", SLE_UART_SERVER_LOG, ret);
            return NULL;
        }
    }
    while (1) {
        sle_uart_server_rx_buf_init(rx_buf, &rx_length);
        sle_uart_server_receive_msgqueue(rx_buf, &rx_length);
        if (strncmp((const char *)rx_buf, (const char *)sle_connect_state, sizeof(sle_connect_state)) == 0) {
            ret = sle_start_announce(SLE_ADV_HANDLE_DEFAULT);
            if (ret != ERRCODE_SLE_SUCCESS) {
                osal_printk("%s sle_connect_state_changed_cbk,sle_start_announce fail :%02x\r\n",
                    SLE_UART_SERVER_LOG, ret);
            }
        }
        osal_msleep(SLE_UART_TASK_DURATION_MS);
    }
    sle_uart_server_delete_msgqueue();
    return NULL;
}
#elif defined(CONFIG_SAMPLE_SUPPORT_SLE_UART_CLIENT)

#define RESCUE_RESCUER_REPORT_INTERVAL_MS      1000
#define RESCUE_RESCUER_LOCAL_SENSOR_ENABLE     0
#define RESCUE_RESCUER_LOCAL_REPORT_ENABLE     0
#define RESCUE_RESCUER_I2C_SDA_PIN             15
#define RESCUE_RESCUER_I2C_SCL_PIN             16
#define RESCUE_RESCUER_I2C_PIN_MODE            2
#define RESCUE_RESCUER_I2C_BUS                 1
#define RESCUE_RESCUER_I2C_MASTER_ADDR         0
#define RESCUE_RESCUER_I2C_BAUDRATE            100000
#define RESCUE_RESCUER_MAX30102_ADDR           0x57
#define RESCUE_RESCUER_MAX30102_PART_ID_REG    0xFF
#define RESCUE_RESCUER_SENSOR_TASK_STACK_SIZE  0x1000
#define RESCUE_RESCUER_SENSOR_TASK_PRIO        29
#define RESCUE_RESCUER_SENSOR_POLL_MS          40

static uint8_t g_rescue_rescuer_sensor_task_started = 0;
static uint8_t g_rescue_rescuer_report_task_started = 0;
static uint8_t g_rescue_rescuer_sensor_ready = 0;
static uint8_t g_rescue_rescuer_finger_present = 0;
static uint8_t g_rescue_rescuer_i2c_ready = 0;
static uint8_t g_rescue_rescuer_i2c_mutex_ready = 0;
static osal_mutex g_rescue_rescuer_i2c_mutex;
static uint32_t g_rescue_rescuer_seq = 0;
static int g_rescue_rescuer_spo2 = 0;
static int g_rescue_rescuer_hr = 0;

static int rescue_rescuer_i2c_prepare(void)
{
    errcode_t ret;

    if (!g_rescue_rescuer_i2c_mutex_ready) {
        if (osal_mutex_init(&g_rescue_rescuer_i2c_mutex) != OSAL_SUCCESS) {
            osal_printk("[RESCUE_RESCUER_I2C] mutex init failed\r\n");
            return 0;
        }
        g_rescue_rescuer_i2c_mutex_ready = 1;
    }

    if (g_rescue_rescuer_i2c_ready) {
        return 1;
    }

    uapi_pin_set_mode(RESCUE_RESCUER_I2C_SDA_PIN, RESCUE_RESCUER_I2C_PIN_MODE);
    uapi_pin_set_mode(RESCUE_RESCUER_I2C_SCL_PIN, RESCUE_RESCUER_I2C_PIN_MODE);
    uapi_pin_set_pull(RESCUE_RESCUER_I2C_SDA_PIN, PIN_PULL_TYPE_UP);
    uapi_pin_set_pull(RESCUE_RESCUER_I2C_SCL_PIN, PIN_PULL_TYPE_UP);
    ret = uapi_i2c_master_init(RESCUE_RESCUER_I2C_BUS, RESCUE_RESCUER_I2C_BAUDRATE,
                               RESCUE_RESCUER_I2C_MASTER_ADDR);
    if (ret != ERRCODE_SUCC && ret != ERRCODE_I2C_ALREADY_INIT) {
        osal_printk("[RESCUE_RESCUER_I2C] i2c init failed:0x%x\r\n", ret);
        return 0;
    }

    g_rescue_rescuer_i2c_ready = 1;
    return 1;
}

static void rescue_rescuer_i2c_lock(void)
{
    if (g_rescue_rescuer_i2c_mutex_ready) {
        osal_mutex_lock(&g_rescue_rescuer_i2c_mutex);
    }
}

static void rescue_rescuer_i2c_unlock(void)
{
    if (g_rescue_rescuer_i2c_mutex_ready) {
        osal_mutex_unlock(&g_rescue_rescuer_i2c_mutex);
    }
}

static void rescue_rescuer_i2c_scan(void)
{
    uint8_t reg = 0x00;
    uint8_t value = 0;
    uint8_t part_id_reg = RESCUE_RESCUER_MAX30102_PART_ID_REG;
    uint8_t part_id = 0;
    uint8_t found = 0;
    i2c_data_t data = {
        .send_buf = &reg,
        .send_len = 1,
        .receive_buf = &value,
        .receive_len = 1,
    };
    i2c_data_t part_id_data = {
        .send_buf = &part_id_reg,
        .send_len = 1,
        .receive_buf = &part_id,
        .receive_len = 1,
    };

    osal_printk("[RESCUE_RESCUER_I2C] scan bus=%d sda=%d(I2C1_SDA) scl=%d(I2C1_SCL) start\r\n",
        RESCUE_RESCUER_I2C_BUS, RESCUE_RESCUER_I2C_SDA_PIN, RESCUE_RESCUER_I2C_SCL_PIN);
    for (uint16_t addr = 0x03; addr <= 0x77; addr++) {
        if (uapi_i2c_master_writeread(RESCUE_RESCUER_I2C_BUS, addr, &data) == ERRCODE_SUCC) {
            osal_printk("[RESCUE_RESCUER_I2C] found addr=0x%x\r\n", addr);
            found = 1;
        }
    }
    if (uapi_i2c_master_writeread(RESCUE_RESCUER_I2C_BUS, RESCUE_RESCUER_MAX30102_ADDR,
                                  &part_id_data) == ERRCODE_SUCC) {
        osal_printk("[RESCUE_RESCUER_I2C] max30102 probe ok addr=0x%x part_id=0x%x\r\n",
            RESCUE_RESCUER_MAX30102_ADDR, part_id);
        found = 1;
    } else {
        osal_printk("[RESCUE_RESCUER_I2C] max30102 probe failed addr=0x%x\r\n", RESCUE_RESCUER_MAX30102_ADDR);
    }
    if (!found) {
        osal_printk("[RESCUE_RESCUER_I2C] no device found, check VCC/GND/SCL/SDA\r\n");
    }
    osal_printk("[RESCUE_RESCUER_I2C] scan done\r\n");
}

static void *rescue_rescuer_sensor_task(const char *arg)
{
    int spo2 = 0;
    int heart_rate = 0;
    int finger_present = 0;
    uint32_t no_finger_log_count = 0;

    unused(arg);
    osal_printk("[RESCUE_RESCUER] max30102 task start\r\n");
    while (!rescue_rescuer_i2c_prepare()) {
        osal_msleep(1000);
    }

    rescue_rescuer_i2c_lock();
    rescue_rescuer_i2c_scan();
    g_rescue_rescuer_sensor_ready = (max30102_init(RESCUE_RESCUER_I2C_BUS) == 0) ? 1 : 0;
    rescue_rescuer_i2c_unlock();
    osal_printk("[RESCUE_RESCUER] max30102 first init %s\r\n",
        g_rescue_rescuer_sensor_ready ? "ok" : "failed");

    while (1) {
        if (!g_rescue_rescuer_sensor_ready) {
            rescue_rescuer_i2c_lock();
            g_rescue_rescuer_sensor_ready = (max30102_init(RESCUE_RESCUER_I2C_BUS) == 0) ? 1 : 0;
            rescue_rescuer_i2c_unlock();
            osal_printk("[RESCUE_RESCUER] max30102 retry init %s\r\n",
                g_rescue_rescuer_sensor_ready ? "ok" : "failed");
            osal_msleep(1000);
            continue;
        }

        rescue_rescuer_i2c_lock();
        if (max30102_update() < 0) {
            osal_printk("[RESCUE_RESCUER] max30102 read failed\r\n");
            g_rescue_rescuer_sensor_ready = 0;
        }
        (void)max30102_get_latest(&spo2, &heart_rate, &finger_present);
        rescue_rescuer_i2c_unlock();

        g_rescue_rescuer_finger_present = (uint8_t)(finger_present ? 1 : 0);
        g_rescue_rescuer_spo2 = spo2;
        g_rescue_rescuer_hr = heart_rate;
        if (!g_rescue_rescuer_finger_present && ((no_finger_log_count++ % 50) == 0)) {
            osal_printk("[RESCUE_RESCUER] max30102 ready, no finger or weak signal\r\n");
        }
        osal_msleep(RESCUE_RESCUER_SENSOR_POLL_MS);
    }
    return NULL;
}

static void rescue_rescuer_start_sensor_task(void)
{
    if (g_rescue_rescuer_sensor_task_started) {
        return;
    }
    osal_task *sensor_task_handle = osal_kthread_create((osal_kthread_handler)rescue_rescuer_sensor_task, 0,
                                                        "RescuerMax30102Task",
                                                        RESCUE_RESCUER_SENSOR_TASK_STACK_SIZE);
    if (sensor_task_handle != NULL) {
        osal_kthread_set_priority(sensor_task_handle, RESCUE_RESCUER_SENSOR_TASK_PRIO);
        g_rescue_rescuer_sensor_task_started = 1;
        osal_printk("[RESCUE_RESCUER] max30102 task created\r\n");
    } else {
        osal_printk("[RESCUE_RESCUER] max30102 task create failed\r\n");
    }
}

static void rescue_rescuer_publish_report(uint32_t seq)
{
    int spo2 = g_rescue_rescuer_spo2;
    int heart_rate = g_rescue_rescuer_hr;
    int systolic = 118;
    int battery = 86 - (seq % 8);
    int sos = 0;

    g_rescue_rescuer_seq = seq;
    osal_printk("[RESCUE_RESCUER] seq=%u finger=%u spo2=%d hr=%d\r\n",
        seq, g_rescue_rescuer_finger_present, spo2, heart_rate);
    rescue_cloud_publish_victim_state(seq, spo2, heart_rate, systolic, battery, sos);
}

static void *rescue_rescuer_report_task(const char *arg)
{
    uint32_t seq = 0;

    unused(arg);
    while (1) {
        rescue_rescuer_publish_report(seq++);
        osal_msleep(RESCUE_RESCUER_REPORT_INTERVAL_MS);
    }
    return NULL;
}

static void rescue_rescuer_start_report_task(void)
{
    if (g_rescue_rescuer_report_task_started) {
        return;
    }
    osal_task *report_task_handle = osal_kthread_create((osal_kthread_handler)rescue_rescuer_report_task, 0,
                                                        "RescuerReportTask", 0x1000);
    if (report_task_handle != NULL) {
        osal_kthread_set_priority(report_task_handle, SLE_UART_TASK_PRIO + 1);
        g_rescue_rescuer_report_task_started = 1;
    } else {
        osal_printk("[RESCUE_RESCUER] report task create failed\r\n");
    }
}

static void rescue_rescuer_handle_sle_payload(ssapc_handle_value_t *data)
{
    char payload[160] = {0};
    uint16_t copy_len;

    if (data == NULL || data->data == NULL || data->data_len == 0) {
        return;
    }

    copy_len = (data->data_len < (sizeof(payload) - 1)) ? data->data_len : (sizeof(payload) - 1);
    if (memcpy_s(payload, sizeof(payload), data->data, copy_len) != EOK) {
        return;
    }
    while (copy_len > 0 && (payload[copy_len - 1] == '\r' || payload[copy_len - 1] == '\n' ||
        payload[copy_len - 1] == '\0')) {
        payload[--copy_len] = '\0';
    }

    osal_printk("\n sle uart recived data : %s\r\n", payload);
    rescue_print_target_state(payload);
    rescue_cloud_publish_sle_payload((const uint8_t *)payload, copy_len);
    if (RESCUE_UART_BRIDGE_ENABLE) {
        uapi_uart_write(CONFIG_SLE_UART_BUS, (uint8_t *)payload, copy_len, 0);
    }
}

void sle_uart_notification_cb(uint8_t client_id, uint16_t conn_id, ssapc_handle_value_t *data,
    errcode_t status)
{
    unused(client_id);
    unused(conn_id);
    unused(status);
    rescue_rescuer_handle_sle_payload(data);
}

void sle_uart_indication_cb(uint8_t client_id, uint16_t conn_id, ssapc_handle_value_t *data,
    errcode_t status)
{
    unused(client_id);
    unused(conn_id);
    unused(status);
    rescue_rescuer_handle_sle_payload(data);
}

static void sle_uart_client_read_int_handler(const void *buffer, uint16_t length, bool error)
{
    unused(error);
    ssapc_write_param_t *sle_uart_send_param = get_g_sle_uart_send_param();
    uint16_t g_sle_uart_conn_id = get_g_sle_uart_conn_id();
    sle_uart_send_param->data_len = length;
    sle_uart_send_param->data = (uint8_t *)buffer;
    ssapc_write_req(0, g_sle_uart_conn_id, sle_uart_send_param);
}

static void *sle_uart_client_task(const char *arg)
{
    unused(arg);
    osal_printk("[RESCUE_RESCUER] boot start: sle gateway + cloud build\r\n");
    if (RESCUE_RESCUER_LOCAL_SENSOR_ENABLE) {
        rescue_rescuer_start_sensor_task();
    } else {
        osal_printk("[RESCUE_RESCUER] local max30102 disabled, using victim SLE data\r\n");
    }
    if (RESCUE_RESCUER_LOCAL_REPORT_ENABLE) {
        rescue_rescuer_start_report_task();
    }
    rescue_cloud_start("rescuer");
    /* UART pinmux. */
    uart_init_pin();

    /* UART init config. */
    uart_init_config();

    errcode_t ret = ERRCODE_SUCC;
    if (RESCUE_UART_BRIDGE_ENABLE) {
        uapi_uart_unregister_rx_callback(CONFIG_SLE_UART_BUS);
        ret = uapi_uart_register_rx_callback(CONFIG_SLE_UART_BUS,
                                             UART_RX_CONDITION_FULL_OR_SUFFICIENT_DATA_OR_IDLE,
                                             1, sle_uart_client_read_int_handler);
    } else {
        osal_printk("[RESCUE_RESCUER] uart bridge disabled\r\n");
    }
    sle_uart_client_init(sle_uart_notification_cb, sle_uart_indication_cb);
    
    if (ret != ERRCODE_SUCC) {
        osal_printk("Register uart callback fail.");
        return NULL;
    }

    return NULL;
}
#endif  /* CONFIG_SAMPLE_SUPPORT_SLE_UART_CLIENT */

static void sle_uart_entry(void)
{
    osal_task *task_handle = NULL;
    osal_kthread_lock();
#if defined(CONFIG_SAMPLE_SUPPORT_SLE_UART_SERVER)
    task_handle = osal_kthread_create((osal_kthread_handler)sle_uart_server_task, 0, "SLEUartServerTask",
                                      SLE_UART_TASK_STACK_SIZE);
#elif defined(CONFIG_SAMPLE_SUPPORT_SLE_UART_CLIENT)
    task_handle = osal_kthread_create((osal_kthread_handler)sle_uart_client_task, 0, "SLEUartDongleTask",
                                      SLE_UART_TASK_STACK_SIZE);
#endif /* CONFIG_SAMPLE_SUPPORT_SLE_UART_CLIENT */
    if (task_handle != NULL) {
        osal_kthread_set_priority(task_handle, SLE_UART_TASK_PRIO);
    }
    osal_kthread_unlock();
}

/* Run the sle_uart_entry. */
app_run(sle_uart_entry);

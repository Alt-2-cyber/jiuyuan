/**
 * Copyright (c) HiSilicon (Shanghai) Technologies Co., Ltd. 2023-2023. All rights reserved.
 *
 * Description: SLE uart sample of client. \n
 *
 * History: \n
 * 2023-04-03, Create file. \n
 */
#include "common_def.h"
#include "soc_osal.h"
#include "securec.h"
#include "product.h"
#include "bts_le_gap.h"
#include "pinctrl.h"
#include "i2c.h"
#include "ssd1306.h"
#include "ssd1306_fonts.h"
#include <stdlib.h>

#include "sle_device_discovery.h"
#include "sle_connection_manager.h"
#include "sle_uart_client.h"
#ifdef CONFIG_SAMPLE_SUPPORT_LOW_LATENCY_TYPE
#include "sle_low_latency.h"
#endif
#define SLE_MTU_SIZE_DEFAULT            520
#define SLE_SEEK_INTERVAL_DEFAULT       100
#define SLE_SEEK_WINDOW_DEFAULT         100
#define UUID_16BIT_LEN                  2
#define UUID_128BIT_LEN                 16
#define SLE_UART_TASK_DELAY_MS          1000
#define SLE_UART_WAIT_SLE_CORE_READY_MS 5000
#define SLE_UART_RECV_CNT               1000
#define SLE_UART_LOW_LATENCY_2K         2000
#define RESCUE_RSSI_POLL_INTERVAL_MS    1000
#define RESCUE_RSSI_INVALID             127
#define RESCUE_OLED_REFRESH_MS          1000
#define RESCUE_OLED_TASK_STACK_SIZE     0x1200
#define RESCUE_OLED_TASK_PRIO           30
#define RESCUE_OLED_SCL_PIN             15
#define RESCUE_OLED_SDA_PIN             16
#define RESCUE_OLED_PIN_MODE            2
#define RESCUE_OLED_I2C_BUS             1
#define RESCUE_OLED_I2C_MASTER_ADDR     0
#define RESCUE_OLED_I2C_BAUDRATE        400000
#define RESCUE_PAYLOAD_CACHE_LEN        160
#define RESCUE_ID_LEN                   16
#ifndef SLE_UART_SERVER_NAME
#define SLE_UART_SERVER_NAME            "sle_uart_server"
#endif
#define SLE_UART_CLIENT_LOG             "[sle uart client]"

static ssapc_find_service_result_t g_sle_uart_find_service_result = { 0 };
static sle_announce_seek_callbacks_t g_sle_uart_seek_cbk = { 0 };
static sle_connection_callbacks_t g_sle_uart_connect_cbk = { 0 };
static ssapc_callbacks_t g_sle_uart_ssapc_cbk = { 0 };
static sle_addr_t g_sle_uart_remote_addr = { 0 };
ssapc_write_param_t g_sle_uart_send_param = { 0 };
uint16_t g_sle_uart_conn_id = 0;
static int8_t g_rescue_latest_rssi = RESCUE_RSSI_INVALID;
static int g_rescue_latest_distance_dm = 0;
static int g_rescue_previous_distance_dm = 0;
static uint8_t g_rescue_connected = 0;
static uint8_t g_rescue_oled_ready = 0;
static uint8_t g_rescue_oled_task_started = 0;
static uint8_t g_rescue_rssi_task_started = 0;
static uint8_t g_rescue_have_payload = 0;
static char g_rescue_latest_payload[RESCUE_PAYLOAD_CACHE_LEN] = { 0 };
static char g_rescue_victim_id[RESCUE_ID_LEN] = "--";
static int g_rescue_spo2 = 0;
static int g_rescue_hr = 0;
static int g_rescue_sbp = 0;
static int g_rescue_battery = 0;
static int g_rescue_sos = 0;

uint16_t get_g_sle_uart_conn_id(void)
{
    return g_sle_uart_conn_id;
}

ssapc_write_param_t *get_g_sle_uart_send_param(void)
{
    return &g_sle_uart_send_param;
}

int rescue_get_target_distance_dm(void)
{
    return g_rescue_latest_distance_dm;
}

static int rescue_abs_int(int value)
{
    return value < 0 ? -value : value;
}

static int rescue_estimate_distance_dm(int8_t rssi)
{
    if (rssi == RESCUE_RSSI_INVALID) {
        return 0;
    }
    if (rssi >= -45) {
        return 10;
    }
    if (rssi >= -55) {
        return 20 + (-45 - rssi) * 3;
    }
    if (rssi >= -65) {
        return 60 + (-55 - rssi) * 8;
    }
    if (rssi >= -75) {
        return 160 + (-65 - rssi) * 18;
    }
    if (rssi >= -85) {
        return 380 + (-75 - rssi) * 40;
    }
    return 900;
}

static const char *rescue_distance_level(int distance_dm)
{
    if (distance_dm <= 0) {
        return "UNKNOWN";
    }
    if (distance_dm <= 30) {
        return "VERY_NEAR";
    }
    if (distance_dm <= 120) {
        return "NEAR";
    }
    if (distance_dm <= 350) {
        return "MID";
    }
    return "FAR";
}

static const char *rescue_trend_text(int current_dm, int previous_dm)
{
    if (previous_dm == 0 || current_dm == 0 || rescue_abs_int(current_dm - previous_dm) < 15) {
        return "STABLE";
    }
    return current_dm < previous_dm ? "CLOSING" : "AWAY";
}

static int rescue_json_read_int(const char *payload, const char *key, int fallback)
{
    const char *pos = NULL;
    if (payload == NULL || key == NULL) {
        return fallback;
    }
    pos = strstr(payload, key);
    if (pos == NULL) {
        return fallback;
    }
    pos += strlen(key);
    return (int)strtol(pos, NULL, 10);
}

static void rescue_json_read_string(const char *payload, const char *key, char *out, size_t out_len)
{
    const char *pos = NULL;
    const char *end = NULL;
    size_t len;
    if (payload == NULL || key == NULL || out == NULL || out_len == 0) {
        return;
    }
    pos = strstr(payload, key);
    if (pos == NULL) {
        return;
    }
    pos += strlen(key);
    end = strchr(pos, '"');
    if (end == NULL) {
        return;
    }
    len = (size_t)(end - pos);
    if (len >= out_len) {
        len = out_len - 1;
    }
    (void)memcpy_s(out, out_len, pos, len);
    out[len] = '\0';
}

static void rescue_cache_payload(const char *payload)
{
    if (payload == NULL) {
        return;
    }
    (void)strncpy_s(g_rescue_latest_payload, sizeof(g_rescue_latest_payload), payload,
        sizeof(g_rescue_latest_payload) - 1);
    rescue_json_read_string(payload, "\"id\":\"", g_rescue_victim_id, sizeof(g_rescue_victim_id));
    g_rescue_spo2 = rescue_json_read_int(payload, "\"spo2\":", g_rescue_spo2);
    g_rescue_hr = rescue_json_read_int(payload, "\"hr\":", g_rescue_hr);
    g_rescue_sbp = rescue_json_read_int(payload, "\"sbp\":", g_rescue_sbp);
    g_rescue_battery = rescue_json_read_int(payload, "\"battery\":", g_rescue_battery);
    g_rescue_sos = rescue_json_read_int(payload, "\"sos\":", g_rescue_sos);
    g_rescue_have_payload = 1;
}

static void rescue_oled_draw_line(uint8_t y, const char *text)
{
    char line[22] = { 0 };
    if (!g_rescue_oled_ready || text == NULL) {
        return;
    }
    (void)strncpy_s(line, sizeof(line), text, sizeof(line) - 1);
    ssd1306_SetCursor(0, y);
    (void)ssd1306_DrawString(line, Font_6x8, White);
}

static void rescue_oled_render(void)
{
    char line[32] = { 0 };
    int current_dm = g_rescue_latest_distance_dm;
    const char *trend = rescue_trend_text(current_dm, g_rescue_previous_distance_dm);

    if (!g_rescue_oled_ready) {
        return;
    }

    ssd1306_Fill(Black);
    rescue_oled_draw_line(0, "RESCUE TRACK");
    (void)snprintf_s(line, sizeof(line), sizeof(line) - 1, "LINK %s",
        g_rescue_connected ? "CONNECTED" : "SCANNING");
    rescue_oled_draw_line(8, line);

    if (g_rescue_latest_rssi == RESCUE_RSSI_INVALID) {
        rescue_oled_draw_line(16, "RSSI --");
    } else {
        (void)snprintf_s(line, sizeof(line), sizeof(line) - 1, "RSSI %d dBm", g_rescue_latest_rssi);
        rescue_oled_draw_line(16, line);
    }

    if (current_dm > 0) {
        (void)snprintf_s(line, sizeof(line), sizeof(line) - 1, "DIST %d.%dm %s",
            current_dm / 10, current_dm % 10, rescue_distance_level(current_dm));
        rescue_oled_draw_line(24, line);
        (void)snprintf_s(line, sizeof(line), sizeof(line) - 1, "TREND %s", trend);
        rescue_oled_draw_line(32, line);
    } else {
        rescue_oled_draw_line(24, "DIST --.-m");
        rescue_oled_draw_line(32, "WAIT TARGET");
    }

    if (g_rescue_have_payload) {
        (void)snprintf_s(line, sizeof(line), sizeof(line) - 1, "ID %s", g_rescue_victim_id);
        rescue_oled_draw_line(40, line);
        (void)snprintf_s(line, sizeof(line), sizeof(line) - 1, "SpO2 %d HR %d", g_rescue_spo2, g_rescue_hr);
        rescue_oled_draw_line(48, line);
        (void)snprintf_s(line, sizeof(line), sizeof(line) - 1, "SOS %s BAT %d%%",
            g_rescue_sos ? "YES" : "NO", g_rescue_battery);
        rescue_oled_draw_line(56, line);
    } else {
        rescue_oled_draw_line(40, "NO VICTIM DATA");
        rescue_oled_draw_line(48, "POWER BOTH BOARDS");
        rescue_oled_draw_line(56, "WAIT SLE REPORT");
    }
    ssd1306_UpdateScreen();
}

static void rescue_oled_init(void)
{
    errcode_t ret;
    uapi_pin_set_mode(RESCUE_OLED_SCL_PIN, RESCUE_OLED_PIN_MODE);
    uapi_pin_set_mode(RESCUE_OLED_SDA_PIN, RESCUE_OLED_PIN_MODE);
    ret = uapi_i2c_master_init(RESCUE_OLED_I2C_BUS, RESCUE_OLED_I2C_BAUDRATE, RESCUE_OLED_I2C_MASTER_ADDR);
    if (ret != ERRCODE_SUCC) {
        osal_printk("[RESCUE_OLED] i2c init failed:0x%x\r\n", ret);
        return;
    }
    ssd1306_Init();
    g_rescue_oled_ready = 1;
    rescue_oled_render();
}

static void *rescue_oled_task(const char *arg)
{
    unused(arg);
    rescue_oled_init();
    while (1) {
        rescue_oled_render();
        osal_msleep(RESCUE_OLED_REFRESH_MS);
    }
    return NULL;
}

void rescue_print_target_state(const char *payload)
{
    int current_dm = g_rescue_latest_distance_dm;
    const char *trend = rescue_trend_text(current_dm, g_rescue_previous_distance_dm);
    rescue_cache_payload(payload);
    osal_printk("[RESCUE_TRACK] rssi=%d distance=%d.%dm level=%s trend=%s payload=%s\r\n",
        g_rescue_latest_rssi, current_dm / 10, current_dm % 10, rescue_distance_level(current_dm), trend, payload);
    if (current_dm > 0) {
        g_rescue_previous_distance_dm = current_dm;
    }
}

static void rescue_read_rssi_cb(uint16_t conn_id, int8_t rssi, errcode_t status)
{
    unused(conn_id);
    if (status != ERRCODE_SUCC) {
        osal_printk("[RESCUE_TRACK] read rssi failed:0x%x\r\n", status);
        return;
    }
    g_rescue_latest_rssi = rssi;
    g_rescue_latest_distance_dm = rescue_estimate_distance_dm(rssi);
    osal_printk("[RESCUE_TRACK] rssi=%d distance=%d.%dm level=%s\r\n",
        rssi, g_rescue_latest_distance_dm / 10, g_rescue_latest_distance_dm % 10,
        rescue_distance_level(g_rescue_latest_distance_dm));
}

static void *rescue_rssi_poll_task(const char *arg)
{
    unused(arg);
    while (1) {
        if (g_rescue_connected) {
            sle_read_remote_device_rssi(g_sle_uart_conn_id);
        }
        osal_msleep(RESCUE_RSSI_POLL_INTERVAL_MS);
    }
    return NULL;
}

static void rescue_start_oled_task(void)
{
    if (g_rescue_oled_task_started) {
        return;
    }
    osal_task *oled_task_handle = osal_kthread_create((osal_kthread_handler)rescue_oled_task, 0,
                                                      "RescueOledTask", RESCUE_OLED_TASK_STACK_SIZE);
    if (oled_task_handle != NULL) {
        osal_kthread_set_priority(oled_task_handle, RESCUE_OLED_TASK_PRIO);
        g_rescue_oled_task_started = 1;
    }
}

void sle_uart_start_scan(void)
{
    sle_seek_param_t param = { 0 };
    param.own_addr_type = 0;
    param.filter_duplicates = 0;
    param.seek_filter_policy = 0;
    param.seek_phys = 1;
    param.seek_type[0] = 1;
    param.seek_interval[0] = SLE_SEEK_INTERVAL_DEFAULT;
    param.seek_window[0] = SLE_SEEK_WINDOW_DEFAULT;
    sle_set_seek_param(&param);
    sle_start_seek();
}

static void sle_uart_client_sample_sle_enable_cbk(errcode_t status)
{
    osal_printk("sle enable: %d.\r\n", status);
    sle_uart_client_init(sle_uart_notification_cb, sle_uart_indication_cb);
    sle_uart_start_scan();
}

static void sle_uart_client_sample_seek_enable_cbk(errcode_t status)
{
    if (status != 0) {
        osal_printk("%s sle_uart_client_sample_seek_enable_cbk,status error\r\n", SLE_UART_CLIENT_LOG);
    }
}

static void sle_uart_client_sample_seek_result_info_cbk(sle_seek_result_info_t *seek_result_data)
{
    osal_printk("%s sle uart scan data :%s\r\n", SLE_UART_CLIENT_LOG, seek_result_data->data);
    if (seek_result_data == NULL) {
        osal_printk("status error\r\n");
    } else if (strstr((const char *)seek_result_data->data, SLE_UART_SERVER_NAME) != NULL) {
        g_rescue_latest_rssi = (int8_t)seek_result_data->rssi;
        g_rescue_latest_distance_dm = rescue_estimate_distance_dm(g_rescue_latest_rssi);
        osal_printk("[RESCUE_TRACK] target found by scan rssi=%d distance=%d.%dm level=%s\r\n",
            g_rescue_latest_rssi, g_rescue_latest_distance_dm / 10, g_rescue_latest_distance_dm % 10,
            rescue_distance_level(g_rescue_latest_distance_dm));
        memcpy_s(&g_sle_uart_remote_addr, sizeof(sle_addr_t), &seek_result_data->addr, sizeof(sle_addr_t));
        sle_stop_seek();
    }
}

static void sle_uart_client_sample_seek_disable_cbk(errcode_t status)
{
    if (status != 0) {
        osal_printk("%s sle_uart_client_sample_seek_disable_cbk,status error = %x\r\n", SLE_UART_CLIENT_LOG, status);
    } else {
        sle_remove_paired_remote_device(&g_sle_uart_remote_addr);
        sle_connect_remote_device(&g_sle_uart_remote_addr);
    }
}

static void sle_uart_client_sample_seek_cbk_register(void)
{
    g_sle_uart_seek_cbk.sle_enable_cb = sle_uart_client_sample_sle_enable_cbk;
    g_sle_uart_seek_cbk.seek_enable_cb = sle_uart_client_sample_seek_enable_cbk;
    g_sle_uart_seek_cbk.seek_result_cb = sle_uart_client_sample_seek_result_info_cbk;
    g_sle_uart_seek_cbk.seek_disable_cb = sle_uart_client_sample_seek_disable_cbk;
    sle_announce_seek_register_callbacks(&g_sle_uart_seek_cbk);
}

#ifdef CONFIG_SAMPLE_SUPPORT_LOW_LATENCY_TYPE
static void sle_uart_client_sample_set_phy_param(void)
{
#ifdef CONFIG_SAMPLE_SUPPORT_PERFORMANCE_TYPE
    sle_set_phy_t param = {0};
    param.tx_format = 1;         // 0 :无线帧类型1(GFSK); 1:无线帧类型2(QPSK)
    param.rx_format = 1;         //
    param.tx_phy = 2;            // 0：1M; 1:2M; 2:4M;
    param.rx_phy = 2;            //
    param.tx_pilot_density = 0x2;  // 导频密度16:1
    param.rx_pilot_density = 0x2;  // 导频密度16:1
    param.g_feedback = 0;
    param.t_feedback = 0;
    if (sle_set_phy_param(get_g_sle_uart_conn_id(), &param) != 0) {
        osal_printk("%s sle_set_phy_param fail\r\n", SLE_UART_CLIENT_LOG);
        return;
    }
    osal_printk("%s sle_set_phy_param success\r\n", SLE_UART_CLIENT_LOG);
#else
    // 非跑流sample使用原phy参数
#endif
    return;
}
#endif

static void sle_uart_client_sample_connect_state_changed_cbk(uint16_t conn_id, const sle_addr_t *addr,
                                                             sle_acb_state_t conn_state, sle_pair_state_t pair_state,
                                                             sle_disc_reason_t disc_reason)
{
    unused(addr);
    unused(pair_state);
    osal_printk("%s conn state changed disc_reason:0x%x\r\n", SLE_UART_CLIENT_LOG, disc_reason);
    g_sle_uart_conn_id = conn_id;
    if (conn_state == SLE_ACB_STATE_CONNECTED) {
        g_rescue_connected = 1;
        osal_printk("%s SLE_ACB_STATE_CONNECTED\r\n", SLE_UART_CLIENT_LOG);
        if (pair_state == SLE_PAIR_NONE) {
            sle_pair_remote_device(&g_sle_uart_remote_addr);
        }
#ifdef CONFIG_SAMPLE_SUPPORT_LOW_LATENCY_TYPE
        sle_uart_client_sample_set_phy_param();
        osal_msleep(SLE_UART_TASK_DELAY_MS);
        sle_low_latency_rx_enable();
        sle_low_latency_set(get_g_sle_uart_conn_id(), true, SLE_UART_LOW_LATENCY_2K);
#endif
        osal_printk("%s sle_low_latency_rx_enable \r\n", SLE_UART_CLIENT_LOG);
    } else if (conn_state == SLE_ACB_STATE_NONE) {
        osal_printk("%s SLE_ACB_STATE_NONE\r\n", SLE_UART_CLIENT_LOG);
    } else if (conn_state == SLE_ACB_STATE_DISCONNECTED) {
        g_rescue_connected = 0;
        osal_printk("%s SLE_ACB_STATE_DISCONNECTED\r\n", SLE_UART_CLIENT_LOG);
        sle_remove_paired_remote_device(&g_sle_uart_remote_addr);
        sle_uart_start_scan();
    } else {
        osal_printk("%s status error\r\n", SLE_UART_CLIENT_LOG);
    }
}

void  sle_uart_client_sample_pair_complete_cbk(uint16_t conn_id, const sle_addr_t *addr, errcode_t status)
{
    osal_printk("%s pair complete conn_id:%d, addr:%02x***%02x%02x\n", SLE_UART_CLIENT_LOG, conn_id,
                addr->addr[0], addr->addr[4], addr->addr[5]);
    if (status == 0) {
        ssap_exchange_info_t info = {0};
        info.mtu_size = SLE_MTU_SIZE_DEFAULT;
        info.version = 1;
        ssapc_exchange_info_req(0, g_sle_uart_conn_id, &info);
    }
}

static void sle_uart_client_sample_connect_cbk_register(void)
{
    g_sle_uart_connect_cbk.connect_state_changed_cb = sle_uart_client_sample_connect_state_changed_cbk;
    g_sle_uart_connect_cbk.pair_complete_cb =  sle_uart_client_sample_pair_complete_cbk;
    g_sle_uart_connect_cbk.read_rssi_cb = rescue_read_rssi_cb;
    sle_connection_register_callbacks(&g_sle_uart_connect_cbk);
}

static void sle_uart_client_sample_exchange_info_cbk(uint8_t client_id, uint16_t conn_id, ssap_exchange_info_t *param,
                                                     errcode_t status)
{
    osal_printk("%s exchange_info_cbk,pair complete client id:%d status:%d\r\n",
                SLE_UART_CLIENT_LOG, client_id, status);
    osal_printk("%s exchange mtu, mtu size: %d, version: %d.\r\n", SLE_UART_CLIENT_LOG,
                param->mtu_size, param->version);
    ssapc_find_structure_param_t find_param = { 0 };
    find_param.type = SSAP_FIND_TYPE_PROPERTY;
    find_param.start_hdl = 1;
    find_param.end_hdl = 0xFFFF;
    ssapc_find_structure(0, conn_id, &find_param);
}

static void sle_uart_client_sample_find_structure_cbk(uint8_t client_id, uint16_t conn_id,
                                                      ssapc_find_service_result_t *service,
                                                      errcode_t status)
{
    osal_printk("%s find structure cbk client: %d conn_id:%d status: %d \r\n", SLE_UART_CLIENT_LOG,
                client_id, conn_id, status);
    osal_printk("%s find structure start_hdl:[0x%02x], end_hdl:[0x%02x], uuid len:%d\r\n", SLE_UART_CLIENT_LOG,
                service->start_hdl, service->end_hdl, service->uuid.len);
    g_sle_uart_find_service_result.start_hdl = service->start_hdl;
    g_sle_uart_find_service_result.end_hdl = service->end_hdl;
    memcpy_s(&g_sle_uart_find_service_result.uuid, sizeof(sle_uuid_t), &service->uuid, sizeof(sle_uuid_t));
}

static void sle_uart_client_sample_find_property_cbk(uint8_t client_id, uint16_t conn_id,
                                                     ssapc_find_property_result_t *property, errcode_t status)
{
    osal_printk("%s sle_uart_client_sample_find_property_cbk, client id: %d, conn id: %d, operate ind: %d, "
                "descriptors count: %d status:%d property->handle %d\r\n", SLE_UART_CLIENT_LOG,
                client_id, conn_id, property->operate_indication,
                property->descriptors_count, status, property->handle);
    g_sle_uart_send_param.handle = property->handle;
    g_sle_uart_send_param.type = SSAP_PROPERTY_TYPE_VALUE;
}

static void sle_uart_client_sample_find_structure_cmp_cbk(uint8_t client_id, uint16_t conn_id,
                                                          ssapc_find_structure_result_t *structure_result,
                                                          errcode_t status)
{
    unused(conn_id);
    osal_printk("%s sle_uart_client_sample_find_structure_cmp_cbk,client id:%d status:%d type:%d uuid len:%d \r\n",
                SLE_UART_CLIENT_LOG, client_id, status, structure_result->type, structure_result->uuid.len);
}

static void sle_uart_client_sample_write_cfm_cb(uint8_t client_id, uint16_t conn_id,
                                                ssapc_write_result_t *write_result, errcode_t status)
{
    osal_printk("%s sle_uart_client_sample_write_cfm_cb, conn_id:%d client id:%d status:%d handle:%02x type:%02x\r\n",
                SLE_UART_CLIENT_LOG, conn_id, client_id, status, write_result->handle, write_result->type);
}

static void sle_uart_client_sample_ssapc_cbk_register(ssapc_notification_callback notification_cb,
                                                      ssapc_notification_callback indication_cb)
{
    g_sle_uart_ssapc_cbk.exchange_info_cb = sle_uart_client_sample_exchange_info_cbk;
    g_sle_uart_ssapc_cbk.find_structure_cb = sle_uart_client_sample_find_structure_cbk;
    g_sle_uart_ssapc_cbk.ssapc_find_property_cbk = sle_uart_client_sample_find_property_cbk;
    g_sle_uart_ssapc_cbk.find_structure_cmp_cb = sle_uart_client_sample_find_structure_cmp_cbk;
    g_sle_uart_ssapc_cbk.write_cfm_cb = sle_uart_client_sample_write_cfm_cb;
    g_sle_uart_ssapc_cbk.notification_cb = notification_cb;
    g_sle_uart_ssapc_cbk.indication_cb = indication_cb;
    ssapc_register_callbacks(&g_sle_uart_ssapc_cbk);
}

#ifdef CONFIG_SAMPLE_SUPPORT_LOW_LATENCY_TYPE
#include "uart.h"
#ifdef CONFIG_SAMPLE_SUPPORT_PERFORMANCE_TYPE
#include "tcxo.h"
static uint32_t g_sle_recv_count = 0;
static uint64_t g_sle_recv_start_time = 0;
static uint64_t g_sle_recv_end_time = 0;
static uint64_t g_sle_recv_param[2] = { 0 };
#endif
void sle_uart_client_low_latency_recv_data_cbk(uint16_t len, uint8_t *value)
{
#ifdef CONFIG_SAMPLE_SUPPORT_PERFORMANCE_TYPE
    static uint64_t sle_throughput = 0;
    if (value == NULL || len == 0) {
        return;
    }
    g_sle_recv_count++;
    if (g_sle_recv_count == 1) {
        g_sle_recv_start_time = uapi_tcxo_get_us();
    } else if (g_sle_recv_count == SLE_UART_RECV_CNT) {
        g_sle_recv_end_time = uapi_tcxo_get_us();
        g_sle_recv_param[0] = g_sle_recv_count;
        g_sle_recv_param[1] = g_sle_recv_end_time - g_sle_recv_start_time;
        g_sle_recv_count = 0;
        g_sle_recv_end_time = 0;
        g_sle_recv_start_time = 0;
        uint64_t tmp;
        tmp = g_sle_recv_param[1] / 1000; // 1000 代表us转化成ms
        sle_throughput = len * SLE_UART_RECV_CNT * 8 / tmp; // 8 代表1byte = 8bit
        osal_printk("recv_len = %d, recv_count = %llu\r\n", len, g_sle_recv_param[0]);
        osal_printk("diff time:%lluus, throughput:%llukbps\r\n", g_sle_recv_param[1], sle_throughput);
    }
#else
    osal_printk("uart recv low latency data:\r\n");
    uapi_uart_write(CONFIG_SLE_UART_BUS, value, len, 0);
#endif
}

void sle_uart_client_low_latency_recv_data_cbk_register(void)
{
    osal_printk("uart recv low latency data register success\r\n");
    sle_low_latency_rx_callbacks_t cbk_func = { NULL };
    cbk_func.low_latency_rx_cb = (low_latency_general_rx_callback)sle_uart_client_low_latency_recv_data_cbk;
    sle_low_latency_rx_register_callbacks(&cbk_func);
}
#endif

void sle_uart_client_init(ssapc_notification_callback notification_cb, ssapc_indication_callback indication_cb)
{
    rescue_start_oled_task();
    (void)osal_msleep(5000); /* 延时5000ms，等待SLE初始化完毕 */
    osal_printk("[SLE Client] try enable.\r\n");
    sle_uart_client_sample_seek_cbk_register();
    sle_uart_client_sample_connect_cbk_register();
    sle_uart_client_sample_ssapc_cbk_register(notification_cb, indication_cb);
#ifdef CONFIG_SAMPLE_SUPPORT_LOW_LATENCY_TYPE
    sle_uart_client_low_latency_recv_data_cbk_register();
#endif
    if (enable_sle() != ERRCODE_SUCC) {
        osal_printk("[SLE Client] sle enbale fail !\r\n");
    }
    if (!g_rescue_rssi_task_started) {
        osal_task *rescue_task_handle = osal_kthread_create((osal_kthread_handler)rescue_rssi_poll_task, 0,
                                                            "RescueRssiPollTask", 0x1000);
        if (rescue_task_handle != NULL) {
            osal_kthread_set_priority(rescue_task_handle, 29);
            g_rescue_rssi_task_started = 1;
        }
    }
    if (!g_rescue_oled_task_started) {
        osal_task *oled_task_handle = osal_kthread_create((osal_kthread_handler)rescue_oled_task, 0,
                                                          "RescueOledTask", RESCUE_OLED_TASK_STACK_SIZE);
        if (oled_task_handle != NULL) {
            osal_kthread_set_priority(oled_task_handle, RESCUE_OLED_TASK_PRIO);
            g_rescue_oled_task_started = 1;
        }
    }
}

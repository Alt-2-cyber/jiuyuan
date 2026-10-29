#include <stdbool.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include "app_init.h"
#include "common_def.h"
#include "i2c.h"
#include "mac_addr.h"
#include "max30102.h"
#include "pinctrl.h"
#include "platform_core.h"
#include "rescue_route.h"
#include "securec.h"
#include "soc_osal.h"
#include "systick.h"
#include "uart.h"
#include "sle_connection_manager.h"
#include "sle_device_discovery.h"
#include "sle_errcode.h"
#include "sle_ssap_client.h"
#include "sle_ssap_server.h"
#include "sle_uart_server.h"
#include "sle_uart_server_adv.h"

#if defined(CONFIG_SAMPLE_SUPPORT_SLE_RELAY_ROOT)
#include "ground_ui.h"
#endif

#define SLE_RELAY_LOG "[sle relay]"
#define SLE_RELAY_TASK_PRIO 28
#define SLE_RELAY_TASK_STACK_SIZE 0x1800
#define SLE_RELAY_PERIOD_MS 3000
#define SLE_RELAY_SCAN_INTERVAL 100
#define SLE_RELAY_SCAN_WINDOW 100
#define SLE_RELAY_MTU 520
#define SLE_RELAY_PLAIN_LOG_MAX 192
#define SLE_RELAY_MESH_NAME_PREFIX "jm_"
#define SLE_RELAY_MESH_ROOT_DST 0
#define SLE_RELAY_MAX_TTL 4
#define SLE_RELAY_INVALID_NODE_ID 0xFF
#define SLE_RELAY_INVALID_CONN_ID 0xFFFF
#define RESCUE_VICTIM_REPORT_PERIOD_MS 2000
#define RESCUE_VICTIM_SENSOR_POLL_MS 40
#define RESCUE_VICTIM_SENSOR_RETRY_MS 3000
#define RESCUE_VICTIM_I2C_SDA_PIN 15
#define RESCUE_VICTIM_I2C_SCL_PIN 16
#define RESCUE_VICTIM_I2C_PIN_MODE 2
#define RESCUE_VICTIM_I2C_BUS 1
#define RESCUE_VICTIM_I2C_MASTER_ADDR 0
#define RESCUE_VICTIM_I2C_BAUDRATE 400000
#define RESCUE_VICTIM_SENSOR_TASK_STACK_SIZE 0x1000
#define RESCUE_VICTIM_SENSOR_TASK_PRIO 29
#define SLE_RELAY_RSSI_POLL_MS 1000
#define SLE_RELAY_RSSI_EXPIRE_MS 5000
#define SLE_RELAY_RSSI_TASK_STACK_SIZE 0x1000
#define SLE_RELAY_RSSI_TASK_PRIO 30

#if defined(CONFIG_SAMPLE_SUPPORT_SLE_RELAY_MESH)
#define SLE_RELAY_MESH_MODE 1
#else
#define SLE_RELAY_MESH_MODE 0
#endif

#if defined(CONFIG_SAMPLE_SUPPORT_SLE_RELAY_MESH) || defined(CONFIG_SAMPLE_SUPPORT_SLE_RELAY_VICTIM)
static const char *relay_role(void);
#endif

#if defined(CONFIG_SAMPLE_SUPPORT_SLE_RELAY_NODE) || defined(CONFIG_SAMPLE_SUPPORT_SLE_RELAY_VICTIM) || \
    defined(CONFIG_SAMPLE_SUPPORT_SLE_RELAY_LEAF) || \
    defined(CONFIG_SAMPLE_SUPPORT_SLE_RELAY_MESH)
#define SLE_RELAY_HAS_UPSTREAM 1
#else
#define SLE_RELAY_HAS_UPSTREAM 0
#endif

#if defined(CONFIG_SAMPLE_SUPPORT_SLE_RELAY_ROOT) || defined(CONFIG_SAMPLE_SUPPORT_SLE_RELAY_NODE) || \
    (defined(CONFIG_SAMPLE_SUPPORT_SLE_RELAY_VICTIM) && CONFIG_SLE_RELAY_VICTIM_INDEX < 3) || \
    defined(CONFIG_SAMPLE_SUPPORT_SLE_RELAY_MESH)
#define SLE_RELAY_HAS_DOWNSTREAM 1
#else
#define SLE_RELAY_HAS_DOWNSTREAM 0
#endif

#if defined(CONFIG_SAMPLE_SUPPORT_SLE_RELAY_MESH)
#define SLE_RELAY_ROLE relay_role()
#elif defined(CONFIG_SAMPLE_SUPPORT_SLE_RELAY_ROOT)
#define SLE_RELAY_NODE_ID 1
#define SLE_RELAY_ROLE "ROOT"
#elif defined(CONFIG_SAMPLE_SUPPORT_SLE_RELAY_NODE)
#define SLE_RELAY_NODE_ID 2
#define SLE_RELAY_ROLE "SKY"
#elif defined(CONFIG_SAMPLE_SUPPORT_SLE_RELAY_VICTIM)
#define SLE_RELAY_NODE_ID (2 + CONFIG_SLE_RELAY_VICTIM_INDEX)
#define SLE_RELAY_ROLE relay_role()
#else
#define SLE_RELAY_NODE_ID 3
#define SLE_RELAY_ROLE "LEAF"
#endif

#if defined(CONFIG_SAMPLE_SUPPORT_SLE_RELAY_LEAF)
#define SLE_RELAY_TARGET_NAME "nr_relay"
#elif defined(CONFIG_SAMPLE_SUPPORT_SLE_RELAY_NODE)
#define SLE_RELAY_TARGET_NAME "jr_ground"
#elif defined(CONFIG_SAMPLE_SUPPORT_SLE_RELAY_VICTIM)
#if CONFIG_SLE_RELAY_VICTIM_INDEX == 1
#define SLE_RELAY_TARGET_NAME "jr_sky"
#elif CONFIG_SLE_RELAY_VICTIM_INDEX == 2
#define SLE_RELAY_TARGET_NAME "jr_v1"
#else
#define SLE_RELAY_TARGET_NAME "jr_v2"
#endif
#else
#define SLE_RELAY_TARGET_NAME ""
#endif

#if defined(CONFIG_SAMPLE_SUPPORT_SLE_RELAY_MESH)
static uint8_t g_node_id = SLE_RELAY_INVALID_NODE_ID;
static sle_addr_t g_local_addr = {0};
#define SLE_RELAY_NODE_ID g_node_id
static uint8_t g_mesh_best_parent_id = SLE_RELAY_INVALID_NODE_ID;
static sle_addr_t g_mesh_best_parent_addr = {0};
static uint8_t g_mesh_scan_started = 0;
#endif

static sle_connection_callbacks_t g_conn_cbks = {0};

#if SLE_RELAY_HAS_UPSTREAM
static sle_announce_seek_callbacks_t g_seek_cbks = {0};
static ssapc_callbacks_t g_ssapc_cbks = {0};
static sle_addr_t g_upstream_addr = {0};
static ssapc_write_param_t g_upstream_write = {0};
static uint16_t g_upstream_conn_id = SLE_RELAY_INVALID_CONN_ID;
static uint8_t g_upstream_connected = 0;
static uint8_t g_upstream_ready = 0;
static uint8_t g_client_connecting = 0;
#endif

#if SLE_RELAY_HAS_DOWNSTREAM
static uint16_t g_downstream_conn_id = 0;
static uint8_t g_downstream_connected = 0;
#endif

#if SLE_RELAY_HAS_UPSTREAM
static uint32_t g_local_seq = 0;
typedef struct {
    int16_t filtered;
    uint8_t valid;
    uint64_t updated_ms;
} relay_rssi_state_t;

static relay_rssi_state_t g_upstream_rssi = {127, 0, 0};
static osal_mutex g_upstream_rssi_mutex;
static uint8_t g_upstream_rssi_mutex_ready = 0;
static uint8_t g_upstream_rssi_task_started = 0;
#endif

#if defined(CONFIG_SAMPLE_SUPPORT_SLE_RELAY_VICTIM)
static uint8_t g_victim_sensor_task_started = 0;
static uint8_t g_victim_sensor_ready = 0;
static uint8_t g_victim_finger_present = 0;
static uint8_t g_victim_i2c_ready = 0;
static int g_victim_spo2 = 0;
static int g_victim_heart_rate = 0;
#endif

static void relay_plain_log(const char *fmt, ...);
#if SLE_RELAY_HAS_UPSTREAM
static void relay_send_upstream(const uint8_t *data, uint16_t len);
static void relay_format_packet(char *out, uint16_t out_len, uint8_t dst, const char *body);
static void relay_read_rssi_cb(uint16_t conn_id, int8_t rssi, errcode_t status);

static bool relay_data_contains(const uint8_t *data, uint8_t data_len, const char *target)
{
    size_t target_len = strlen(target);

    if (data == NULL || target_len == 0 || target_len > data_len) {
        return false;
    }
    for (size_t i = 0; i + target_len <= data_len; i++) {
        if (memcmp(&data[i], target, target_len) == 0) {
            return true;
        }
    }
    return false;
}
#endif
#if SLE_RELAY_HAS_UPSTREAM && SLE_RELAY_HAS_DOWNSTREAM
static void relay_send_downstream(const uint8_t *data, uint16_t len);
#endif

#if defined(CONFIG_SAMPLE_SUPPORT_SLE_RELAY_MESH) || defined(CONFIG_SAMPLE_SUPPORT_SLE_RELAY_VICTIM)
static const char *relay_role(void)
{
#if defined(CONFIG_SAMPLE_SUPPORT_SLE_RELAY_MESH)
    if (g_upstream_ready) {
        return "MESH-RELAY";
    }
    return "MESH-ROOT";
#elif defined(CONFIG_SAMPLE_SUPPORT_SLE_RELAY_VICTIM)
#if CONFIG_SLE_RELAY_VICTIM_INDEX == 1
    return "VICTIM-1";
#elif CONFIG_SLE_RELAY_VICTIM_INDEX == 2
    return "VICTIM-2";
#else
    return "VICTIM-3";
#endif
#else
    return SLE_RELAY_ROLE;
#endif
}
#endif

#if defined(CONFIG_SAMPLE_SUPPORT_SLE_RELAY_VICTIM)
static const char *rescue_victim_id(void)
{
#if CONFIG_SLE_RELAY_VICTIM_INDEX == 1
    return "VD-001";
#elif CONFIG_SLE_RELAY_VICTIM_INDEX == 2
    return "VD-002";
#else
    return "VD-003";
#endif
}

static int rescue_victim_i2c_prepare(void)
{
    errcode_t ret;
    errcode_t sda_mode_ret;
    errcode_t scl_mode_ret;
    errcode_t sda_pull_ret;
    errcode_t scl_pull_ret;

    if (g_victim_i2c_ready) {
        return 1;
    }
    sda_mode_ret = uapi_pin_set_mode(RESCUE_VICTIM_I2C_SDA_PIN, RESCUE_VICTIM_I2C_PIN_MODE);
    scl_mode_ret = uapi_pin_set_mode(RESCUE_VICTIM_I2C_SCL_PIN, RESCUE_VICTIM_I2C_PIN_MODE);
    sda_pull_ret = uapi_pin_set_pull(RESCUE_VICTIM_I2C_SDA_PIN, PIN_PULL_TYPE_UP);
    scl_pull_ret = uapi_pin_set_pull(RESCUE_VICTIM_I2C_SCL_PIN, PIN_PULL_TYPE_UP);
    ret = uapi_i2c_master_init(RESCUE_VICTIM_I2C_BUS, RESCUE_VICTIM_I2C_BAUDRATE,
                               RESCUE_VICTIM_I2C_MASTER_ADDR);
    relay_plain_log("[VICTIM_I2C_DIAG] id=%s bus=%u baud=%lu init=0x%08lx\r\n", rescue_victim_id(),
                    (unsigned int)RESCUE_VICTIM_I2C_BUS, (unsigned long)RESCUE_VICTIM_I2C_BAUDRATE,
                    (unsigned long)ret);
    relay_plain_log("[VICTIM_I2C_DIAG] sda=%u mode=0x%08lx pull=0x%08lx scl=%u mode=0x%08lx pull=0x%08lx\r\n",
                    (unsigned int)RESCUE_VICTIM_I2C_SDA_PIN, (unsigned long)sda_mode_ret,
                    (unsigned long)sda_pull_ret, (unsigned int)RESCUE_VICTIM_I2C_SCL_PIN,
                    (unsigned long)scl_mode_ret, (unsigned long)scl_pull_ret);
    if (ret != ERRCODE_SUCC && ret != ERRCODE_I2C_ALREADY_INIT) {
        relay_plain_log("[VICTIM_SENSOR] id=%s i2c_init_failed=0x%x\r\n", rescue_victim_id(), ret);
        return 0;
    }
    g_victim_i2c_ready = 1;
    return 1;
}

static void *rescue_victim_sensor_task(const char *arg)
{
    int spo2 = 0;
    int heart_rate = 0;
    int finger_present = 0;

    unused(arg);
    while (!rescue_victim_i2c_prepare()) {
        osal_msleep(RESCUE_VICTIM_SENSOR_RETRY_MS);
    }
    while (1) {
        if (!g_victim_sensor_ready) {
            g_victim_sensor_ready = (max30102_init(RESCUE_VICTIM_I2C_BUS) == 0) ? 1 : 0;
            relay_plain_log("[VICTIM_SENSOR] id=%s max30102=%s\r\n", rescue_victim_id(),
                            g_victim_sensor_ready ? "READY" : "NOT_FOUND");
            if (!g_victim_sensor_ready) {
                osal_msleep(RESCUE_VICTIM_SENSOR_RETRY_MS);
                continue;
            }
        }
        if (max30102_update() < 0) {
            g_victim_sensor_ready = 0;
            g_victim_finger_present = 0;
            continue;
        }
        (void)max30102_get_latest(&spo2, &heart_rate, &finger_present);
        g_victim_finger_present = (uint8_t)(finger_present ? 1 : 0);
        g_victim_spo2 = spo2;
        g_victim_heart_rate = heart_rate;
        osal_msleep(RESCUE_VICTIM_SENSOR_POLL_MS);
    }
    return NULL;
}

static void rescue_victim_start_sensor_task(void)
{
    osal_task *task;

    if (g_victim_sensor_task_started) {
        return;
    }
    task = osal_kthread_create((osal_kthread_handler)rescue_victim_sensor_task, 0,
                               "RelayVictimSensor", RESCUE_VICTIM_SENSOR_TASK_STACK_SIZE);
    if (task == NULL) {
        relay_plain_log("[VICTIM_SENSOR] id=%s task_create_failed\r\n", rescue_victim_id());
        return;
    }
    osal_kthread_set_priority(task, RESCUE_VICTIM_SENSOR_TASK_PRIO);
    g_victim_sensor_task_started = 1;
}

static void rescue_victim_fallback_values(uint32_t seq, int *spo2, int *heart_rate, int *systolic, int *sos)
{
#if CONFIG_SLE_RELAY_VICTIM_INDEX == 1
    *spo2 = 91 + (int)(seq % 2);
    *heart_rate = 116 + (int)(seq % 4);
    *systolic = 96 + (int)(seq % 3);
    *sos = 1;
#elif CONFIG_SLE_RELAY_VICTIM_INDEX == 2
    *spo2 = 95 + (int)(seq % 2);
    *heart_rate = 92 + (int)(seq % 5);
    *systolic = 108 + (int)(seq % 4);
    *sos = 0;
#else
    *spo2 = 87 + (int)(seq % 2);
    *heart_rate = 128 + (int)(seq % 6);
    *systolic = 90 + (int)(seq % 3);
    *sos = 1;
#endif
}

static void rescue_victim_send_state(void)
{
    char report[160] = {0};
    char packet[256] = {0};
    int spo2 = g_victim_spo2;
    int heart_rate = g_victim_heart_rate;
    int systolic = 0;
    int sos = 0;
    int battery = 88 - (int)(g_local_seq % 17);
    bool real_sensor = g_victim_sensor_ready && g_victim_finger_present && spo2 > 0 && heart_rate > 0;

    if (!real_sensor) {
        rescue_victim_fallback_values(g_local_seq, &spo2, &heart_rate, &systolic, &sos);
    } else {
        systolic = 102 + (int)(g_local_seq % 5);
        sos = (spo2 < 90 || heart_rate > 125) ? 1 : 0;
    }
    (void)snprintf_s(report, sizeof(report), sizeof(report) - 1,
        "{\"type\":\"VICTIM\",\"id\":\"%s\",\"seq\":%lu,\"spo2\":%d,\"hr\":%d,"
        "\"sbp\":%d,\"battery\":%d,\"sos\":%d,\"source\":\"%s\"}",
        rescue_victim_id(), (unsigned long)g_local_seq, spo2, heart_rate, systolic, battery, sos,
        real_sensor ? "MAX30102" : "SIM");
    relay_format_packet(packet, sizeof(packet), 1, report);
    relay_send_upstream((const uint8_t *)packet, (uint16_t)strlen(packet));
    relay_plain_log("[VICTIM_TX] role=%s id=%s source=%s seq=%lu\r\n", SLE_RELAY_ROLE,
                    rescue_victim_id(), real_sensor ? "MAX30102" : "SIM", (unsigned long)(g_local_seq - 1));
}
#endif

#if defined(CONFIG_SAMPLE_SUPPORT_SLE_RELAY_MESH)
static uint8_t relay_mesh_node_id_from_addr(const uint8_t *addr)
{
    uint8_t id = addr[SLE_ADDR_LEN - 1];
    if (id != 0 && id != SLE_RELAY_INVALID_NODE_ID) {
        return id;
    }
    id = (uint8_t)(addr[SLE_ADDR_LEN - 2] ^ addr[SLE_ADDR_LEN - 3] ^ addr[0]);
    return (id == 0 || id == SLE_RELAY_INVALID_NODE_ID) ? 1 : id;
}

static void relay_mesh_init_identity(void)
{
    char name[16] = {0};
    errcode_t addr_ret;

    g_local_addr.type = 0;
    addr_ret = get_dev_addr(g_local_addr.addr, SLE_ADDR_LEN, IFTYPE_SLE);
    if (addr_ret != ERRCODE_SUCC) {
        uint8_t fallback[SLE_ADDR_LEN] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x70};
        (void)memcpy_s(g_local_addr.addr, SLE_ADDR_LEN, fallback, SLE_ADDR_LEN);
    }
    g_node_id = relay_mesh_node_id_from_addr(g_local_addr.addr);
    (void)snprintf_s(name, sizeof(name), sizeof(name) - 1, SLE_RELAY_MESH_NAME_PREFIX "%u", g_node_id);
    sle_uart_server_adv_set_identity(name, g_local_addr.addr);
    relay_plain_log("[JRTEST] mesh_identity id=%u name=%s get=0x%x addr=%02x:%02x:%02x:%02x:%02x:%02x\r\n",
        g_node_id, name, addr_ret, g_local_addr.addr[0], g_local_addr.addr[1], g_local_addr.addr[2],
        g_local_addr.addr[3], g_local_addr.addr[4], g_local_addr.addr[5]);
}

void sle_relay_apply_local_addr(void)
{
    errcode_t set_ret = sle_set_local_addr(&g_local_addr);
    relay_plain_log("[JRTEST] role=%s set_local_addr ret=0x%x id=%u\r\n", SLE_RELAY_ROLE, set_ret, g_node_id);
}

static bool relay_mesh_parse_peer_id(const uint8_t *data, uint8_t len, uint8_t *peer_id)
{
    const char *prefix = SLE_RELAY_MESH_NAME_PREFIX;
    size_t prefix_len = strlen(prefix);

    if (data == NULL || peer_id == NULL || len <= prefix_len) {
        return false;
    }
    for (uint8_t i = 0; i + prefix_len < len; i++) {
        if (memcmp(&data[i], prefix, prefix_len) != 0) {
            continue;
        }
        uint32_t id = 0;
        uint8_t j = (uint8_t)(i + prefix_len);
        bool has_digit = false;
        while (j < len && data[j] >= '0' && data[j] <= '9') {
            id = id * 10 + (uint32_t)(data[j] - '0');
            has_digit = true;
            j++;
        }
        if (!has_digit || id == 0 || id > 254) {
            return false;
        }
        *peer_id = (uint8_t)id;
        return true;
    }
    return false;
}

static void relay_mesh_choose_parent(const sle_seek_result_info_t *result, uint8_t peer_id)
{
    if (peer_id >= g_node_id || peer_id == 0) {
        return;
    }
    if (g_mesh_best_parent_id != SLE_RELAY_INVALID_NODE_ID && peer_id <= g_mesh_best_parent_id) {
        return;
    }
    g_mesh_best_parent_id = peer_id;
    (void)memcpy_s(&g_mesh_best_parent_addr, sizeof(g_mesh_best_parent_addr), &result->addr, sizeof(result->addr));
    relay_plain_log("[JRTEST] role=%s candidate_parent id=%u rssi=%d\r\n",
        SLE_RELAY_ROLE, g_mesh_best_parent_id, (int8_t)result->rssi);
}

static bool relay_mesh_forward_or_ack_from_downstream(const uint8_t *data, uint16_t len)
{
    if (g_upstream_ready) {
        relay_send_upstream(data, len);
        return true;
    }
    char ack[160] = {0};
    (void)snprintf_s(ack, sizeof(ack), sizeof(ack) - 1, "JR|src=%u|dst=0|ttl=%u|seq=%lu|ack=%.*s",
        g_node_id, SLE_RELAY_MAX_TTL, (unsigned long)g_local_seq++, len, data);
    relay_send_downstream((const uint8_t *)ack, (uint16_t)strlen(ack));
    return true;
}
#endif

static void relay_plain_log(const char *fmt, ...)
{
    char buf[SLE_RELAY_PLAIN_LOG_MAX] = {0};
    va_list args;
    int32_t len;

    va_start(args, fmt);
    len = vsnprintf_s(buf, sizeof(buf), sizeof(buf) - 1, fmt, args);
    va_end(args);
    if (len <= 0) {
        return;
    }

    (void)uapi_uart_write(UART_BUS_0, (const uint8_t *)buf, (uint32_t)len, 0);
#if UART_BUS_MAX_NUMBER > 1
    (void)uapi_uart_write(UART_BUS_1, (const uint8_t *)buf, (uint32_t)len, 0);
#endif
#if UART_BUS_MAX_NUMBER > 2
    (void)uapi_uart_write(UART_BUS_2, (const uint8_t *)buf, (uint32_t)len, 0);
#endif
}

#if SLE_RELAY_HAS_UPSTREAM
static void relay_send_upstream(const uint8_t *data, uint16_t len)
{
    rescue_route_packet_t packet;
    uint8_t rewritten[RESCUE_ROUTE_WIRE_CAP] = {0};
    size_t rewritten_len = 0;
    int16_t rssi = 127;

    if (!g_upstream_ready) {
        osal_printk("%s upstream not ready, drop:%.*s\r\n", SLE_RELAY_LOG, len, data);
        relay_plain_log("[JRTEST] role=%s upstream_not_ready drop=%.*s\r\n", SLE_RELAY_ROLE, len, data);
        return;
    }
    if (!rescue_route_decode(data, len, &packet)) {
        relay_plain_log("[JRTEST] role=%s route_decode_failed len=%u\r\n", SLE_RELAY_ROLE, len);
        return;
    }
    if (g_upstream_rssi_mutex_ready) {
        uint64_t now_ms = uapi_systick_get_ms();
        osal_mutex_lock(&g_upstream_rssi_mutex);
        if (g_upstream_rssi.valid && now_ms >= g_upstream_rssi.updated_ms &&
            now_ms - g_upstream_rssi.updated_ms <= SLE_RELAY_RSSI_EXPIRE_MS) {
            rssi = g_upstream_rssi.filtered;
        }
        osal_mutex_unlock(&g_upstream_rssi_mutex);
    }
    if (!rescue_route_add_hop(&packet, rssi) ||
        !rescue_route_encode(&packet, rewritten, sizeof(rewritten), &rewritten_len)) {
        relay_plain_log("[JRTEST] role=%s route_add_hop_failed ttl=%u len=%u\r\n",
            SLE_RELAY_ROLE, packet.ttl, len);
        return;
    }
    g_upstream_write.data = rewritten;
    g_upstream_write.data_len = (uint16_t)rewritten_len;
    errcode_t ret = ssapc_write_req(0, g_upstream_conn_id, &g_upstream_write);
    osal_printk("%s tx-up conn=%u len=%u ret=0x%x data=%.*s\r\n",
        SLE_RELAY_LOG, g_upstream_conn_id, (unsigned int)rewritten_len, ret,
        (int)rewritten_len, rewritten);
    relay_plain_log("[JRTEST] role=%s tx_up conn=%u len=%u ret=0x%x data=%.*s\r\n",
        SLE_RELAY_ROLE, g_upstream_conn_id, (unsigned int)rewritten_len, ret,
        (int)rewritten_len, rewritten);
}
#endif

#if SLE_RELAY_HAS_UPSTREAM && SLE_RELAY_HAS_DOWNSTREAM
static void relay_send_downstream(const uint8_t *data, uint16_t len)
{
    if (!g_downstream_connected || !sle_uart_client_is_connected()) {
        osal_printk("%s downstream not ready, drop:%.*s\r\n", SLE_RELAY_LOG, len, data);
        relay_plain_log("[JRTEST] role=%s downstream_not_ready drop=%.*s\r\n", SLE_RELAY_ROLE, len, data);
        return;
    }
    errcode_t ret = sle_uart_server_send_report_by_handle(data, len);
    osal_printk("%s tx-down conn=%u len=%u ret=0x%x data=%.*s\r\n",
        SLE_RELAY_LOG, g_downstream_conn_id, len, ret, len, data);
    relay_plain_log("[JRTEST] role=%s tx_down conn=%u len=%u ret=0x%x data=%.*s\r\n",
        SLE_RELAY_ROLE, g_downstream_conn_id, len, ret, len, data);
}
#endif

#if SLE_RELAY_HAS_UPSTREAM
static void relay_format_packet(char *out, uint16_t out_len, uint8_t dst, const char *body)
{
    (void)snprintf_s(out, out_len, out_len - 1,
        "JR|src=%u|dst=%u|ttl=4|seq=%lu|hops=0|route_dm=0|route_ok=1|last_rssi=127|%s",
        SLE_RELAY_NODE_ID, dst, (unsigned long)g_local_seq++, body);
}
#endif

#if SLE_RELAY_HAS_DOWNSTREAM
static void ssaps_read_cb(uint8_t server_id, uint16_t conn_id, ssaps_req_read_cb_t *read_cb_para, errcode_t status)
{
    unused(server_id);
    unused(conn_id);
    unused(read_cb_para);
    unused(status);
}

static void ssaps_write_cb(uint8_t server_id, uint16_t conn_id, ssaps_req_write_cb_t *write_cb_para, errcode_t status)
{
    unused(server_id);
    unused(status);
    if (write_cb_para == NULL || write_cb_para->value == NULL || write_cb_para->length == 0) {
        return;
    }

    osal_printk("%s rx-down conn=%u len=%u data=%.*s\r\n",
        SLE_RELAY_LOG, conn_id, write_cb_para->length, write_cb_para->length, write_cb_para->value);
    relay_plain_log("[JRTEST] role=%s rx_down conn=%u len=%u data=%.*s\r\n",
        SLE_RELAY_ROLE, conn_id, write_cb_para->length, write_cb_para->length, write_cb_para->value);

#if defined(CONFIG_SAMPLE_SUPPORT_SLE_RELAY_ROOT)
    ground_ui_ingest_report(write_cb_para->value, write_cb_para->length);
    relay_plain_log("[GROUND_RX] %.*s\r\n", write_cb_para->length, write_cb_para->value);
#elif defined(CONFIG_SAMPLE_SUPPORT_SLE_RELAY_NODE)
    relay_plain_log("[SKY_FORWARD] %.*s\r\n", write_cb_para->length, write_cb_para->value);
    relay_send_upstream(write_cb_para->value, write_cb_para->length);
#elif defined(CONFIG_SAMPLE_SUPPORT_SLE_RELAY_VICTIM)
    relay_plain_log("[VICTIM_RELAY] role=%s %.*s\r\n", SLE_RELAY_ROLE,
                    write_cb_para->length, write_cb_para->value);
    relay_send_upstream(write_cb_para->value, write_cb_para->length);
#elif defined(CONFIG_SAMPLE_SUPPORT_SLE_RELAY_MESH)
    (void)relay_mesh_forward_or_ack_from_downstream(write_cb_para->value, write_cb_para->length);
#else
    unused(conn_id);
#endif
}
#endif

#if SLE_RELAY_HAS_UPSTREAM
static void relay_start_scan(void)
{
#if defined(CONFIG_SAMPLE_SUPPORT_SLE_RELAY_MESH)
    g_mesh_best_parent_id = SLE_RELAY_INVALID_NODE_ID;
    (void)memset_s(&g_mesh_best_parent_addr, sizeof(g_mesh_best_parent_addr), 0, sizeof(g_mesh_best_parent_addr));
    g_mesh_scan_started = 1;
#endif
    sle_seek_param_t param = {0};
    param.own_addr_type = 0;
    param.filter_duplicates = 0;
    param.seek_filter_policy = 0;
    param.seek_phys = 1;
    param.seek_type[0] = 1;
    param.seek_interval[0] = SLE_RELAY_SCAN_INTERVAL;
    param.seek_window[0] = SLE_RELAY_SCAN_WINDOW;
    (void)sle_set_seek_param(&param);
    (void)sle_start_seek();
    osal_printk("%s scan target=%s\r\n", SLE_RELAY_LOG, SLE_RELAY_TARGET_NAME);
    relay_plain_log("[JRTEST] role=%s scan target=%s\r\n", SLE_RELAY_ROLE, SLE_RELAY_TARGET_NAME);
}

static void sle_enable_cb(errcode_t status)
{
    osal_printk("%s sle enable status=0x%x role=%s\r\n", SLE_RELAY_LOG, status, SLE_RELAY_ROLE);
    relay_plain_log("[JRTEST] role=%s sle_enable status=0x%x\r\n", SLE_RELAY_ROLE, status);
    relay_start_scan();
}

static void seek_result_cb(sle_seek_result_info_t *result)
{
    if (result == NULL || result->data == NULL || g_upstream_connected || g_client_connecting) {
        return;
    }
#if defined(CONFIG_SAMPLE_SUPPORT_SLE_RELAY_MESH)
    uint8_t peer_id = 0;
    if (!relay_mesh_parse_peer_id(result->data, result->data_length, &peer_id) || peer_id == g_node_id) {
        return;
    }
    osal_printk("%s mesh scan peer=%u len=%u\r\n", SLE_RELAY_LOG, peer_id, result->data_length);
    relay_mesh_choose_parent(result, peer_id);
#else
    osal_printk("%s scan data=%.*s\r\n", SLE_RELAY_LOG, result->data_length, result->data);
    if (!relay_data_contains(result->data, result->data_length, SLE_RELAY_TARGET_NAME)) {
        return;
    }
    relay_plain_log("[JRTEST] role=%s found target=%s\r\n", SLE_RELAY_ROLE, SLE_RELAY_TARGET_NAME);
    (void)memcpy_s(&g_upstream_addr, sizeof(g_upstream_addr), &result->addr, sizeof(result->addr));
    g_client_connecting = 1;
    (void)sle_stop_seek();
#endif
}

static void seek_disable_cb(errcode_t status)
{
    if (status != ERRCODE_SUCC) {
        osal_printk("%s seek stop failed=0x%x\r\n", SLE_RELAY_LOG, status);
        relay_plain_log("[JRTEST] role=%s seek_stop_failed status=0x%x\r\n", SLE_RELAY_ROLE, status);
        g_client_connecting = 0;
        return;
    }
#if defined(CONFIG_SAMPLE_SUPPORT_SLE_RELAY_MESH)
    g_mesh_scan_started = 0;
    if (g_mesh_best_parent_id == SLE_RELAY_INVALID_NODE_ID || g_upstream_connected) {
        relay_plain_log("[JRTEST] role=%s no_parent self_root id=%u\r\n", SLE_RELAY_ROLE, g_node_id);
        return;
    }
    (void)memcpy_s(&g_upstream_addr, sizeof(g_upstream_addr), &g_mesh_best_parent_addr, sizeof(g_mesh_best_parent_addr));
    g_client_connecting = 1;
#endif
    (void)sle_remove_paired_remote_device(&g_upstream_addr);
    errcode_t ret = sle_connect_remote_device(&g_upstream_addr);
    osal_printk("%s connect target=%s ret=0x%x\r\n", SLE_RELAY_LOG, SLE_RELAY_TARGET_NAME, ret);
    relay_plain_log("[JRTEST] role=%s connect target=%s ret=0x%x\r\n",
        SLE_RELAY_ROLE, SLE_RELAY_TARGET_NAME, ret);
    if (ret != ERRCODE_SUCC) {
        g_client_connecting = 0;
        relay_start_scan();
    }
}

static void register_seek_callbacks(void)
{
    g_seek_cbks.sle_enable_cb = sle_enable_cb;
    g_seek_cbks.seek_result_cb = seek_result_cb;
    g_seek_cbks.seek_disable_cb = seek_disable_cb;
    (void)sle_announce_seek_register_callbacks(&g_seek_cbks);
}
#endif

static void connect_state_cb(uint16_t conn_id, const sle_addr_t *addr, sle_acb_state_t conn_state,
    sle_pair_state_t pair_state, sle_disc_reason_t disc_reason)
{
#if SLE_RELAY_HAS_UPSTREAM
    bool is_upstream = addr != NULL &&
        memcmp(addr->addr, g_upstream_addr.addr, SLE_ADDR_LEN) == 0;
#else
    unused(addr);
#endif
    osal_printk("%s conn state conn=%u state=%u pair=%u reason=%u connecting=%u\r\n",
        SLE_RELAY_LOG, conn_id, conn_state, pair_state, disc_reason,
#if SLE_RELAY_HAS_UPSTREAM
        g_client_connecting);
#else
        0);
#endif
    relay_plain_log("[JRTEST] role=%s conn=%u state=%u pair=%u reason=%u\r\n",
        SLE_RELAY_ROLE, conn_id, conn_state, pair_state, disc_reason);

    if (conn_state == SLE_ACB_STATE_CONNECTED) {
#if SLE_RELAY_HAS_UPSTREAM
        if (is_upstream) {
            g_upstream_conn_id = conn_id;
            g_upstream_connected = 1;
            g_client_connecting = 0;
            if (pair_state == SLE_PAIR_NONE) {
                errcode_t pair_ret = sle_pair_remote_device(&g_upstream_addr);
                if (pair_ret != ERRCODE_SUCC) {
                    relay_plain_log("[JRTEST] role=%s pair_start_failed status=0x%x\r\n",
                        SLE_RELAY_ROLE, pair_ret);
                    (void)sle_disconnect_remote_device(&g_upstream_addr);
                }
            }
            return;
        }
#endif
#if SLE_RELAY_HAS_DOWNSTREAM
        {
            g_downstream_conn_id = conn_id;
            g_downstream_connected = 1;
            sle_uart_server_relay_set_connected(conn_id);
        }
#endif
    } else if (conn_state == SLE_ACB_STATE_DISCONNECTED) {
#if SLE_RELAY_HAS_UPSTREAM
        if (is_upstream || conn_id == g_upstream_conn_id) {
            if (g_upstream_rssi_mutex_ready) {
                osal_mutex_lock(&g_upstream_rssi_mutex);
                g_upstream_rssi.filtered = 127;
                g_upstream_rssi.valid = 0;
                g_upstream_rssi.updated_ms = 0;
                osal_mutex_unlock(&g_upstream_rssi_mutex);
            }
            g_upstream_connected = 0;
            g_upstream_ready = 0;
            g_upstream_conn_id = SLE_RELAY_INVALID_CONN_ID;
            g_client_connecting = 0;
            relay_start_scan();
        }
#endif
#if SLE_RELAY_HAS_DOWNSTREAM
        if (conn_id == g_downstream_conn_id) {
            g_downstream_connected = 0;
            g_downstream_conn_id = 0;
            sle_uart_server_relay_set_disconnected();
            (void)sle_start_announce(1);
        }
#endif
    }
}

#if SLE_RELAY_HAS_UPSTREAM
static void relay_read_rssi_cb(uint16_t conn_id, int8_t rssi, errcode_t status)
{
    if (!g_upstream_rssi_mutex_ready || conn_id != g_upstream_conn_id ||
        status != ERRCODE_SUCC || rssi == 127 || rssi < -127 || rssi > 20) {
        return;
    }
    osal_mutex_lock(&g_upstream_rssi_mutex);
    if (g_upstream_rssi.valid) {
        g_upstream_rssi.filtered = (int16_t)((3 * g_upstream_rssi.filtered + rssi) / 4);
    } else {
        g_upstream_rssi.filtered = rssi;
    }
    g_upstream_rssi.valid = 1;
    g_upstream_rssi.updated_ms = uapi_systick_get_ms();
    osal_mutex_unlock(&g_upstream_rssi_mutex);
}

static void *relay_rssi_task(const char *arg)
{
    unused(arg);
    while (1) {
        if (g_upstream_connected && g_upstream_conn_id != SLE_RELAY_INVALID_CONN_ID) {
            (void)sle_read_remote_device_rssi(g_upstream_conn_id);
        }
        osal_msleep(SLE_RELAY_RSSI_POLL_MS);
    }
    return NULL;
}

static void relay_start_rssi_task(void)
{
    osal_task *task;

    if (g_upstream_rssi_task_started) {
        return;
    }
    task = osal_kthread_create((osal_kthread_handler)relay_rssi_task, 0,
        "RelayRssiTask", SLE_RELAY_RSSI_TASK_STACK_SIZE);
    if (task == NULL) {
        relay_plain_log("[JRTEST] role=%s rssi_task_create_failed\r\n", SLE_RELAY_ROLE);
        return;
    }
    osal_kthread_set_priority(task, SLE_RELAY_RSSI_TASK_PRIO);
    g_upstream_rssi_task_started = 1;
}
#endif

static void pair_complete_cb(uint16_t conn_id, const sle_addr_t *addr, errcode_t status)
{
    unused(addr);
    osal_printk("%s pair complete conn=%u status=0x%x\r\n", SLE_RELAY_LOG, conn_id, status);
    relay_plain_log("[JRTEST] role=%s pair_complete conn=%u status=0x%x\r\n", SLE_RELAY_ROLE, conn_id, status);
    if (status != ERRCODE_SUCC) {
#if SLE_RELAY_HAS_UPSTREAM
        if (conn_id == g_upstream_conn_id) {
            (void)sle_disconnect_remote_device(&g_upstream_addr);
        }
#endif
        return;
    }
#if SLE_RELAY_HAS_UPSTREAM
    if (g_upstream_connected && conn_id == g_upstream_conn_id) {
        ssap_exchange_info_t info = {0};
        info.mtu_size = SLE_RELAY_MTU;
        info.version = 1;
        (void)ssapc_exchange_info_req(0, g_upstream_conn_id, &info);
        return;
    }
#endif
#if SLE_RELAY_HAS_DOWNSTREAM
    {
        sle_uart_server_relay_set_paired(conn_id);
    }
#endif
}

static void register_connection_callbacks(void)
{
    g_conn_cbks.connect_state_changed_cb = connect_state_cb;
    g_conn_cbks.pair_complete_cb = pair_complete_cb;
#if SLE_RELAY_HAS_UPSTREAM
    g_conn_cbks.read_rssi_cb = relay_read_rssi_cb;
#endif
    (void)sle_connection_register_callbacks(&g_conn_cbks);
}

#if SLE_RELAY_HAS_UPSTREAM
static void exchange_info_cb(uint8_t client_id, uint16_t conn_id, ssap_exchange_info_t *param, errcode_t status)
{
    unused(client_id);
    osal_printk("%s exchange conn=%u mtu=%u status=0x%x\r\n",
        SLE_RELAY_LOG, conn_id, param == NULL ? 0 : param->mtu_size, status);
    relay_plain_log("[JRTEST] role=%s exchange conn=%u mtu=%u status=0x%x\r\n",
        SLE_RELAY_ROLE, conn_id, param == NULL ? 0 : param->mtu_size, status);
    ssapc_find_structure_param_t find_param = {0};
    find_param.type = SSAP_FIND_TYPE_PROPERTY;
    find_param.start_hdl = 1;
    find_param.end_hdl = 0xFFFF;
    (void)ssapc_find_structure(0, conn_id, &find_param);
}

static void find_property_cb(uint8_t client_id, uint16_t conn_id, ssapc_find_property_result_t *property,
    errcode_t status)
{
    unused(client_id);
    if (property == NULL || status != ERRCODE_SUCC) {
        osal_printk("%s find property failed status=0x%x\r\n", SLE_RELAY_LOG, status);
        return;
    }
    g_upstream_write.handle = property->handle;
    g_upstream_write.type = SSAP_PROPERTY_TYPE_VALUE;
    g_upstream_ready = 1;
    osal_printk("%s upstream ready conn=%u handle=%u\r\n", SLE_RELAY_LOG, conn_id, property->handle);
    relay_plain_log("[JRTEST] role=%s upstream_ready conn=%u handle=%u\r\n",
        SLE_RELAY_ROLE, conn_id, property->handle);
}

static void notification_cb(uint8_t client_id, uint16_t conn_id, ssapc_handle_value_t *data, errcode_t status)
{
    unused(client_id);
    unused(status);
    if (data == NULL || data->data == NULL || data->data_len == 0) {
        return;
    }
    osal_printk("%s rx-up conn=%u len=%u data=%.*s\r\n",
        SLE_RELAY_LOG, conn_id, data->data_len, data->data_len, data->data);
    relay_plain_log("[JRTEST] role=%s rx_up conn=%u len=%u data=%.*s\r\n",
        SLE_RELAY_ROLE, conn_id, data->data_len, data->data_len, data->data);
#if defined(CONFIG_SAMPLE_SUPPORT_SLE_RELAY_NODE) || \
    (defined(CONFIG_SAMPLE_SUPPORT_SLE_RELAY_VICTIM) && CONFIG_SLE_RELAY_VICTIM_INDEX < 3) || \
    defined(CONFIG_SAMPLE_SUPPORT_SLE_RELAY_MESH)
    relay_send_downstream(data->data, data->data_len);
#else
    unused(conn_id);
#endif
}

static void register_ssapc_callbacks(void)
{
    g_ssapc_cbks.exchange_info_cb = exchange_info_cb;
    g_ssapc_cbks.ssapc_find_property_cbk = find_property_cb;
    g_ssapc_cbks.notification_cb = notification_cb;
    g_ssapc_cbks.indication_cb = notification_cb;
    (void)ssapc_register_callbacks(&g_ssapc_cbks);
}

static void relay_upstream_start(bool stack_enabled)
{
    relay_start_rssi_task();
    register_seek_callbacks();
    register_connection_callbacks();
    register_ssapc_callbacks();
    if (stack_enabled) {
        relay_start_scan();
    } else if (enable_sle() != ERRCODE_SUCC) {
        osal_printk("%s enable_sle failed\r\n", SLE_RELAY_LOG);
        relay_plain_log("[JRTEST] role=%s enable_sle_failed\r\n", SLE_RELAY_ROLE);
    }
}
#endif

static void *relay_task(const char *arg)
{
    unused(arg);
#if SLE_RELAY_HAS_UPSTREAM
    if (osal_mutex_init(&g_upstream_rssi_mutex) == OSAL_SUCCESS) {
        g_upstream_rssi_mutex_ready = 1;
    }
#endif
#if defined(CONFIG_SAMPLE_SUPPORT_SLE_RELAY_VICTIM)
    rescue_victim_start_sensor_task();
#endif
#if defined(CONFIG_SAMPLE_SUPPORT_SLE_RELAY_MESH)
    relay_mesh_init_identity();
#endif
    osal_printk("%s boot role=%s id=%u\r\n", SLE_RELAY_LOG, SLE_RELAY_ROLE, SLE_RELAY_NODE_ID);
    relay_plain_log("[JRTEST] boot role=%s id=%u\r\n", SLE_RELAY_ROLE, SLE_RELAY_NODE_ID);

#if SLE_RELAY_HAS_DOWNSTREAM
#if defined(CONFIG_SAMPLE_SUPPORT_SLE_RELAY_ROOT)
    int ground_init_ret = ground_ui_init();
    relay_plain_log("[GROUND_UI] init=%d\r\n", ground_init_ret);
#endif
    errcode_t ret = sle_uart_server_init(ssaps_read_cb, ssaps_write_cb);
    osal_printk("%s server init ret=0x%x\r\n", SLE_RELAY_LOG, ret);
    relay_plain_log("[JRTEST] role=%s server_init ret=0x%x\r\n", SLE_RELAY_ROLE, ret);
    register_connection_callbacks();
#if defined(CONFIG_SAMPLE_SUPPORT_SLE_RELAY_ROOT)
    int ground_start_ret = ground_ui_start();
    relay_plain_log("[GROUND_UI] start=%d\r\n", ground_start_ret);
#endif
    osal_msleep(1000);
#endif

#if defined(CONFIG_SAMPLE_SUPPORT_SLE_RELAY_NODE)
    relay_upstream_start(true);
#elif defined(CONFIG_SAMPLE_SUPPORT_SLE_RELAY_VICTIM)
#if CONFIG_SLE_RELAY_VICTIM_INDEX < 3
    relay_upstream_start(true);
#else
    relay_upstream_start(false);
#endif
#elif defined(CONFIG_SAMPLE_SUPPORT_SLE_RELAY_LEAF)
    relay_upstream_start(false);
#elif defined(CONFIG_SAMPLE_SUPPORT_SLE_RELAY_MESH)
    relay_upstream_start(true);
    osal_msleep(3000);
    if (g_mesh_scan_started && !g_upstream_connected && !g_client_connecting) {
        (void)sle_stop_seek();
    }
#endif

    while (1) {
#if defined(CONFIG_SAMPLE_SUPPORT_SLE_RELAY_LEAF) || defined(CONFIG_SAMPLE_SUPPORT_SLE_RELAY_NODE) || \
    defined(CONFIG_SAMPLE_SUPPORT_SLE_RELAY_MESH)
        char packet[160] = {0};
#endif
#if defined(CONFIG_SAMPLE_SUPPORT_SLE_RELAY_LEAF)
        relay_format_packet(packet, sizeof(packet), 1, "body=leaf_heartbeat");
        relay_send_upstream((const uint8_t *)packet, (uint16_t)strlen(packet));
#elif defined(CONFIG_SAMPLE_SUPPORT_SLE_RELAY_NODE)
        relay_format_packet(packet, sizeof(packet), 1, "body=relay_local");
        relay_send_upstream((const uint8_t *)packet, (uint16_t)strlen(packet));
#elif defined(CONFIG_SAMPLE_SUPPORT_SLE_RELAY_VICTIM)
        if (g_upstream_ready) {
            rescue_victim_send_state();
        } else {
            relay_plain_log("[VICTIM_WAIT] role=%s parent=%s\r\n", SLE_RELAY_ROLE, SLE_RELAY_TARGET_NAME);
        }
#elif defined(CONFIG_SAMPLE_SUPPORT_SLE_RELAY_MESH)
        if (g_upstream_ready) {
            relay_format_packet(packet, sizeof(packet), SLE_RELAY_MESH_ROOT_DST, "body=mesh_heartbeat");
            relay_send_upstream((const uint8_t *)packet, (uint16_t)strlen(packet));
        } else {
            osal_printk("%s mesh root waiting downstream=%u\r\n", SLE_RELAY_LOG, g_downstream_connected);
        }
#elif defined(CONFIG_SAMPLE_SUPPORT_SLE_RELAY_ROOT)
        osal_printk("%s root waiting downstream=%u\r\n", SLE_RELAY_LOG, g_downstream_connected);
#endif
#if defined(CONFIG_SAMPLE_SUPPORT_SLE_RELAY_VICTIM)
        osal_msleep(RESCUE_VICTIM_REPORT_PERIOD_MS);
#else
        osal_msleep(SLE_RELAY_PERIOD_MS);
#endif
    }
    return NULL;
}

static void sle_relay_entry(void)
{
    osal_task *task = NULL;
    osal_kthread_lock();
    task = osal_kthread_create((osal_kthread_handler)relay_task, 0, "SleRelayTask", SLE_RELAY_TASK_STACK_SIZE);
    if (task != NULL) {
        osal_kthread_set_priority(task, SLE_RELAY_TASK_PRIO);
    }
    osal_kthread_unlock();
}

app_run(sle_relay_entry);

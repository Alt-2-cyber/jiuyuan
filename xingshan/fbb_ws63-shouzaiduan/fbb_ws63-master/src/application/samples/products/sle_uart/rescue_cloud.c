#if defined(__has_include)
#if __has_include("rescue_cloud.local.h")
#include "rescue_cloud.local.h"
#endif
#endif

#include "rescue_cloud.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "lwip/netifapi.h"
#include "MQTTClient.h"
#include "securec.h"
#include "soc_osal.h"
#include "td_base.h"
#include "td_type.h"
#include "wifi_hotspot.h"
#include "wifi_hotspot_config.h"

#ifndef RESCUE_CLOUD_WIFI_SSID
#define RESCUE_CLOUD_WIFI_SSID "YOUR_WIFI_SSID"
#endif
#ifndef RESCUE_CLOUD_WIFI_PASSWORD
#define RESCUE_CLOUD_WIFI_PASSWORD "YOUR_WIFI_PASSWORD"
#endif

#ifndef RESCUE_CLOUD_MQTT_ADDRESS
#define RESCUE_CLOUD_MQTT_ADDRESS "tcp://YOUR_IOTDA_HOST:1883"
#endif
#ifndef RESCUE_CLOUD_DEVICE_ID
#define RESCUE_CLOUD_DEVICE_ID "YOUR_DEVICE_ID"
#endif
#ifndef RESCUE_CLOUD_CLIENT_ID
#define RESCUE_CLOUD_CLIENT_ID "YOUR_MQTT_CLIENT_ID"
#endif
#ifndef RESCUE_CLOUD_USERNAME
#define RESCUE_CLOUD_USERNAME "YOUR_MQTT_USERNAME"
#endif
#ifndef RESCUE_CLOUD_PASSWORD
#define RESCUE_CLOUD_PASSWORD "YOUR_MQTT_PASSWORD"
#endif
#define RESCUE_CLOUD_SERVICE_ID             "environment"

#define RESCUE_CLOUD_IFNAME_MAX_SIZE        16
#define RESCUE_CLOUD_MAX_SSID_LEN           33
#define RESCUE_CLOUD_SCAN_AP_LIMIT          64
#define RESCUE_CLOUD_MAC_LEN                6
#define RESCUE_CLOUD_GET_IP_MAX_COUNT       300
#define RESCUE_CLOUD_TASK_STACK_SIZE        0x3000
#define RESCUE_CLOUD_TASK_PRIO              24
#define RESCUE_CLOUD_RETRY_MS               5000
#define RESCUE_CLOUD_HEARTBEAT_MS           30000
#define RESCUE_CLOUD_STATE_PUBLISH_MS       5000
#define RESCUE_CLOUD_LOOP_MS                1000
#define RESCUE_CLOUD_QOS                    0
#define RESCUE_CLOUD_PUB_TIMEOUT_MS         3000L
#define RESCUE_CLOUD_TOPIC_MAX_LEN          160
#define RESCUE_CLOUD_CONTENT_MAX_LEN        384
#define RESCUE_CLOUD_PAYLOAD_MAX_LEN        640
#define RESCUE_CLOUD_ROLE_MAX_LEN           16
#define RESCUE_CLOUD_LOG                    "[RESCUE_CLOUD]"

enum {
    RESCUE_WIFI_INIT = 0,
    RESCUE_WIFI_SCANING,
    RESCUE_WIFI_SCAN_DONE,
    RESCUE_WIFI_FOUND_TARGET,
    RESCUE_WIFI_CONNECTING,
    RESCUE_WIFI_CONNECT_DONE,
    RESCUE_WIFI_GET_IP,
};

static uint8_t g_rescue_wifi_state = RESCUE_WIFI_INIT;
static uint8_t g_rescue_wifi_ready = 0;
static uint8_t g_rescue_cloud_started = 0;
static uint8_t g_rescue_cloud_mutex_ready = 0;
static uint8_t g_rescue_cloud_mqtt_inited = 0;
static uint8_t g_rescue_cloud_pending_victim = 0;
static uint8_t g_rescue_cloud_pending_sle = 0;
static uint32_t g_rescue_cloud_msg_seq = 0;
static uint32_t g_rescue_cloud_last_heartbeat_ms = 0;
static uint32_t g_rescue_cloud_last_state_ms = 0;
static char g_rescue_cloud_role[RESCUE_CLOUD_ROLE_MAX_LEN] = "node";
static char g_rescue_cloud_sle_text[RESCUE_CLOUD_CONTENT_MAX_LEN] = {0};
static uint16_t g_rescue_cloud_sle_len = 0;
static uint32_t g_rescue_cloud_victim_seq = 0;
static int g_rescue_cloud_victim_spo2 = 0;
static int g_rescue_cloud_victim_hr = 0;
static int g_rescue_cloud_victim_sbp = 0;
static int g_rescue_cloud_victim_battery = 0;
static int g_rescue_cloud_victim_sos = 0;
static osal_mutex g_rescue_cloud_mutex;
static MQTTClient g_rescue_cloud_client = NULL;

static void rescue_cloud_mqtt_global_init(void)
{
    if (g_rescue_cloud_mqtt_inited) {
        return;
    }

    MQTTClient_init_options init_options = MQTTClient_init_options_initializer;
    MQTTClient_global_init(&init_options);
    g_rescue_cloud_mqtt_inited = 1;
    osal_printk("%s mqtt global init done.\r\n", RESCUE_CLOUD_LOG);
}

static void rescue_wifi_scan_state_changed(td_s32 state, td_s32 size)
{
    (void)state;
    (void)size;
    osal_printk("%s wifi scan done.\r\n", RESCUE_CLOUD_LOG);
    g_rescue_wifi_state = RESCUE_WIFI_SCAN_DONE;
}

static void rescue_wifi_connection_changed(td_s32 state, const wifi_linked_info_stru *info, td_s32 reason_code)
{
    (void)info;
    (void)reason_code;

    if (state == 0) {
        osal_printk("%s wifi connect failed, retry.\r\n", RESCUE_CLOUD_LOG);
        g_rescue_wifi_state = RESCUE_WIFI_INIT;
    } else {
        osal_printk("%s wifi connected.\r\n", RESCUE_CLOUD_LOG);
        g_rescue_wifi_state = RESCUE_WIFI_CONNECT_DONE;
    }
}

static wifi_event_stru g_rescue_wifi_event_cb = {
    .wifi_event_connection_changed = rescue_wifi_connection_changed,
    .wifi_event_scan_state_changed = rescue_wifi_scan_state_changed,
};

static td_s32 rescue_wifi_get_match_network(wifi_sta_config_stru *expected_bss)
{
    td_u32 num = RESCUE_CLOUD_SCAN_AP_LIMIT;
    td_bool find_ap = TD_FALSE;
    td_u8 bss_index;
    td_u32 scan_len = sizeof(wifi_scan_info_stru) * RESCUE_CLOUD_SCAN_AP_LIMIT;
    wifi_scan_info_stru *result = osal_kmalloc(scan_len, OSAL_GFP_ATOMIC);
    if (result == TD_NULL) {
        return -1;
    }

    (void)memset_s(result, scan_len, 0, scan_len);
    if (wifi_sta_get_scan_info(result, &num) != 0) {
        osal_kfree(result);
        return -1;
    }

    for (bss_index = 0; bss_index < num; bss_index++) {
        if ((strlen(RESCUE_CLOUD_WIFI_SSID) == strlen(result[bss_index].ssid)) &&
            (memcmp(RESCUE_CLOUD_WIFI_SSID, result[bss_index].ssid, strlen(RESCUE_CLOUD_WIFI_SSID)) == 0)) {
            find_ap = TD_TRUE;
            break;
        }
    }

    if (find_ap == TD_FALSE) {
        osal_kfree(result);
        return -1;
    }

    if (memcpy_s(expected_bss->ssid, RESCUE_CLOUD_MAX_SSID_LEN,
        RESCUE_CLOUD_WIFI_SSID, strlen(RESCUE_CLOUD_WIFI_SSID)) != EOK) {
        osal_kfree(result);
        return -1;
    }
    if (memcpy_s(expected_bss->bssid, RESCUE_CLOUD_MAC_LEN, result[bss_index].bssid, RESCUE_CLOUD_MAC_LEN) != EOK) {
        osal_kfree(result);
        return -1;
    }
    expected_bss->security_type = result[bss_index].security_type;
    if (memcpy_s(expected_bss->pre_shared_key, sizeof(expected_bss->pre_shared_key),
        RESCUE_CLOUD_WIFI_PASSWORD, strlen(RESCUE_CLOUD_WIFI_PASSWORD)) != EOK) {
        osal_kfree(result);
        return -1;
    }
    expected_bss->ip_type = 1;
    osal_kfree(result);
    return 0;
}

static td_bool rescue_wifi_check_dhcp_status(struct netif *netif_p, td_u32 *wait_count)
{
    if ((ip_addr_isany(&(netif_p->ip_addr)) == 0) && (*wait_count <= RESCUE_CLOUD_GET_IP_MAX_COUNT)) {
        osal_printk("%s wifi DHCP success.\r\n", RESCUE_CLOUD_LOG);
        return 0;
    }

    if (*wait_count > RESCUE_CLOUD_GET_IP_MAX_COUNT) {
        osal_printk("%s wifi DHCP timeout, retry.\r\n", RESCUE_CLOUD_LOG);
        *wait_count = 0;
        g_rescue_wifi_state = RESCUE_WIFI_INIT;
    }
    return -1;
}

static td_s32 rescue_wifi_connect(void)
{
    td_char ifname[RESCUE_CLOUD_IFNAME_MAX_SIZE + 1] = "wlan0";
    wifi_sta_config_stru expected_bss = {0};
    struct netif *netif_p = TD_NULL;
    td_u32 wait_count = 0;

    if (wifi_register_event_cb(&g_rescue_wifi_event_cb) != 0) {
        osal_printk("%s wifi event register failed.\r\n", RESCUE_CLOUD_LOG);
        return -1;
    }

    while (wifi_is_wifi_inited() == 0) {
        osal_msleep(100);
    }

    if (wifi_sta_enable() != 0) {
        osal_printk("%s wifi sta enable failed.\r\n", RESCUE_CLOUD_LOG);
        return -1;
    }

    g_rescue_wifi_state = RESCUE_WIFI_INIT;
    while (1) {
        osal_msleep(10);
        if (g_rescue_wifi_state == RESCUE_WIFI_INIT) {
            osal_printk("%s wifi scan start.\r\n", RESCUE_CLOUD_LOG);
            g_rescue_wifi_state = RESCUE_WIFI_SCANING;
            if (wifi_sta_scan() != 0) {
                g_rescue_wifi_state = RESCUE_WIFI_INIT;
            }
        } else if (g_rescue_wifi_state == RESCUE_WIFI_SCAN_DONE) {
            if (rescue_wifi_get_match_network(&expected_bss) != 0) {
                osal_printk("%s wifi AP not found, retry.\r\n", RESCUE_CLOUD_LOG);
                g_rescue_wifi_state = RESCUE_WIFI_INIT;
                continue;
            }
            g_rescue_wifi_state = RESCUE_WIFI_FOUND_TARGET;
        } else if (g_rescue_wifi_state == RESCUE_WIFI_FOUND_TARGET) {
            osal_printk("%s wifi connect start.\r\n", RESCUE_CLOUD_LOG);
            g_rescue_wifi_state = RESCUE_WIFI_CONNECTING;
            if (wifi_sta_connect(&expected_bss) != 0) {
                g_rescue_wifi_state = RESCUE_WIFI_INIT;
            }
        } else if (g_rescue_wifi_state == RESCUE_WIFI_CONNECT_DONE) {
            osal_printk("%s wifi DHCP start.\r\n", RESCUE_CLOUD_LOG);
            g_rescue_wifi_state = RESCUE_WIFI_GET_IP;
            netif_p = netifapi_netif_find(ifname);
            if (netif_p == TD_NULL || netifapi_dhcp_start(netif_p) != 0) {
                osal_printk("%s wifi DHCP start failed.\r\n", RESCUE_CLOUD_LOG);
                g_rescue_wifi_state = RESCUE_WIFI_INIT;
            }
        } else if (g_rescue_wifi_state == RESCUE_WIFI_GET_IP) {
            if (rescue_wifi_check_dhcp_status(netif_p, &wait_count) == 0) {
                return 0;
            }
            wait_count++;
        }
    }
}

static int rescue_cloud_make_message_topic(char *topic, size_t topic_len)
{
    if (topic == NULL || topic_len == 0) {
        return -1;
    }
    int ret = snprintf_s(topic, topic_len, topic_len - 1,
        "$oc/devices/%s/sys/messages/up", RESCUE_CLOUD_DEVICE_ID);
    return (ret > 0) ? 0 : -1;
}

static int rescue_cloud_make_property_topic(char *topic, size_t topic_len)
{
    if (topic == NULL || topic_len == 0) {
        return -1;
    }
    int ret = snprintf_s(topic, topic_len, topic_len - 1,
        "$oc/devices/%s/sys/properties/report", RESCUE_CLOUD_DEVICE_ID);
    return (ret > 0) ? 0 : -1;
}

static int rescue_cloud_connect_locked(void)
{
    if (g_rescue_cloud_client != NULL && MQTTClient_isConnected(g_rescue_cloud_client)) {
        return 0;
    }
    if (g_rescue_cloud_client != NULL) {
        MQTTClient_destroy(&g_rescue_cloud_client);
        g_rescue_cloud_client = NULL;
    }

    int rc = MQTTClient_create(&g_rescue_cloud_client, RESCUE_CLOUD_MQTT_ADDRESS, RESCUE_CLOUD_CLIENT_ID,
        MQTTCLIENT_PERSISTENCE_NONE, NULL);
    if (rc != MQTTCLIENT_SUCCESS) {
        osal_printk("%s mqtt create failed:%d %s\r\n", RESCUE_CLOUD_LOG, rc, MQTTClient_strerror(rc));
        g_rescue_cloud_client = NULL;
        return -1;
    }

    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
    conn_opts.keepAliveInterval = 120;
    conn_opts.cleansession = 1;
    conn_opts.MQTTVersion = MQTTVERSION_3_1_1;
    conn_opts.username = RESCUE_CLOUD_USERNAME;
    conn_opts.password = RESCUE_CLOUD_PASSWORD;
    conn_opts.connectTimeout = 10;

    rc = MQTTClient_connect(g_rescue_cloud_client, &conn_opts);
    if (rc != MQTTCLIENT_SUCCESS) {
        osal_printk("%s mqtt connect failed:%d %s\r\n", RESCUE_CLOUD_LOG, rc, MQTTClient_strerror(rc));
        MQTTClient_destroy(&g_rescue_cloud_client);
        g_rescue_cloud_client = NULL;
        return -1;
    }

    osal_printk("%s mqtt connected.\r\n", RESCUE_CLOUD_LOG);
    return 0;
}

static int rescue_cloud_lock_init(void)
{
    if (g_rescue_cloud_mutex_ready) {
        return 0;
    }
    if (osal_mutex_init(&g_rescue_cloud_mutex) != OSAL_SUCCESS) {
        osal_printk("%s mutex init failed.\r\n", RESCUE_CLOUD_LOG);
        return -1;
    }
    g_rescue_cloud_mutex_ready = 1;
    return 0;
}

static void rescue_cloud_escape_json(const uint8_t *src, uint16_t src_len, char *dst, size_t dst_len)
{
    size_t out = 0;
    uint16_t i;

    if (dst == NULL || dst_len == 0) {
        return;
    }
    if (src == NULL) {
        dst[0] = '\0';
        return;
    }

    for (i = 0; i < src_len && out + 2 < dst_len; i++) {
        uint8_t ch = src[i];
        if (ch == '\0') {
            break;
        }
        if (ch == '\"' || ch == '\\') {
            if (out + 2 >= dst_len) {
                break;
            }
            dst[out++] = '\\';
            dst[out++] = (char)ch;
        } else if (ch == '\r' || ch == '\n' || ch == '\t') {
            dst[out++] = ' ';
        } else if (ch < 0x20) {
            dst[out++] = ' ';
        } else {
            dst[out++] = (char)ch;
        }
    }
    dst[out] = '\0';
}

static int rescue_cloud_publish_message(const char *name, const uint8_t *content, uint16_t content_len)
{
    char topic[RESCUE_CLOUD_TOPIC_MAX_LEN] = {0};
    char escaped[RESCUE_CLOUD_CONTENT_MAX_LEN] = {0};
    char payload[RESCUE_CLOUD_PAYLOAD_MAX_LEN] = {0};
    MQTTClient_message pubmsg = MQTTClient_message_initializer;
    MQTTClient_deliveryToken token;
    int rc;

    if (!g_rescue_wifi_ready || content == NULL || content_len == 0) {
        return -1;
    }
    if (rescue_cloud_make_message_topic(topic, sizeof(topic)) != 0) {
        return -1;
    }
    rescue_cloud_escape_json(content, content_len, escaped, sizeof(escaped));
    int ret = snprintf_s(payload, sizeof(payload), sizeof(payload) - 1,
        "{\"content\":{\"device_id\":\"%s\",\"name\":\"%s\",\"id\":%u,\"payload\":\"%s\"}}",
        RESCUE_CLOUD_DEVICE_ID, name, g_rescue_cloud_msg_seq++, escaped);
    if (ret <= 0) {
        return -1;
    }

    if (rescue_cloud_lock_init() != 0) {
        return -1;
    }

    osal_mutex_lock(&g_rescue_cloud_mutex);
    if (rescue_cloud_connect_locked() != 0) {
        osal_mutex_unlock(&g_rescue_cloud_mutex);
        return -1;
    }

    pubmsg.payload = payload;
    pubmsg.payloadlen = (int)strlen(payload);
    pubmsg.qos = RESCUE_CLOUD_QOS;
    pubmsg.retained = 0;
    rc = MQTTClient_publishMessage(g_rescue_cloud_client, topic, &pubmsg, &token);
    if (rc == MQTTCLIENT_SUCCESS && RESCUE_CLOUD_QOS > 0) {
        rc = MQTTClient_waitForCompletion(g_rescue_cloud_client, token, RESCUE_CLOUD_PUB_TIMEOUT_MS);
    }
    osal_mutex_unlock(&g_rescue_cloud_mutex);

    if (rc != MQTTCLIENT_SUCCESS) {
        osal_printk("%s mqtt publish failed:%d %s\r\n", RESCUE_CLOUD_LOG, rc, MQTTClient_strerror(rc));
        return -1;
    }
    osal_printk("%s mqtt publish ok:%s\r\n", RESCUE_CLOUD_LOG, name);
    return 0;
}

static int rescue_cloud_publish_property_payload(const char *name, const char *payload)
{
    char topic[RESCUE_CLOUD_TOPIC_MAX_LEN] = {0};
    MQTTClient_message pubmsg = MQTTClient_message_initializer;
    MQTTClient_deliveryToken token;
    int rc;

    if (!g_rescue_wifi_ready || payload == NULL || payload[0] == '\0') {
        return -1;
    }
    if (rescue_cloud_make_property_topic(topic, sizeof(topic)) != 0) {
        return -1;
    }
    if (rescue_cloud_lock_init() != 0) {
        return -1;
    }

    osal_mutex_lock(&g_rescue_cloud_mutex);
    if (rescue_cloud_connect_locked() != 0) {
        osal_mutex_unlock(&g_rescue_cloud_mutex);
        return -1;
    }

    pubmsg.payload = (void *)payload;
    pubmsg.payloadlen = (int)strlen(payload);
    pubmsg.qos = RESCUE_CLOUD_QOS;
    pubmsg.retained = 0;
    rc = MQTTClient_publishMessage(g_rescue_cloud_client, topic, &pubmsg, &token);
    if (rc == MQTTCLIENT_SUCCESS && RESCUE_CLOUD_QOS > 0) {
        rc = MQTTClient_waitForCompletion(g_rescue_cloud_client, token, RESCUE_CLOUD_PUB_TIMEOUT_MS);
    }
    osal_mutex_unlock(&g_rescue_cloud_mutex);

    if (rc != MQTTCLIENT_SUCCESS) {
        osal_printk("%s mqtt property publish failed:%d %s\r\n", RESCUE_CLOUD_LOG, rc, MQTTClient_strerror(rc));
        return -1;
    }
    osal_printk("%s mqtt property publish ok:%s\r\n", RESCUE_CLOUD_LOG, name);
    return 0;
}

static bool rescue_cloud_parse_int_field(const char *text, const char *key, int *value)
{
    char pattern[32] = {0};
    const char *pos = NULL;
    int sign = 1;
    int number = 0;
    bool has_digit = false;

    if (text == NULL || key == NULL || value == NULL) {
        return false;
    }
    if (snprintf_s(pattern, sizeof(pattern), sizeof(pattern) - 1, "\"%s\":", key) <= 0) {
        return false;
    }
    pos = strstr(text, pattern);
    if (pos == NULL) {
        return false;
    }
    pos += strlen(pattern);
    while (*pos == ' ' || *pos == '\"') {
        pos++;
    }
    if (*pos == '-') {
        sign = -1;
        pos++;
    }
    while (*pos >= '0' && *pos <= '9') {
        has_digit = true;
        number = number * 10 + (*pos - '0');
        pos++;
    }
    if (!has_digit) {
        return false;
    }
    *value = number * sign;
    return true;
}

int rescue_cloud_publish_sle_payload(const uint8_t *payload, uint16_t payload_len)
{
    char text[RESCUE_CLOUD_CONTENT_MAX_LEN] = {0};
    int seq = 0;
    int spo2 = 0;
    int heart_rate = 0;
    int systolic = 0;
    int battery = 0;
    int sos = 1;
    size_t copy_len;

    if (rescue_cloud_lock_init() != 0) {
        return -1;
    }

    if (payload != NULL && payload_len > 0) {
        copy_len = (payload_len < (sizeof(text) - 1)) ? payload_len : (sizeof(text) - 1);
        if (memcpy_s(text, sizeof(text), payload, copy_len) == EOK) {
            osal_mutex_lock(&g_rescue_cloud_mutex);
            (void)memset_s(g_rescue_cloud_sle_text, sizeof(g_rescue_cloud_sle_text), 0,
                           sizeof(g_rescue_cloud_sle_text));
            if (memcpy_s(g_rescue_cloud_sle_text, sizeof(g_rescue_cloud_sle_text), text, copy_len) == EOK) {
                g_rescue_cloud_sle_len = (uint16_t)copy_len;
                g_rescue_cloud_pending_sle = 1;
            }
            if (rescue_cloud_parse_int_field(text, "seq", &seq) &&
                rescue_cloud_parse_int_field(text, "spo2", &spo2) &&
                rescue_cloud_parse_int_field(text, "hr", &heart_rate) &&
                rescue_cloud_parse_int_field(text, "sbp", &systolic) &&
                rescue_cloud_parse_int_field(text, "battery", &battery)) {
                (void)rescue_cloud_parse_int_field(text, "sos", &sos);
                g_rescue_cloud_victim_seq = (uint32_t)seq;
                g_rescue_cloud_victim_spo2 = spo2;
                g_rescue_cloud_victim_hr = heart_rate;
                g_rescue_cloud_victim_sbp = systolic;
                g_rescue_cloud_victim_battery = battery;
                g_rescue_cloud_victim_sos = sos;
                g_rescue_cloud_pending_victim = 1;
            }
            osal_mutex_unlock(&g_rescue_cloud_mutex);
            osal_printk("%s sle payload cached.\r\n", RESCUE_CLOUD_LOG);
            return 0;
        }
    }
    return -1;
}

int rescue_cloud_publish_victim_state(uint32_t seq, int spo2, int heart_rate, int systolic, int battery, int sos)
{
    if (rescue_cloud_lock_init() != 0) {
        return -1;
    }

    osal_mutex_lock(&g_rescue_cloud_mutex);
    g_rescue_cloud_victim_seq = seq;
    g_rescue_cloud_victim_spo2 = spo2;
    g_rescue_cloud_victim_hr = heart_rate;
    g_rescue_cloud_victim_sbp = systolic;
    g_rescue_cloud_victim_battery = battery;
    g_rescue_cloud_victim_sos = sos;
    g_rescue_cloud_pending_victim = 1;
    osal_mutex_unlock(&g_rescue_cloud_mutex);
    return 0;
}

static int rescue_cloud_publish_victim_state_now(uint32_t seq, int spo2, int heart_rate,
    int systolic, int battery, int sos)
{
    char payload[RESCUE_CLOUD_PAYLOAD_MAX_LEN] = {0};
    int ret = snprintf_s(payload, sizeof(payload), sizeof(payload) - 1,
        "{\"services\":[{\"service_id\":\"%s\",\"properties\":{\"seq\":%u,\"spo2\":%d,"
        "\"heart_rate\":%d,\"systolic\":%d,\"battery\":%d,\"sos\":%d}}]}",
        RESCUE_CLOUD_SERVICE_ID, seq, spo2, heart_rate, systolic, battery, sos);
    if (ret <= 0) {
        return -1;
    }
    return rescue_cloud_publish_property_payload("victim_state", payload);
}

static int rescue_cloud_publish_heartbeat(void)
{
    char content[RESCUE_CLOUD_CONTENT_MAX_LEN] = {0};
    int ret = snprintf_s(content, sizeof(content), sizeof(content) - 1,
        "{\"role\":\"%s\",\"event\":\"online\",\"wifi_ssid\":\"%s\"}",
        g_rescue_cloud_role, RESCUE_CLOUD_WIFI_SSID);
    if (ret <= 0) {
        return -1;
    }
    return rescue_cloud_publish_message("heartbeat", (const uint8_t *)content, (uint16_t)ret);
}

static void rescue_cloud_flush_pending(uint32_t now_ms)
{
    char sle_text[RESCUE_CLOUD_CONTENT_MAX_LEN] = {0};
    uint16_t sle_len = 0;
    uint32_t seq = 0;
    int spo2 = 0;
    int heart_rate = 0;
    int systolic = 0;
    int battery = 0;
    int sos = 0;
    uint8_t publish_sle = 0;
    uint8_t publish_victim = 0;

    if ((now_ms - g_rescue_cloud_last_heartbeat_ms) >= RESCUE_CLOUD_HEARTBEAT_MS) {
        (void)rescue_cloud_publish_heartbeat();
        g_rescue_cloud_last_heartbeat_ms = now_ms;
    }

    if ((now_ms - g_rescue_cloud_last_state_ms) < RESCUE_CLOUD_STATE_PUBLISH_MS) {
        return;
    }

    osal_mutex_lock(&g_rescue_cloud_mutex);
    if (g_rescue_cloud_pending_victim) {
        seq = g_rescue_cloud_victim_seq;
        spo2 = g_rescue_cloud_victim_spo2;
        heart_rate = g_rescue_cloud_victim_hr;
        systolic = g_rescue_cloud_victim_sbp;
        battery = g_rescue_cloud_victim_battery;
        sos = g_rescue_cloud_victim_sos;
        g_rescue_cloud_pending_victim = 0;
        publish_victim = 1;
    }
    if (g_rescue_cloud_pending_sle) {
        sle_len = g_rescue_cloud_sle_len;
        if (sle_len >= sizeof(sle_text)) {
            sle_len = sizeof(sle_text) - 1;
        }
        (void)memcpy_s(sle_text, sizeof(sle_text), g_rescue_cloud_sle_text, sle_len);
        g_rescue_cloud_pending_sle = 0;
        publish_sle = 1;
    }
    osal_mutex_unlock(&g_rescue_cloud_mutex);

    if (publish_victim) {
        (void)rescue_cloud_publish_victim_state_now(seq, spo2, heart_rate, systolic, battery, sos);
    }
    if (publish_sle) {
        (void)rescue_cloud_publish_message("sle_report", (const uint8_t *)sle_text, sle_len);
    }
    if (publish_victim || publish_sle) {
        g_rescue_cloud_last_state_ms = now_ms;
    }
}

static void *rescue_cloud_task(const char *arg)
{
    (void)arg;
    uint32_t now_ms = 0;

    while (rescue_wifi_connect() != 0) {
        osal_printk("%s wifi connect failed, retry later.\r\n", RESCUE_CLOUD_LOG);
        osal_msleep(RESCUE_CLOUD_RETRY_MS);
    }
    g_rescue_wifi_ready = 1;
    osal_printk("%s wifi ready, start mqtt.\r\n", RESCUE_CLOUD_LOG);
    (void)rescue_cloud_publish_heartbeat();

    while (1) {
        rescue_cloud_flush_pending(now_ms);
        osal_msleep(RESCUE_CLOUD_LOOP_MS);
        now_ms += RESCUE_CLOUD_LOOP_MS;
    }
    return NULL;
}

void rescue_cloud_start(const char *role)
{
    if (g_rescue_cloud_started) {
        return;
    }
    if (role != NULL) {
        (void)strncpy_s(g_rescue_cloud_role, sizeof(g_rescue_cloud_role), role, sizeof(g_rescue_cloud_role) - 1);
    }
    if (rescue_cloud_lock_init() != 0) {
        return;
    }
    rescue_cloud_mqtt_global_init();

    osal_task *task_handle = osal_kthread_create((osal_kthread_handler)rescue_cloud_task, 0,
                                                 "RescueCloudTask", RESCUE_CLOUD_TASK_STACK_SIZE);
    if (task_handle == NULL) {
        osal_printk("%s create cloud task failed.\r\n", RESCUE_CLOUD_LOG);
        return;
    }
    osal_kthread_set_priority(task_handle, RESCUE_CLOUD_TASK_PRIO);
    g_rescue_cloud_started = 1;
}

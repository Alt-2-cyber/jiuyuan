#include "rescue_route.h"

#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

#define RESCUE_ROUTE_RSSI_MIN (-127)
#define RESCUE_ROUTE_RSSI_MAX 20
#define RESCUE_ROUTE_RSSI_INVALID 127

typedef struct {
    const uint8_t *data;
    size_t len;
    size_t pos;
} rescue_route_cursor_t;

static bool rescue_route_rssi_is_valid(int16_t rssi)
{
    return rssi >= RESCUE_ROUTE_RSSI_MIN && rssi <= RESCUE_ROUTE_RSSI_MAX;
}

static bool rescue_route_status_is_valid(uint8_t hops, int16_t last_rssi)
{
    if (hops == 0U) {
        return last_rssi == RESCUE_ROUTE_RSSI_INVALID;
    }
    return rescue_route_rssi_is_valid(last_rssi);
}

static bool rescue_route_cursor_consume(rescue_route_cursor_t *cursor, const char *literal)
{
    size_t literal_len = strlen(literal);

    if (literal_len > cursor->len - cursor->pos ||
        memcmp(&cursor->data[cursor->pos], literal, literal_len) != 0) {
        return false;
    }
    cursor->pos += literal_len;
    return true;
}

static bool rescue_route_cursor_starts_with(const rescue_route_cursor_t *cursor, const char *literal)
{
    size_t literal_len = strlen(literal);

    return literal_len <= cursor->len - cursor->pos &&
        memcmp(&cursor->data[cursor->pos], literal, literal_len) == 0;
}

static bool rescue_route_cursor_take_token(rescue_route_cursor_t *cursor, const uint8_t **token,
    size_t *token_len)
{
    const uint8_t *start = &cursor->data[cursor->pos];
    const uint8_t *separator = memchr(start, '|', cursor->len - cursor->pos);

    if (separator == NULL || separator == start) {
        return false;
    }
    *token = start;
    *token_len = (size_t)(separator - start);
    cursor->pos += *token_len + 1U;
    return true;
}

static bool rescue_route_parse_u32(const uint8_t *token, size_t token_len, uint32_t limit, uint32_t *value)
{
    uint32_t parsed = 0U;

    if (token_len == 0U) {
        return false;
    }
    for (size_t i = 0U; i < token_len; ++i) {
        uint32_t digit;

        if (token[i] < (uint8_t)'0' || token[i] > (uint8_t)'9') {
            return false;
        }
        digit = (uint32_t)(token[i] - (uint8_t)'0');
        if (digit > limit || parsed > (limit - digit) / 10U) {
            return false;
        }
        parsed = parsed * 10U + digit;
    }
    *value = parsed;
    return true;
}

static bool rescue_route_parse_i16(const uint8_t *token, size_t token_len, int16_t *value)
{
    bool negative = false;
    size_t offset = 0U;
    uint32_t magnitude;
    uint32_t limit = INT16_MAX;

    if (token_len == 0U) {
        return false;
    }
    if (token[0] == (uint8_t)'-') {
        negative = true;
        offset = 1U;
        limit = (uint32_t)INT16_MAX + 1U;
    }
    if (offset == token_len ||
        !rescue_route_parse_u32(&token[offset], token_len - offset, limit, &magnitude)) {
        return false;
    }
    if (negative) {
        *value = (int16_t)(-(int32_t)magnitude);
    } else {
        *value = (int16_t)magnitude;
    }
    return true;
}

static bool rescue_route_cursor_take_u32(rescue_route_cursor_t *cursor, const char *label,
    uint32_t limit, uint32_t *value)
{
    const uint8_t *token;
    size_t token_len;

    if (label != NULL && !rescue_route_cursor_consume(cursor, label)) {
        return false;
    }
    return rescue_route_cursor_take_token(cursor, &token, &token_len) &&
        rescue_route_parse_u32(token, token_len, limit, value);
}

static bool rescue_route_cursor_take_i16(rescue_route_cursor_t *cursor, const char *label, int16_t *value)
{
    const uint8_t *token;
    size_t token_len;

    if (!rescue_route_cursor_consume(cursor, label)) {
        return false;
    }
    return rescue_route_cursor_take_token(cursor, &token, &token_len) &&
        rescue_route_parse_i16(token, token_len, value);
}

static bool rescue_route_decode_common(rescue_route_cursor_t *cursor, rescue_route_packet_t *packet)
{
    uint32_t src;
    uint32_t dst;
    uint32_t ttl;

    if (!rescue_route_cursor_take_u32(cursor, "src=", UINT8_MAX, &src) ||
        !rescue_route_cursor_take_u32(cursor, "dst=", UINT8_MAX, &dst) ||
        !rescue_route_cursor_take_u32(cursor, "ttl=", UINT8_MAX, &ttl) ||
        !rescue_route_cursor_take_u32(cursor, "seq=", UINT32_MAX, &packet->seq) ||
        cursor->pos >= cursor->len) {
        return false;
    }
    packet->src = (uint8_t)src;
    packet->dst = (uint8_t)dst;
    packet->ttl = (uint8_t)ttl;
    return true;
}

static bool rescue_route_decode_current_fields(rescue_route_cursor_t *cursor, rescue_route_packet_t *packet)
{
    uint32_t hops;
    uint32_t route_ok;

    if (!rescue_route_cursor_take_u32(cursor, "hops=", UINT8_MAX, &hops) ||
        !rescue_route_cursor_take_u32(cursor, "route_dm=", UINT32_MAX, &packet->route_dm) ||
        !rescue_route_cursor_take_u32(cursor, "route_ok=", 1U, &route_ok) ||
        !rescue_route_cursor_take_i16(cursor, "last_rssi=", &packet->last_rssi) ||
        cursor->pos >= cursor->len) {
        return false;
    }
    packet->hops = (uint8_t)hops;
    packet->route_ok = route_ok == 1U && rescue_route_status_is_valid(packet->hops, packet->last_rssi);
    return true;
}

bool rescue_route_decode(const uint8_t *wire, size_t wire_len, rescue_route_packet_t *packet)
{
    rescue_route_cursor_t cursor = {wire, wire_len, 0U};
    rescue_route_packet_t decoded = {0};
    rescue_route_cursor_t current_cursor;
    rescue_route_packet_t current_packet;

    if (wire == NULL || packet == NULL || wire_len == 0U ||
        wire_len > RESCUE_ROUTE_WIRE_CAP - 1U ||
        !rescue_route_cursor_consume(&cursor, "JR|")) {
        return false;
    }

    if (!rescue_route_decode_common(&cursor, &decoded)) {
        return false;
    }

    if (rescue_route_cursor_starts_with(&cursor, "hops=")) {
        current_cursor = cursor;
        current_packet = decoded;
        if (!rescue_route_decode_current_fields(&current_cursor, &current_packet)) {
            return false;
        }
        cursor = current_cursor;
        decoded = current_packet;
    } else {
        decoded.hops = 0U;
        decoded.route_dm = 0U;
        decoded.route_ok = false;
        decoded.last_rssi = RESCUE_ROUTE_RSSI_INVALID;
    }
    decoded.body = &wire[cursor.pos];
    decoded.body_len = wire_len - cursor.pos;
    *packet = decoded;
    return true;
}

bool rescue_route_encode(const rescue_route_packet_t *packet, uint8_t *wire, size_t wire_cap, size_t *wire_len)
{
    uint8_t staged[RESCUE_ROUTE_WIRE_CAP];
    bool route_ok;
    int header_len;
    size_t total_len;

    if (wire_len == NULL) {
        return false;
    }
    *wire_len = 0U;
    if (wire != NULL && wire_cap > 0U) {
        wire[0] = '\0';
    }
    if (packet == NULL || wire == NULL || packet->body == NULL || packet->body_len == 0U) {
        return false;
    }

    route_ok = packet->route_ok && rescue_route_status_is_valid(packet->hops, packet->last_rssi);
    header_len = snprintf((char *)staged, sizeof(staged),
        "JR|src=%u|dst=%u|ttl=%u|seq=%" PRIu32 "|hops=%u|route_dm=%" PRIu32
        "|route_ok=%u|last_rssi=%d|",
        (unsigned int)packet->src, (unsigned int)packet->dst, (unsigned int)packet->ttl,
        packet->seq, (unsigned int)packet->hops, packet->route_dm, route_ok ? 1U : 0U,
        (int)packet->last_rssi);
    if (header_len < 0 || (size_t)header_len >= sizeof(staged) ||
        packet->body_len > (RESCUE_ROUTE_WIRE_CAP - 1U) - (size_t)header_len) {
        return false;
    }

    total_len = (size_t)header_len + packet->body_len;
    if (wire_cap <= total_len) {
        return false;
    }
    memcpy(&staged[header_len], packet->body, packet->body_len);
    staged[total_len] = '\0';
    memmove(wire, staged, total_len + 1U);
    *wire_len = total_len;
    return true;
}

uint16_t rescue_route_rssi_to_dm(int16_t rssi)
{
    if (rssi >= -45) {
        return 10U;
    }
    if (rssi >= -55) {
        return (uint16_t)(20 + (-45 - rssi) * 3);
    }
    if (rssi >= -65) {
        return (uint16_t)(60 + (-55 - rssi) * 8);
    }
    if (rssi >= -75) {
        return (uint16_t)(160 + (-65 - rssi) * 18);
    }
    if (rssi >= -85) {
        return (uint16_t)(380 + (-75 - rssi) * 40);
    }
    return 900U;
}

bool rescue_route_add_hop(rescue_route_packet_t *packet, int16_t rssi)
{
    bool rssi_valid;
    uint16_t hop_dm = 0U;

    if (packet == NULL || packet->ttl == 0U || packet->hops == UINT8_MAX) {
        return false;
    }
    rssi_valid = rescue_route_rssi_is_valid(rssi);
    if (rssi_valid) {
        hop_dm = rescue_route_rssi_to_dm(rssi);
        if (packet->route_dm > UINT32_MAX - hop_dm) {
            return false;
        }
    }

    packet->ttl--;
    packet->hops++;
    packet->last_rssi = rssi;
    packet->route_ok = packet->route_ok && rssi_valid;
    if (rssi_valid) {
        packet->route_dm += hop_dm;
    }
    return true;
}

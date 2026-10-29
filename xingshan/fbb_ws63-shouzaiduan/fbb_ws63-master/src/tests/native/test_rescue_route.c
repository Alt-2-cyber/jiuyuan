#include "rescue_route.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                                            \
    do {                                                                                            \
        if (!(condition)) {                                                                         \
            (void)fprintf(stderr, "%s:%d: CHECK(%s) failed\n", __FILE__, __LINE__, #condition);    \
            return false;                                                                           \
        }                                                                                           \
    } while (0)

static bool bytes_equal(const uint8_t *actual, size_t actual_len, const char *expected)
{
    size_t expected_len = strlen(expected);

    return actual_len == expected_len && memcmp(actual, expected, expected_len) == 0;
}

static bool test_legacy_decode_marks_route_unknown_and_preserves_body(void)
{
    static const uint8_t wire[] = "JR|src=1|dst=0|ttl=4|seq=17|{\"sos\":1,\"note\":\"a|b\"}";
    static const char expected_body[] = "{\"sos\":1,\"note\":\"a|b\"}";
    rescue_route_packet_t packet;

    CHECK(rescue_route_decode(wire, sizeof(wire) - 1U, &packet));
    CHECK(packet.src == 1U);
    CHECK(packet.dst == 0U);
    CHECK(packet.ttl == 4U);
    CHECK(packet.seq == 17U);
    CHECK(packet.hops == 0U);
    CHECK(packet.route_dm == 0U);
    CHECK(!packet.route_ok);
    CHECK(bytes_equal(packet.body, packet.body_len, expected_body));
    return true;
}

static bool test_malformed_current_fields_are_rejected(void)
{
    static const uint8_t wire[] =
        "JR|src=3|dst=1|ttl=4|seq=17|"
        "hops=1|route_dm=10|route_ok=1|last_rssi=not-route|{\"sos\":1}";
    rescue_route_packet_t packet;

    CHECK(!rescue_route_decode(wire, sizeof(wire) - 1U, &packet));
    return true;
}

static bool test_new_format_round_trip_preserves_body_bytes(void)
{
    static const uint8_t wire[] =
        "JR|src=1|dst=0|ttl=4|seq=17|hops=2|route_dm=220|route_ok=1|last_rssi=-60|"
        "{\"sos\":1,\"note\":\"a|b\"}";
    static const char expected_body[] = "{\"sos\":1,\"note\":\"a|b\"}";
    rescue_route_packet_t packet;
    uint8_t encoded[RESCUE_ROUTE_WIRE_CAP];
    size_t encoded_len = 0U;

    CHECK(rescue_route_decode(wire, sizeof(wire) - 1U, &packet));
    CHECK(packet.src == 1U);
    CHECK(packet.dst == 0U);
    CHECK(packet.ttl == 4U);
    CHECK(packet.seq == 17U);
    CHECK(packet.hops == 2U);
    CHECK(packet.route_dm == 220U);
    CHECK(packet.route_ok);
    CHECK(packet.last_rssi == -60);
    CHECK(bytes_equal(packet.body, packet.body_len, expected_body));

    CHECK(rescue_route_encode(&packet, encoded, sizeof(encoded), &encoded_len));
    CHECK(encoded_len == sizeof(wire) - 1U);
    CHECK(memcmp(encoded, wire, encoded_len) == 0);
    CHECK(encoded[encoded_len] == '\0');
    return true;
}

static bool test_initial_route_accepts_first_valid_hop(void)
{
    static const uint8_t wire[] =
        "JR|src=3|dst=1|ttl=4|seq=17|hops=0|route_dm=0|route_ok=1|last_rssi=127|{\"sos\":1}";
    static const char expected[] =
        "JR|src=3|dst=1|ttl=3|seq=17|hops=1|route_dm=10|route_ok=1|last_rssi=-45|{\"sos\":1}";
    rescue_route_packet_t packet;
    uint8_t encoded[RESCUE_ROUTE_WIRE_CAP];
    size_t encoded_len = 0U;

    CHECK(rescue_route_decode(wire, sizeof(wire) - 1U, &packet));
    CHECK(packet.hops == 0U);
    CHECK(packet.route_dm == 0U);
    CHECK(packet.route_ok);
    CHECK(packet.last_rssi == 127);

    CHECK(rescue_route_add_hop(&packet, -45));
    CHECK(packet.ttl == 3U);
    CHECK(packet.hops == 1U);
    CHECK(packet.route_dm == 10U);
    CHECK(packet.route_ok);
    CHECK(packet.last_rssi == -45);
    CHECK(rescue_route_encode(&packet, encoded, sizeof(encoded), &encoded_len));
    CHECK(bytes_equal(encoded, encoded_len, expected));
    return true;
}

static bool test_rssi_piecewise_boundaries(void)
{
    static const struct {
        int16_t rssi;
        uint16_t dm;
    } cases[] = {
        {-44, 10U}, {-45, 10U}, {-46, 23U}, {-55, 50U}, {-56, 68U},
        {-65, 140U}, {-66, 178U}, {-75, 340U}, {-76, 420U}, {-85, 780U}, {-86, 900U},
    };

    for (size_t i = 0U; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        CHECK(rescue_route_rssi_to_dm(cases[i].rssi) == cases[i].dm);
    }
    return true;
}

static rescue_route_packet_t make_packet(uint8_t ttl, uint8_t hops, uint32_t route_dm)
{
    static const uint8_t body[] = "{}";
    rescue_route_packet_t packet = {
        .src = 1U,
        .dst = 0U,
        .ttl = ttl,
        .hops = hops,
        .seq = 99U,
        .route_dm = route_dm,
        .route_ok = true,
        .last_rssi = -45,
        .body = body,
        .body_len = sizeof(body) - 1U,
    };

    return packet;
}

static bool test_add_hop_adds_exactly_one_hop(void)
{
    rescue_route_packet_t packet = make_packet(4U, 2U, 100U);

    CHECK(rescue_route_add_hop(&packet, -60));
    CHECK(packet.ttl == 3U);
    CHECK(packet.hops == 3U);
    CHECK(packet.route_dm == 200U);
    CHECK(packet.route_ok);
    CHECK(packet.last_rssi == -60);
    return true;
}

static bool test_ttl_one_can_finish_current_encode(void)
{
    static const char expected[] =
        "JR|src=1|dst=0|ttl=0|seq=99|hops=1|route_dm=10|route_ok=1|last_rssi=-45|{}";
    rescue_route_packet_t packet = make_packet(1U, 0U, 0U);
    uint8_t encoded[RESCUE_ROUTE_WIRE_CAP];
    size_t encoded_len = 0U;

    CHECK(rescue_route_add_hop(&packet, -45));
    CHECK(packet.ttl == 0U);
    CHECK(rescue_route_encode(&packet, encoded, sizeof(encoded), &encoded_len));
    CHECK(bytes_equal(encoded, encoded_len, expected));
    return true;
}

static bool test_zero_ttl_rejects_add_hop_without_mutation(void)
{
    rescue_route_packet_t packet = make_packet(0U, 7U, 321U);

    CHECK(!rescue_route_add_hop(&packet, -50));
    CHECK(packet.ttl == 0U);
    CHECK(packet.hops == 7U);
    CHECK(packet.route_dm == 321U);
    CHECK(packet.route_ok);
    CHECK(packet.last_rssi == -45);
    return true;
}

static bool test_invalid_rssi_marks_route_not_ok(void)
{
    static const uint8_t invalid_wire[] =
        "JR|src=1|dst=0|ttl=4|seq=17|hops=2|route_dm=220|route_ok=1|last_rssi=127|{}";
    rescue_route_packet_t packet = make_packet(2U, 1U, 10U);
    rescue_route_packet_t decoded;

    CHECK(rescue_route_add_hop(&packet, 127));
    CHECK(packet.ttl == 1U);
    CHECK(packet.hops == 2U);
    CHECK(packet.route_dm == 10U);
    CHECK(!packet.route_ok);
    CHECK(packet.last_rssi == 127);

    CHECK(rescue_route_decode(invalid_wire, sizeof(invalid_wire) - 1U, &decoded));
    CHECK(!decoded.route_ok);
    return true;
}

static bool test_encode_rejects_wire_longer_than_255_bytes(void)
{
    uint8_t body[220];
    rescue_route_packet_t packet = make_packet(4U, 1U, 10U);
    uint8_t encoded[RESCUE_ROUTE_WIRE_CAP];
    size_t encoded_len = 123U;

    memset(body, 'x', sizeof(body));
    memset(encoded, 'x', sizeof(encoded));
    packet.body = body;
    packet.body_len = sizeof(body);

    CHECK(!rescue_route_encode(&packet, encoded, sizeof(encoded), &encoded_len));
    CHECK(encoded_len == 0U);
    CHECK(encoded[0] == '\0');
    return true;
}

static bool test_encode_accepts_wire_exactly_255_bytes(void)
{
    static const char header[] =
        "JR|src=1|dst=0|ttl=4|seq=99|hops=1|route_dm=10|route_ok=1|last_rssi=-45|";
    uint8_t body[RESCUE_ROUTE_WIRE_CAP];
    rescue_route_packet_t packet = make_packet(4U, 1U, 10U);
    uint8_t encoded[RESCUE_ROUTE_WIRE_CAP];
    size_t encoded_len = 0U;

    memset(body, 'x', sizeof(body));
    packet.body = body;
    packet.body_len = (RESCUE_ROUTE_WIRE_CAP - 1U) - (sizeof(header) - 1U);

    CHECK(rescue_route_encode(&packet, encoded, sizeof(encoded), &encoded_len));
    CHECK(encoded_len == RESCUE_ROUTE_WIRE_CAP - 1U);
    CHECK(encoded[encoded_len] == '\0');
    CHECK(memcmp(encoded, header, sizeof(header) - 1U) == 0);
    return true;
}

static bool test_decode_rejects_256_byte_wire(void)
{
    static const char prefix[] = "JR|src=1|dst=0|ttl=4|seq=17|";
    uint8_t wire[RESCUE_ROUTE_WIRE_CAP];
    rescue_route_packet_t packet;

    memset(wire, 'x', sizeof(wire));
    memcpy(wire, prefix, sizeof(prefix) - 1U);
    CHECK(!rescue_route_decode(wire, sizeof(wire), &packet));
    return true;
}

static bool test_decode_respects_wire_len_and_numeric_ranges(void)
{
    static const uint8_t storage[] = "JR|src=1|dst=0|ttl=4|seq=17|{\"x\":1}ignored";
    static const char expected_body[] = "{\"x\":1}";
    static const uint8_t src_overflow[] = "JR|src=256|dst=0|ttl=4|seq=17|{}";
    static const uint8_t seq_overflow[] =
        "JR|src=1|dst=0|ttl=4|seq=4294967296|hops=0|route_dm=0|route_ok=0|last_rssi=-45|{}";
    static const uint8_t route_ok_overflow[] =
        "JR|src=1|dst=0|ttl=4|seq=17|hops=0|route_dm=0|route_ok=2|last_rssi=-45|{}";
    rescue_route_packet_t packet;
    const size_t bounded_len = sizeof("JR|src=1|dst=0|ttl=4|seq=17|{\"x\":1}") - 1U;

    CHECK(rescue_route_decode(storage, bounded_len, &packet));
    CHECK(bytes_equal(packet.body, packet.body_len, expected_body));
    CHECK(!rescue_route_decode(src_overflow, sizeof(src_overflow) - 1U, &packet));
    CHECK(!rescue_route_decode(seq_overflow, sizeof(seq_overflow) - 1U, &packet));
    CHECK(!rescue_route_decode(route_ok_overflow, sizeof(route_ok_overflow) - 1U, &packet));
    return true;
}

typedef bool (*test_fn_t)(void);

static bool run_test(const char *name, test_fn_t test)
{
    if (!test()) {
        (void)fprintf(stderr, "FAIL %s\n", name);
        return false;
    }
    (void)printf("PASS %s\n", name);
    return true;
}

int main(void)
{
    static const struct {
        const char *name;
        test_fn_t fn;
    } tests[] = {
        {"legacy decode", test_legacy_decode_marks_route_unknown_and_preserves_body},
        {"new format round trip", test_new_format_round_trip_preserves_body_bytes},
        {"initial route first hop", test_initial_route_accepts_first_valid_hop},
        {"RSSI boundaries", test_rssi_piecewise_boundaries},
        {"single hop", test_add_hop_adds_exactly_one_hop},
        {"ttl one encode", test_ttl_one_can_finish_current_encode},
        {"ttl zero rejection", test_zero_ttl_rejects_add_hop_without_mutation},
        {"invalid RSSI", test_invalid_rssi_marks_route_not_ok},
        {"wire length cap", test_encode_rejects_wire_longer_than_255_bytes},
        {"wire length exactly 255", test_encode_accepts_wire_exactly_255_bytes},
        {"decode length 256", test_decode_rejects_256_byte_wire},
        {"bounded decode", test_decode_respects_wire_len_and_numeric_ranges},
        {"malformed current", test_malformed_current_fields_are_rejected},
    };

    for (size_t i = 0U; i < sizeof(tests) / sizeof(tests[0]); ++i) {
        if (!run_test(tests[i].name, tests[i].fn)) {
            return 1;
        }
    }
    (void)printf("All %zu rescue route tests passed.\n", sizeof(tests) / sizeof(tests[0]));
    return 0;
}

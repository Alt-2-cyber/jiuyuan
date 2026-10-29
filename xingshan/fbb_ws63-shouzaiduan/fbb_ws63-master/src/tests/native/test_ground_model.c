#define OK 0
#define NONE 0
#include "ground_model.h"
#undef NONE
#undef OK

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

#define ARRAY_SIZE(array) (sizeof(array) / sizeof((array)[0]))

static bool parse_text(const char *wire, uint64_t now_ms, ground_report_t *report)
{
    return ground_model_parse_report((const uint8_t *)wire, strlen(wire), now_ms, report);
}

static void debounce(ground_model_t *model, bool k1, bool k2, uint64_t changed_ms)
{
    ground_model_sample_buttons(model, k1, k2, changed_ms);
    ground_model_sample_buttons(model, k1, k2, changed_ms + 30U);
}

static bool test_init_has_known_defaults(void)
{
    ground_model_t model;

    memset(&model, 0xA5, sizeof(model));
    ground_model_init(&model);
    CHECK(model.selected_victim == 0U);
    CHECK(model.page == GROUND_PAGE_VICTIM);
    for (size_t i = 0U; i < ARRAY_SIZE(model.victims); ++i) {
        CHECK(!model.victims[i].valid);
    }
    return true;
}

static bool test_three_ids_map_to_fixed_slots(void)
{
    static const char *const wires[] = {
        "JR|src=1|dst=0|ttl=4|seq=11|{\"type\":\"VICTIM\",\"id\":\"VD-001\",\"spo2\":98,\"hr\":71,\"battery\":90,\"sos\":0,\"source\":\"MAX30102\"}",
        "JR|src=2|dst=0|ttl=4|seq=12|{\"type\":\"VICTIM\",\"id\":\"VD-002\",\"spo2\":97,\"hr\":72,\"battery\":80,\"sos\":1,\"source\":\"SIM\"}",
        "JR|src=3|dst=0|ttl=4|seq=13|{\"type\":\"VICTIM\",\"id\":\"VD-003\",\"spo2\":96,\"hr\":73,\"battery\":70,\"sos\":0,\"source\":\"MAX30102\"}",
    };
    ground_model_t model;

    ground_model_init(&model);
    for (size_t i = 0U; i < ARRAY_SIZE(wires); ++i) {
        ground_report_t report;

        CHECK(parse_text(wires[i], 100U + i, &report));
        CHECK(report.victim_index == i);
        CHECK(ground_model_apply_report(&model, &report));
        CHECK(model.victims[i].valid);
        CHECK(model.victims[i].seq == 11U + i);
    }
    return true;
}

static bool test_legacy_packet_marks_route_unknown(void)
{
    static const char wire[] =
        "JR|src=1|dst=0|ttl=4|seq=17|"
        "{\"type\":\"VICTIM\",\"id\":\"VD-001\",\"seq\":999,\"spo2\":99,\"hr\":68,"
        "\"sbp\":120,\"battery\":88,\"sos\":1,\"source\":\"MAX30102\"}\r\n";
    ground_report_t report;

    CHECK(parse_text(wire, 1234U, &report));
    CHECK(report.victim.seq == 17U);
    CHECK(report.victim.route_dm == 0U);
    CHECK(report.victim.hops == 0U);
    CHECK(!report.victim.route_ok);
    CHECK(report.victim.last_rx_ms == 1234U);
    return true;
}

static bool test_new_packet_copies_route_and_source(void)
{
    static const char max_wire[] =
        "JR|src=1|dst=0|ttl=3|seq=23|hops=2|route_dm=345|route_ok=1|last_rssi=-60|"
        "{\"type\":\"VICTIM\",\"id\":\"VD-002\",\"spo2\":98,\"hr\":75,\"battery\":64,\"sos\":1,\"source\":\"MAX30102\"}";
    static const char sim_wire[] =
        "JR|src=1|dst=0|ttl=3|seq=24|hops=1|route_dm=90|route_ok=1|last_rssi=-55|"
        "{\"type\":\"VICTIM\",\"id\":\"VD-003\",\"spo2\":-1,\"hr\":-1,\"battery\":-1,\"sos\":0,\"source\":\"SIM\"}";
    ground_report_t report;

    CHECK(parse_text(max_wire, 2000U, &report));
    CHECK(report.victim_index == 1U);
    CHECK(report.victim.route_dm == 345U);
    CHECK(report.victim.hops == 2U);
    CHECK(report.victim.route_ok);
    CHECK(report.victim.source_max30102);
    CHECK(!report.victim.source_sim);

    CHECK(parse_text(sim_wire, 2001U, &report));
    CHECK(!report.victim.source_max30102);
    CHECK(report.victim.source_sim);
    CHECK(report.victim.spo2 == -1);
    CHECK(report.victim.heart_rate == -1);
    CHECK(report.victim.battery == -1);
    return true;
}

static bool test_parse_rejects_bad_json_unknown_values_and_invalid_arguments(void)
{
    static const char *const invalid[] = {
        "JR|src=1|dst=0|ttl=4|seq=1|not-json",
        "JR|src=1|dst=0|ttl=4|seq=1|{\"type\":\"ENV\",\"id\":\"VD-001\",\"spo2\":98,\"hr\":70,\"battery\":80,\"sos\":0,\"source\":\"SIM\"}",
        "JR|src=1|dst=0|ttl=4|seq=1|{\"type\":\"VICTIM\",\"id\":\"VD-004\",\"spo2\":98,\"hr\":70,\"battery\":80,\"sos\":0,\"source\":\"SIM\"}",
        "JR|src=1|dst=0|ttl=4|seq=1|{\"type\":\"VICTIM\",\"id\":\"VD-001\",\"spo2\":101,\"hr\":70,\"battery\":80,\"sos\":0,\"source\":\"SIM\"}",
        "JR|src=1|dst=0|ttl=4|seq=1|{\"type\":\"VICTIM\",\"id\":\"VD-001\",\"spo2\":98,\"hr\":251,\"battery\":80,\"sos\":0,\"source\":\"SIM\"}",
        "JR|src=1|dst=0|ttl=4|seq=1|{\"type\":\"VICTIM\",\"id\":\"VD-001\",\"spo2\":98,\"hr\":70,\"battery\":-2,\"sos\":0,\"source\":\"SIM\"}",
        "JR|src=1|dst=0|ttl=4|seq=1|{\"type\":\"VICTIM\",\"id\":\"VD-001\",\"spo2\":98,\"hr\":70,\"battery\":80,\"sos\":2,\"source\":\"SIM\"}",
        "JR|src=1|dst=0|ttl=4|seq=1|{\"type\":\"VICTIM\",\"id\":\"VD-001\",\"spo2\":98,\"hr\":70,\"battery\":80,\"sos\":0,\"source\":\"OTHER\"}",
        "JR|src=1|dst=0|ttl=4|seq=1|{\"type\":\"VICTIM\",\"id\":\"VD-001\",\"spo2\":98,\"hr\":70,\"battery\":80,\"sos\":0}",
    };
    static const uint8_t valid[] =
        "JR|src=1|dst=0|ttl=4|seq=1|{\"type\":\"VICTIM\",\"id\":\"VD-001\",\"spo2\":98,\"hr\":70,\"battery\":80,\"sos\":0,\"source\":\"SIM\"}";
    ground_report_t report;

    for (size_t i = 0U; i < ARRAY_SIZE(invalid); ++i) {
        CHECK(!parse_text(invalid[i], 0U, &report));
    }
    CHECK(!ground_model_parse_report(NULL, sizeof(valid) - 1U, 0U, &report));
    CHECK(!ground_model_parse_report(valid, sizeof(valid) - 1U, 0U, NULL));
    CHECK(!ground_model_parse_report(valid, 0U, 0U, &report));
    return true;
}

static bool test_parse_rejects_embedded_nul_with_trailing_bytes(void)
{
    static const uint8_t wire[] =
        "JR|src=1|dst=0|ttl=4|seq=1|"
        "{\"type\":\"VICTIM\",\"id\":\"VD-001\",\"spo2\":98,\"hr\":70,\"battery\":80,"
        "\"sos\":0,\"source\":\"SIM\"}\0garbage";
    ground_report_t report;

    CHECK(!ground_model_parse_report(wire, sizeof(wire) - 1U, 0U, &report));
    return true;
}

static bool test_json_integer_leading_zero_rules(void)
{
    static const char positive_leading_zero[] =
        "JR|src=1|dst=0|ttl=4|seq=1|"
        "{\"type\":\"VICTIM\",\"id\":\"VD-001\",\"spo2\":01,\"hr\":70,\"battery\":80,"
        "\"sos\":0,\"source\":\"SIM\"}";
    static const char negative_leading_zero[] =
        "JR|src=1|dst=0|ttl=4|seq=1|"
        "{\"type\":\"VICTIM\",\"id\":\"VD-001\",\"spo2\":-01,\"hr\":70,\"battery\":80,"
        "\"sos\":0,\"source\":\"SIM\"}";
    static const char negative_zero[] =
        "JR|src=1|dst=0|ttl=4|seq=1|"
        "{\"type\":\"VICTIM\",\"id\":\"VD-001\",\"spo2\":-0,\"hr\":70,\"battery\":80,"
        "\"sos\":0,\"source\":\"SIM\"}";
    ground_report_t report;
    bool accepted_positive = parse_text(positive_leading_zero, 0U, &report);
    bool accepted_negative = parse_text(negative_leading_zero, 0U, &report);

    CHECK(!accepted_negative);
    CHECK(!accepted_positive);
    CHECK(parse_text(negative_zero, 0U, &report));
    CHECK(report.victim.spo2 == 0);
    return true;
}

static bool test_json_integer_extremes_and_overflow(void)
{
    static const char extrema[] =
        "JR|src=1|dst=0|ttl=4|seq=1|"
        "{\"type\":\"VICTIM\",\"id\":\"VD-001\",\"seq\":9223372036854775807,"
        "\"sbp\":-9223372036854775808,\"spo2\":98,\"hr\":70,\"battery\":80,"
        "\"sos\":0,\"source\":\"SIM\"}";
    static const char *const invalid[] = {
        "JR|src=1|dst=0|ttl=4|seq=1|{\"type\":\"VICTIM\",\"id\":\"VD-001\",\"seq\":9223372036854775808,\"spo2\":98,\"hr\":70,\"battery\":80,\"sos\":0,\"source\":\"SIM\"}",
        "JR|src=1|dst=0|ttl=4|seq=1|{\"type\":\"VICTIM\",\"id\":\"VD-001\",\"sbp\":-9223372036854775809,\"spo2\":98,\"hr\":70,\"battery\":80,\"sos\":0,\"source\":\"SIM\"}",
        "JR|src=1|dst=0|ttl=4|seq=1|{\"type\":\"VICTIM\",\"id\":\"VD-001\",\"sbp\":-,\"spo2\":98,\"hr\":70,\"battery\":80,\"sos\":0,\"source\":\"SIM\"}",
    };
    ground_report_t report;

    CHECK(parse_text(extrema, 0U, &report));
    for (size_t i = 0U; i < ARRAY_SIZE(invalid); ++i) {
        CHECK(!parse_text(invalid[i], 0U, &report));
    }
    return true;
}

static bool test_report_value_ranges_reject_every_outside_boundary(void)
{
    static const struct {
        int spo2;
        int heart_rate;
        int battery;
        int sos;
    } cases[] = {
        {-2, 70, 80, 0},
        {101, 70, 80, 0},
        {98, -2, 80, 0},
        {98, 251, 80, 0},
        {98, 70, -2, 0},
        {98, 70, 101, 0},
        {98, 70, 80, -1},
        {98, 70, 80, 2},
    };
    char wire[RESCUE_ROUTE_WIRE_CAP];
    ground_report_t report;

    for (size_t i = 0U; i < ARRAY_SIZE(cases); ++i) {
        int written = snprintf(wire, sizeof(wire),
            "JR|src=1|dst=0|ttl=4|seq=1|"
            "{\"type\":\"VICTIM\",\"id\":\"VD-001\",\"spo2\":%d,\"hr\":%d,"
            "\"battery\":%d,\"sos\":%d,\"source\":\"SIM\"}",
            cases[i].spo2, cases[i].heart_rate, cases[i].battery, cases[i].sos);

        CHECK(written > 0);
        CHECK((size_t)written < sizeof(wire));
        CHECK(!ground_model_parse_report((const uint8_t *)wire, (size_t)written, 0U, &report));
    }
    return true;
}

static bool test_protocol_length_limit_and_maximum_bounded_body(void)
{
    static const char valid[] =
        "JR|src=1|dst=0|ttl=4|seq=1|"
        "{\"type\":\"VICTIM\",\"id\":\"VD-001\",\"spo2\":98,\"hr\":70,\"battery\":80,"
        "\"sos\":0,\"source\":\"SIM\"}";
    uint8_t wire[RESCUE_ROUTE_WIRE_CAP];
    ground_report_t report;

    CHECK(sizeof(valid) - 1U < sizeof(wire));
    memcpy(wire, valid, sizeof(valid) - 1U);
    memset(&wire[sizeof(valid) - 1U], ' ', sizeof(wire) - (sizeof(valid) - 1U));

    CHECK(ground_model_parse_report(wire, RESCUE_ROUTE_WIRE_CAP - 1U, 0U, &report));
    CHECK(!ground_model_parse_report(wire, RESCUE_ROUTE_WIRE_CAP, 0U, &report));
    return true;
}

static bool test_apply_only_accepts_strictly_newer_sequence(void)
{
    ground_model_t model;
    ground_report_t report = {
        .victim_index = 1U,
        .victim = {
            .valid = true,
            .seq = 10U,
            .spo2 = 90,
            .heart_rate = 60,
        },
    };

    ground_model_init(&model);
    CHECK(ground_model_apply_report(&model, &report));
    CHECK(model.victims[1].spo2 == 90);

    report.victim.spo2 = 91;
    CHECK(!ground_model_apply_report(&model, &report));
    CHECK(model.victims[1].spo2 == 90);

    report.victim.seq = 9U;
    CHECK(!ground_model_apply_report(&model, &report));
    CHECK(model.victims[1].spo2 == 90);

    report.victim.seq = 11U;
    CHECK(ground_model_apply_report(&model, &report));
    CHECK(model.victims[1].spo2 == 91);

    report.victim_index = 3U;
    CHECK(!ground_model_apply_report(&model, &report));
    CHECK(!ground_model_apply_report(NULL, &report));
    CHECK(!ground_model_apply_report(&model, NULL));
    return true;
}

static bool test_link_boundaries_and_clock_rollback(void)
{
    ground_victim_t victim = {0};

    CHECK(ground_model_link(NULL, 0U) == GROUND_LINK_NONE);
    CHECK(ground_model_link(&victim, 0U) == GROUND_LINK_NONE);
    victim.valid = true;
    victim.last_rx_ms = 1000U;
    CHECK(ground_model_link(&victim, 7000U) == GROUND_LINK_OK);
    CHECK(ground_model_link(&victim, 7001U) == GROUND_LINK_LOST);
    CHECK(ground_model_link(&victim, 999U) == GROUND_LINK_LOST);
    return true;
}

static bool test_k1_and_k2_cycle_on_stable_release(void)
{
    ground_model_t model;

    ground_model_init(&model);
    ground_model_sample_buttons(&model, false, false, 0U);
    debounce(&model, true, false, 10U);
    CHECK(model.selected_victim == 0U);
    debounce(&model, false, false, 50U);
    CHECK(model.selected_victim == 2U);

    debounce(&model, false, true, 100U);
    CHECK(model.selected_victim == 2U);
    debounce(&model, false, false, 140U);
    CHECK(model.selected_victim == 0U);
    return true;
}

static bool test_resistor_ladder_key_voltage_boundaries(void)
{
    CHECK(ground_model_decode_key_mv(0U) == GROUND_KEY_K1);
    CHECK(ground_model_decode_key_mv(749U) == GROUND_KEY_K1);
    CHECK(ground_model_decode_key_mv(750U) == GROUND_KEY_K2);
    CHECK(ground_model_decode_key_mv(1399U) == GROUND_KEY_K2);
    CHECK(ground_model_decode_key_mv(1400U) == GROUND_KEY_NONE);
    CHECK(ground_model_decode_key_mv(3300U) == GROUND_KEY_NONE);
    return true;
}

static bool test_k1_long_hold_toggles_page_once_and_suppresses_previous(void)
{
    ground_model_t model;

    ground_model_init(&model);
    ground_model_sample_buttons(&model, false, false, 0U);
    debounce(&model, true, false, 10U);
    CHECK(model.page == GROUND_PAGE_VICTIM);
    ground_model_sample_buttons(&model, true, false, 839U);
    CHECK(model.page == GROUND_PAGE_VICTIM);
    ground_model_sample_buttons(&model, true, false, 840U);
    CHECK(model.page == GROUND_PAGE_ENV);
    ground_model_sample_buttons(&model, true, false, 5000U);
    CHECK(model.page == GROUND_PAGE_ENV);
    debounce(&model, false, false, 5010U);
    CHECK(model.page == GROUND_PAGE_ENV);
    CHECK(model.selected_victim == 0U);
    return true;
}

static bool test_bounce_does_not_trigger_early_or_repeat(void)
{
    ground_model_t model;

    ground_model_init(&model);
    ground_model_sample_buttons(&model, false, false, 0U);
    ground_model_sample_buttons(&model, false, true, 10U);
    ground_model_sample_buttons(&model, false, false, 20U);
    ground_model_sample_buttons(&model, false, true, 25U);
    ground_model_sample_buttons(&model, false, true, 54U);
    CHECK(model.selected_victim == 0U);
    ground_model_sample_buttons(&model, false, true, 55U);
    ground_model_sample_buttons(&model, false, false, 60U);
    ground_model_sample_buttons(&model, false, true, 70U);
    ground_model_sample_buttons(&model, false, false, 80U);
    ground_model_sample_buttons(&model, false, false, 109U);
    CHECK(model.selected_victim == 0U);
    ground_model_sample_buttons(&model, false, false, 110U);
    CHECK(model.selected_victim == 1U);
    ground_model_sample_buttons(&model, false, false, 500U);
    CHECK(model.selected_victim == 1U);
    return true;
}

static bool test_chord_takes_priority_and_toggles_after_both_release(void)
{
    ground_model_t model;

    ground_model_init(&model);
    ground_model_sample_buttons(&model, false, false, 0U);
    debounce(&model, true, true, 10U);
    CHECK(model.page == GROUND_PAGE_VICTIM);
    debounce(&model, false, true, 50U);
    CHECK(model.page == GROUND_PAGE_VICTIM);
    debounce(&model, false, false, 90U);
    CHECK(model.page == GROUND_PAGE_ENV);
    CHECK(model.selected_victim == 0U);
    return true;
}

static bool test_environment_k1_short_press_returns_to_victim_page(void)
{
    ground_model_t model;

    ground_model_init(&model);
    model.selected_victim = 2U;
    model.page = GROUND_PAGE_ENV;
    ground_model_sample_buttons(&model, false, false, 0U);
    debounce(&model, true, false, 10U);
    debounce(&model, false, false, 50U);
    CHECK(model.page == GROUND_PAGE_VICTIM);
    CHECK(model.selected_victim == 2U);
    return true;
}

static bool test_long_hold_never_repeats_and_release_rearms(void)
{
    ground_model_t model;

    ground_model_init(&model);
    ground_model_sample_buttons(&model, false, false, 0U);
    debounce(&model, false, true, 10U);
    ground_model_sample_buttons(&model, false, true, 1000U);
    ground_model_sample_buttons(&model, false, true, 10000U);
    CHECK(model.selected_victim == 0U);
    debounce(&model, false, false, 10010U);
    CHECK(model.selected_victim == 1U);
    ground_model_sample_buttons(&model, false, false, 20000U);
    CHECK(model.selected_victim == 1U);

    debounce(&model, false, true, 20010U);
    debounce(&model, false, false, 20050U);
    CHECK(model.selected_victim == 2U);
    return true;
}

static bool test_chord_hold_toggles_once_and_requires_full_release(void)
{
    ground_model_t model;

    ground_model_init(&model);
    ground_model_sample_buttons(&model, false, false, 0U);
    debounce(&model, true, true, 10U);
    ground_model_sample_buttons(&model, true, true, 5000U);
    CHECK(model.page == GROUND_PAGE_VICTIM);
    debounce(&model, false, true, 5010U);
    ground_model_sample_buttons(&model, true, true, 5050U);
    ground_model_sample_buttons(&model, true, true, 5080U);
    CHECK(model.page == GROUND_PAGE_VICTIM);
    debounce(&model, false, false, 5100U);
    CHECK(model.page == GROUND_PAGE_ENV);
    ground_model_sample_buttons(&model, false, false, 9000U);
    CHECK(model.page == GROUND_PAGE_ENV);

    debounce(&model, true, true, 9010U);
    debounce(&model, false, false, 9050U);
    CHECK(model.page == GROUND_PAGE_VICTIM);
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
        {"init defaults", test_init_has_known_defaults},
        {"three fixed IDs", test_three_ids_map_to_fixed_slots},
        {"legacy route unknown", test_legacy_packet_marks_route_unknown},
        {"new route and sources", test_new_packet_copies_route_and_source},
        {"parse rejection", test_parse_rejects_bad_json_unknown_values_and_invalid_arguments},
        {"embedded NUL rejection", test_parse_rejects_embedded_nul_with_trailing_bytes},
        {"JSON leading zero rules", test_json_integer_leading_zero_rules},
        {"JSON integer extremes", test_json_integer_extremes_and_overflow},
        {"report value boundaries", test_report_value_ranges_reject_every_outside_boundary},
        {"protocol length boundary", test_protocol_length_limit_and_maximum_bounded_body},
        {"strictly newer sequence", test_apply_only_accepts_strictly_newer_sequence},
        {"link boundary", test_link_boundaries_and_clock_rollback},
        {"K1 K2 cycling", test_k1_and_k2_cycle_on_stable_release},
        {"resistor ladder keys", test_resistor_ladder_key_voltage_boundaries},
        {"K1 long page toggle", test_k1_long_hold_toggles_page_once_and_suppresses_previous},
        {"button bounce", test_bounce_does_not_trigger_early_or_repeat},
        {"chord priority", test_chord_takes_priority_and_toggles_after_both_release},
        {"environment K1 returns", test_environment_k1_short_press_returns_to_victim_page},
        {"long hold", test_long_hold_never_repeats_and_release_rearms},
        {"chord hold", test_chord_hold_toggles_once_and_requires_full_release},
    };

    for (size_t i = 0U; i < ARRAY_SIZE(tests); ++i) {
        if (!run_test(tests[i].name, tests[i].fn)) {
            return 1;
        }
    }
    (void)printf("All %zu ground model tests passed.\n", ARRAY_SIZE(tests));
    return 0;
}

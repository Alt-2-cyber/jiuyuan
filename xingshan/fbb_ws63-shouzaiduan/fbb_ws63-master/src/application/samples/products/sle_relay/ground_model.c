#include "ground_model.h"

#include "rescue_route.h"

#include <limits.h>
#include <string.h>

#define GROUND_JSON_BUFFER_CAP 256U

#define GROUND_FIELD_TYPE (1U << 0)
#define GROUND_FIELD_ID (1U << 1)
#define GROUND_FIELD_SPO2 (1U << 2)
#define GROUND_FIELD_HEART_RATE (1U << 3)
#define GROUND_FIELD_BATTERY (1U << 4)
#define GROUND_FIELD_SOS (1U << 5)
#define GROUND_FIELD_SOURCE (1U << 6)
#define GROUND_REQUIRED_FIELDS ((1U << 7) - 1U)

typedef struct {
    const char *text;
    size_t pos;
} ground_json_cursor_t;

typedef struct {
    char type[8];
    char id[7];
    char source[9];
    int64_t spo2;
    int64_t heart_rate;
    int64_t battery;
    int64_t sos;
    uint32_t fields;
} ground_json_report_t;

static void ground_json_skip_space(ground_json_cursor_t *cursor)
{
    while (cursor->text[cursor->pos] == ' ' || cursor->text[cursor->pos] == '\t' ||
        cursor->text[cursor->pos] == '\r' || cursor->text[cursor->pos] == '\n') {
        cursor->pos++;
    }
}

static bool ground_json_take_char(ground_json_cursor_t *cursor, char expected)
{
    ground_json_skip_space(cursor);
    if (cursor->text[cursor->pos] != expected) {
        return false;
    }
    cursor->pos++;
    return true;
}

static bool ground_json_take_string(ground_json_cursor_t *cursor, char *output, size_t output_cap)
{
    size_t output_len = 0U;

    ground_json_skip_space(cursor);
    if (output == NULL || output_cap == 0U || cursor->text[cursor->pos] != '"') {
        return false;
    }
    cursor->pos++;
    while (cursor->text[cursor->pos] != '"') {
        unsigned char value = (unsigned char)cursor->text[cursor->pos];

        if (value == '\0' || value < 0x20U || value == (unsigned char)'\\' ||
            output_len + 1U >= output_cap) {
            return false;
        }
        output[output_len++] = (char)value;
        cursor->pos++;
    }
    cursor->pos++;
    output[output_len] = '\0';
    return true;
}

static bool ground_json_take_integer(ground_json_cursor_t *cursor, int64_t *output)
{
    bool negative = false;
    uint64_t magnitude = 0U;
    uint64_t limit = (uint64_t)INT64_MAX;
    bool has_digit = false;

    ground_json_skip_space(cursor);
    if (cursor->text[cursor->pos] == '-') {
        negative = true;
        limit++;
        cursor->pos++;
    }
    if (cursor->text[cursor->pos] == '0' && cursor->text[cursor->pos + 1U] >= '0' &&
        cursor->text[cursor->pos + 1U] <= '9') {
        return false;
    }
    while (cursor->text[cursor->pos] >= '0' && cursor->text[cursor->pos] <= '9') {
        uint64_t digit = (uint64_t)(cursor->text[cursor->pos] - '0');

        has_digit = true;
        if (magnitude > (limit - digit) / 10U) {
            return false;
        }
        magnitude = magnitude * 10U + digit;
        cursor->pos++;
    }
    if (!has_digit) {
        return false;
    }
    if (negative) {
        *output = magnitude == (uint64_t)INT64_MAX + 1U ? INT64_MIN : -(int64_t)magnitude;
    } else {
        *output = (int64_t)magnitude;
    }
    return true;
}

static bool ground_json_set_once(uint32_t *fields, uint32_t field)
{
    if ((*fields & field) != 0U) {
        return false;
    }
    *fields |= field;
    return true;
}

static bool ground_json_parse_named_value(ground_json_cursor_t *cursor, const char *key,
    ground_json_report_t *parsed)
{
    int64_t ignored;

    if (strcmp(key, "type") == 0) {
        return ground_json_set_once(&parsed->fields, GROUND_FIELD_TYPE) &&
            ground_json_take_string(cursor, parsed->type, sizeof(parsed->type));
    }
    if (strcmp(key, "id") == 0) {
        return ground_json_set_once(&parsed->fields, GROUND_FIELD_ID) &&
            ground_json_take_string(cursor, parsed->id, sizeof(parsed->id));
    }
    if (strcmp(key, "spo2") == 0) {
        return ground_json_set_once(&parsed->fields, GROUND_FIELD_SPO2) &&
            ground_json_take_integer(cursor, &parsed->spo2);
    }
    if (strcmp(key, "hr") == 0) {
        return ground_json_set_once(&parsed->fields, GROUND_FIELD_HEART_RATE) &&
            ground_json_take_integer(cursor, &parsed->heart_rate);
    }
    if (strcmp(key, "battery") == 0) {
        return ground_json_set_once(&parsed->fields, GROUND_FIELD_BATTERY) &&
            ground_json_take_integer(cursor, &parsed->battery);
    }
    if (strcmp(key, "sos") == 0) {
        return ground_json_set_once(&parsed->fields, GROUND_FIELD_SOS) &&
            ground_json_take_integer(cursor, &parsed->sos);
    }
    if (strcmp(key, "source") == 0) {
        return ground_json_set_once(&parsed->fields, GROUND_FIELD_SOURCE) &&
            ground_json_take_string(cursor, parsed->source, sizeof(parsed->source));
    }
    if (strcmp(key, "seq") == 0 || strcmp(key, "sbp") == 0) {
        return ground_json_take_integer(cursor, &ignored);
    }
    return false;
}

static bool ground_json_parse(const char *text, ground_json_report_t *parsed)
{
    ground_json_cursor_t cursor = {text, 0U};

    if (!ground_json_take_char(&cursor, '{')) {
        return false;
    }
    for (;;) {
        char key[16];

        if (!ground_json_take_string(&cursor, key, sizeof(key)) ||
            !ground_json_take_char(&cursor, ':') ||
            !ground_json_parse_named_value(&cursor, key, parsed)) {
            return false;
        }
        ground_json_skip_space(&cursor);
        if (cursor.text[cursor.pos] == '}') {
            cursor.pos++;
            break;
        }
        if (cursor.text[cursor.pos] != ',') {
            return false;
        }
        cursor.pos++;
    }
    ground_json_skip_space(&cursor);
    return cursor.text[cursor.pos] == '\0' && parsed->fields == GROUND_REQUIRED_FIELDS;
}

static bool ground_json_validate(const ground_json_report_t *parsed, uint8_t *victim_index)
{
    if (strcmp(parsed->type, "VICTIM") != 0 || parsed->spo2 < -1 || parsed->spo2 > 100 ||
        parsed->heart_rate < -1 || parsed->heart_rate > 250 || parsed->battery < -1 ||
        parsed->battery > 100 || parsed->sos < 0 || parsed->sos > 1) {
        return false;
    }
    if (strcmp(parsed->id, "VD-001") == 0) {
        *victim_index = 0U;
    } else if (strcmp(parsed->id, "VD-002") == 0) {
        *victim_index = 1U;
    } else if (strcmp(parsed->id, "VD-003") == 0) {
        *victim_index = 2U;
    } else {
        return false;
    }
    return strcmp(parsed->source, "MAX30102") == 0 || strcmp(parsed->source, "SIM") == 0;
}

void ground_model_init(ground_model_t *model)
{
    if (model == NULL) {
        return;
    }
    memset(model, 0, sizeof(*model));
    model->page = GROUND_PAGE_VICTIM;
}

bool ground_model_parse_report(const uint8_t *data, size_t len, uint64_t now_ms,
    ground_report_t *report)
{
    rescue_route_packet_t packet;
    ground_json_report_t parsed = {0};
    ground_report_t staged = {0};
    char body[GROUND_JSON_BUFFER_CAP];

    if (data == NULL || report == NULL || !rescue_route_decode(data, len, &packet) ||
        packet.body_len == 0U || packet.body_len >= sizeof(body) ||
        memchr(packet.body, '\0', packet.body_len) != NULL) {
        return false;
    }
    memcpy(body, packet.body, packet.body_len);
    body[packet.body_len] = '\0';
    if (!ground_json_parse(body, &parsed) || !ground_json_validate(&parsed, &staged.victim_index)) {
        return false;
    }

    staged.victim.valid = true;
    staged.victim.seq = packet.seq;
    staged.victim.spo2 = (int16_t)parsed.spo2;
    staged.victim.heart_rate = (int16_t)parsed.heart_rate;
    staged.victim.battery = (int16_t)parsed.battery;
    staged.victim.sos = parsed.sos == 1;
    staged.victim.route_dm = packet.route_dm;
    staged.victim.hops = packet.hops;
    staged.victim.route_ok = packet.route_ok;
    staged.victim.source_max30102 = strcmp(parsed.source, "MAX30102") == 0;
    staged.victim.source_sim = strcmp(parsed.source, "SIM") == 0;
    staged.victim.last_rx_ms = now_ms;
    *report = staged;
    return true;
}

bool ground_model_apply_report(ground_model_t *model, const ground_report_t *report)
{
    ground_victim_t *slot;

    if (model == NULL || report == NULL || report->victim_index >= GROUND_VICTIM_COUNT ||
        !report->victim.valid) {
        return false;
    }
    slot = &model->victims[report->victim_index];
    if (slot->valid && report->victim.seq <= slot->seq) {
        return false;
    }
    *slot = report->victim;
    return true;
}

ground_link_t ground_model_link(const ground_victim_t *victim, uint64_t now_ms)
{
    if (victim == NULL || !victim->valid) {
        return GROUND_LINK_NONE;
    }
    if (now_ms < victim->last_rx_ms || now_ms - victim->last_rx_ms > GROUND_LINK_LOST_MS) {
        return GROUND_LINK_LOST;
    }
    return GROUND_LINK_OK;
}

ground_key_t ground_model_decode_key_mv(uint16_t millivolts)
{
    if (millivolts < GROUND_KEY_K1_MAX_MV) {
        return GROUND_KEY_K1;
    }
    if (millivolts < GROUND_KEY_K2_MAX_MV) {
        return GROUND_KEY_K2;
    }
    return GROUND_KEY_NONE;
}

static bool ground_button_elapsed(uint64_t now_ms, uint64_t changed_ms)
{
    return now_ms >= changed_ms && now_ms - changed_ms >= GROUND_BUTTON_DEBOUNCE_MS;
}

static void ground_model_finish_gesture(ground_model_t *model)
{
    ground_button_state_t *buttons = &model->buttons;

    if (buttons->long_action_done) {
        /* Long-press action already ran while the key was held. */
    } else if (buttons->chord_seen) {
        model->page = model->page == GROUND_PAGE_VICTIM ? GROUND_PAGE_ENV : GROUND_PAGE_VICTIM;
    } else if (model->page == GROUND_PAGE_ENV &&
        buttons->gesture_k1_seen && !buttons->gesture_k2_seen) {
        model->page = GROUND_PAGE_VICTIM;
    } else if (model->page == GROUND_PAGE_VICTIM &&
        buttons->gesture_k1_seen != buttons->gesture_k2_seen) {
        if (buttons->gesture_k1_seen) {
            model->selected_victim = model->selected_victim == 0U ?
                GROUND_VICTIM_COUNT - 1U : (uint8_t)(model->selected_victim - 1U);
        } else {
            model->selected_victim = (uint8_t)((model->selected_victim + 1U) % GROUND_VICTIM_COUNT);
        }
    }
    buttons->gesture_active = false;
    buttons->long_action_done = false;
    buttons->gesture_k1_seen = false;
    buttons->gesture_k2_seen = false;
    buttons->chord_seen = false;
}

void ground_model_sample_buttons(ground_model_t *model, bool k1_pressed, bool k2_pressed,
    uint64_t now_ms)
{
    ground_button_state_t *buttons;

    if (model == NULL) {
        return;
    }
    buttons = &model->buttons;
    if (buttons->raw_k1_pressed != k1_pressed) {
        buttons->raw_k1_pressed = k1_pressed;
        buttons->k1_raw_changed_ms = now_ms;
    }
    if (buttons->raw_k2_pressed != k2_pressed) {
        buttons->raw_k2_pressed = k2_pressed;
        buttons->k2_raw_changed_ms = now_ms;
    }
    if (buttons->stable_k1_pressed != buttons->raw_k1_pressed &&
        ground_button_elapsed(now_ms, buttons->k1_raw_changed_ms)) {
        buttons->stable_k1_pressed = buttons->raw_k1_pressed;
    }
    if (buttons->stable_k2_pressed != buttons->raw_k2_pressed &&
        ground_button_elapsed(now_ms, buttons->k2_raw_changed_ms)) {
        buttons->stable_k2_pressed = buttons->raw_k2_pressed;
    }

    if (buttons->stable_k1_pressed || buttons->stable_k2_pressed) {
        if (!buttons->gesture_active) {
            buttons->gesture_active = true;
            buttons->gesture_started_ms = now_ms;
        }
        buttons->gesture_k1_seen = buttons->gesture_k1_seen || buttons->stable_k1_pressed;
        buttons->gesture_k2_seen = buttons->gesture_k2_seen || buttons->stable_k2_pressed;
        buttons->chord_seen = buttons->chord_seen ||
            (buttons->stable_k1_pressed && buttons->stable_k2_pressed);
        if (!buttons->long_action_done && !buttons->chord_seen &&
            buttons->stable_k1_pressed && !buttons->stable_k2_pressed &&
            ground_button_elapsed(now_ms, buttons->gesture_started_ms) &&
            now_ms - buttons->gesture_started_ms >= GROUND_BUTTON_LONG_PRESS_MS) {
            model->page = model->page == GROUND_PAGE_VICTIM ? GROUND_PAGE_ENV : GROUND_PAGE_VICTIM;
            buttons->long_action_done = true;
        }
    } else if (buttons->gesture_active) {
        ground_model_finish_gesture(model);
    }
}

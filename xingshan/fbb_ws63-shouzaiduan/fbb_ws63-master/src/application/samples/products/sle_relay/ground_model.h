#ifndef GROUND_MODEL_H
#define GROUND_MODEL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GROUND_VICTIM_COUNT 3U
#define GROUND_BUTTON_DEBOUNCE_MS 30U
#define GROUND_BUTTON_LONG_PRESS_MS 800U
#define GROUND_KEY_K1_MAX_MV 750U
#define GROUND_KEY_K2_MAX_MV 1400U
#define GROUND_LINK_LOST_MS 6000U

typedef enum ground_page_t {
    GROUND_PAGE_VICTIM = 0,
    GROUND_PAGE_ENV,
} ground_page_t;

typedef enum ground_link_t {
    GROUND_LINK_NONE = 0,
    GROUND_LINK_OK,
    GROUND_LINK_LOST,
} ground_link_t;

typedef enum ground_key_t {
    GROUND_KEY_NONE = 0,
    GROUND_KEY_K1,
    GROUND_KEY_K2,
} ground_key_t;

typedef struct {
    bool valid;
    uint32_t seq;
    int16_t spo2;
    int16_t heart_rate;
    int16_t battery;
    bool sos;
    uint32_t route_dm;
    uint8_t hops;
    bool route_ok;
    bool source_max30102;
    bool source_sim;
    uint64_t last_rx_ms;
} ground_victim_t;

typedef struct {
    bool raw_k1_pressed;
    bool raw_k2_pressed;
    bool stable_k1_pressed;
    bool stable_k2_pressed;
    uint64_t k1_raw_changed_ms;
    uint64_t k2_raw_changed_ms;
    bool gesture_active;
    bool long_action_done;
    bool gesture_k1_seen;
    bool gesture_k2_seen;
    bool chord_seen;
    uint64_t gesture_started_ms;
} ground_button_state_t;

typedef struct {
    ground_victim_t victims[GROUND_VICTIM_COUNT];
    uint8_t selected_victim;
    ground_page_t page;
    ground_button_state_t buttons;
} ground_model_t;

typedef struct {
    uint8_t victim_index;
    ground_victim_t victim;
} ground_report_t;

void ground_model_init(ground_model_t *model);
bool ground_model_parse_report(const uint8_t *data, size_t len, uint64_t now_ms,
    ground_report_t *report);
bool ground_model_apply_report(ground_model_t *model, const ground_report_t *report);
ground_link_t ground_model_link(const ground_victim_t *victim, uint64_t now_ms);
ground_key_t ground_model_decode_key_mv(uint16_t millivolts);
void ground_model_sample_buttons(ground_model_t *model, bool k1_pressed, bool k2_pressed,
    uint64_t now_ms);

#ifdef __cplusplus
}
#endif

#endif

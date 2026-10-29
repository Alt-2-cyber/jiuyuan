#ifndef GROUND_UI_H
#define GROUND_UI_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int ground_ui_init(void);
int ground_ui_start(void);
void ground_ui_ingest_report(const uint8_t *data, uint16_t len);

#ifdef __cplusplus
}
#endif

#endif

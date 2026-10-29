#ifndef MAX30102_H
#define MAX30102_H

#include <stdint.h>
#include "i2c.h"

#ifdef __cplusplus
extern "C" {
#endif

int max30102_init(i2c_bus_t bus);
int max30102_update(void);
int max30102_get_latest(int *spo2, int *heart_rate, int *finger_present);
int max30102_is_ready(void);

#ifdef __cplusplus
}
#endif

#endif

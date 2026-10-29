#ifndef RESCUE_AHT20_H
#define RESCUE_AHT20_H

#include <stdint.h>

uint32_t AHT20_Calibrate(void);
uint32_t AHT20_StartMeasure(void);
uint32_t AHT20_GetMeasureResult(float *temperature, float *humidity);

#endif

#ifndef RESCUE_CLOUD_H
#define RESCUE_CLOUD_H

#include <stdint.h>

void rescue_cloud_start(const char *role);
int rescue_cloud_publish_sle_payload(const uint8_t *payload, uint16_t payload_len);
int rescue_cloud_publish_victim_state(uint32_t seq, int spo2, int heart_rate, int systolic, int battery, int sos);

#endif

#ifndef RESCUE_ROUTE_H
#define RESCUE_ROUTE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RESCUE_ROUTE_WIRE_CAP 256U

typedef struct {
    uint8_t src;
    uint8_t dst;
    uint8_t ttl;
    uint8_t hops;
    uint32_t seq;
    uint32_t route_dm;
    bool route_ok;
    int16_t last_rssi;
    /* Borrowed read-only view into the input wire. Encode or copy it before that storage changes or expires. */
    const uint8_t *body;
    size_t body_len;
} rescue_route_packet_t;

bool rescue_route_decode(const uint8_t *wire, size_t wire_len, rescue_route_packet_t *packet);
bool rescue_route_encode(const rescue_route_packet_t *packet, uint8_t *wire, size_t wire_cap, size_t *wire_len);
uint16_t rescue_route_rssi_to_dm(int16_t rssi);
bool rescue_route_add_hop(rescue_route_packet_t *packet, int16_t rssi);

#ifdef __cplusplus
}
#endif

#endif

package com.starflash.rescue.model;

import java.time.LocalDateTime;

public record Device(
    String id,
    String name,
    String type,
    String ownerId,
    DeviceStatus status,
    int battery,
    String firmwareVersion,
    String linkQuality,
    LocalDateTime lastSeenAt
) {
}

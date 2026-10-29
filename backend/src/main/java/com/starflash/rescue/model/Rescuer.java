package com.starflash.rescue.model;

import java.time.LocalDateTime;

public record Rescuer(
    String id,
    String name,
    String team,
    String deviceId,
    DeviceStatus status,
    LocationPoint location,
    EnvironmentSample environment,
    String currentTaskId,
    String targetVictimId,
    int battery,
    LocalDateTime lastSeenAt
) {
}

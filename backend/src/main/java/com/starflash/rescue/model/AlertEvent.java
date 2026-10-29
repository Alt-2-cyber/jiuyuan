package com.starflash.rescue.model;

import java.time.LocalDateTime;

public record AlertEvent(
    String id,
    String type,
    RiskLevel severity,
    String title,
    String detail,
    String personId,
    String deviceId,
    String status,
    LocalDateTime firstSeenAt,
    LocalDateTime lastSeenAt
) {
}

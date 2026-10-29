package com.starflash.rescue.model;

import java.time.LocalDateTime;

public record RescueTask(
    String id,
    String victimId,
    String rescuerId,
    TaskStatus status,
    RiskLevel priority,
    double priorityScore,
    String reason,
    LocalDateTime createdAt,
    LocalDateTime assignedAt,
    LocalDateTime completedAt
) {
}

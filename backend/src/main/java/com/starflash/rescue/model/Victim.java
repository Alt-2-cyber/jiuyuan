package com.starflash.rescue.model;

import java.time.LocalDateTime;
import java.util.List;

public record Victim(
    String id,
    String name,
    int age,
    String gender,
    String deviceId,
    RiskLevel level,
    double priorityScore,
    VitalSigns vitals,
    LocationPoint location,
    String assignedRescuerId,
    String taskId,
    String status,
    List<String> reasons,
    LocalDateTime lastSeenAt
) {
}

package com.starflash.rescue.model;

import java.time.LocalDateTime;
import java.util.List;

public record RoutePlan(
    String id,
    String rescuerId,
    String rescuerName,
    String victimId,
    String victimName,
    boolean realVictimData,
    double totalDistance,
    int estimatedMinutes,
    RiskLevel priority,
    String strategy,
    List<String> warnings,
    List<RouteWaypoint> waypoints,
    LocalDateTime generatedAt
) {
}

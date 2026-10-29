package com.starflash.rescue.model;

public record RouteWaypoint(
    double x,
    double y,
    String label,
    String type,
    RiskLevel risk
) {
}

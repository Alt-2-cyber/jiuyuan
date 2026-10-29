package com.starflash.rescue.model;

public record MetricCard(
    String label,
    String value,
    String unit,
    String trend,
    RiskLevel level
) {
}

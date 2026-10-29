package com.starflash.rescue.model;

public record EnvironmentSample(
    double temperature,
    double humidity,
    double combustibleGas,
    String combustibleUnit,
    double co,
    double oxygen,
    RiskLevel riskLevel
) {
}

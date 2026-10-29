package com.starflash.rescue.model;

public record VitalSigns(
    int spo2,
    int heartRate,
    int systolic,
    int diastolic,
    double temperature,
    String motionState,
    double signalQuality,
    int battery
) {
}

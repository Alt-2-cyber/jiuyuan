package com.starflash.rescue.model;

public record TrendPoint(
    String time,
    int spo2,
    int heartRate,
    double gas,
    int alerts
) {
}

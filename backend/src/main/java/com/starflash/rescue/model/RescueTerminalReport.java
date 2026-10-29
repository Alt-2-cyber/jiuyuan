package com.starflash.rescue.model;

import jakarta.validation.constraints.NotBlank;

public record RescueTerminalReport(
    @NotBlank String victimDeviceId,
    String rescuerDeviceId,
    Integer spo2,
    Integer heartRate,
    Integer systolic,
    Integer battery,
    Integer rssi,
    Double distance,
    String trend,
    Boolean sos
) {
}

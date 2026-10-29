package com.starflash.rescue.model;

import java.time.LocalDateTime;
import java.util.Map;

public record CloudVictimSample(
    Long id,
    String deviceId,
    String serviceId,
    LocalDateTime eventTime,
    Integer seq,
    Integer spo2,
    Integer heartRate,
    Integer systolic,
    Integer battery,
    Boolean sos,
    String source,
    Map<String, Object> raw,
    LocalDateTime createdAt
) {
}

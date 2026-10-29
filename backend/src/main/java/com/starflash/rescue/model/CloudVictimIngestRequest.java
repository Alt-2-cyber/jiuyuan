package com.starflash.rescue.model;

import java.util.Map;

public record CloudVictimIngestRequest(
    String deviceId,
    String serviceId,
    String eventTime,
    Integer seq,
    Integer spo2,
    Integer hr,
    Integer heartRate,
    Integer sbp,
    Integer systolic,
    Integer battery,
    Boolean sos,
    Map<String, Object> raw
) {
}

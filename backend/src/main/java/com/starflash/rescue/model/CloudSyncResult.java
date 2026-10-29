package com.starflash.rescue.model;

public record CloudSyncResult(
    boolean success,
    String message,
    Integer inserted,
    CloudVictimSample latest
) {
}

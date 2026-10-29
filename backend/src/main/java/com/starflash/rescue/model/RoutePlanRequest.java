package com.starflash.rescue.model;

public record RoutePlanRequest(
    String victimId,
    String rescuerId
) {
}

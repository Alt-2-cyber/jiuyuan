package com.starflash.rescue.model;

public record LocationPoint(
    double x,
    double y,
    double z,
    double distanceToNearestRescuer,
    double accuracy,
    double azimuth
) {
}

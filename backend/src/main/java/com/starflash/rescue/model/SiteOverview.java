package com.starflash.rescue.model;

import java.time.LocalDateTime;
import java.util.List;
import java.util.Map;

public record SiteOverview(
    String siteId,
    String siteName,
    String phase,
    LocalDateTime serverTime,
    List<MetricCard> metrics,
    Map<RiskLevel, Long> riskDistribution,
    List<TrendPoint> trends,
    List<Victim> priorityQueue,
    List<AlertEvent> latestAlerts,
    List<Rescuer> activeRescuers
) {
}

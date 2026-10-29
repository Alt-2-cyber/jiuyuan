package com.starflash.rescue.service;

import com.starflash.rescue.model.AlertEvent;
import com.starflash.rescue.model.Device;
import com.starflash.rescue.model.DeviceStatus;
import com.starflash.rescue.model.EnvironmentSample;
import com.starflash.rescue.model.LocationPoint;
import com.starflash.rescue.model.MetricCard;
import com.starflash.rescue.model.RescueTask;
import com.starflash.rescue.model.RescueTerminalReport;
import com.starflash.rescue.model.Rescuer;
import com.starflash.rescue.model.RiskLevel;
import com.starflash.rescue.model.SiteOverview;
import com.starflash.rescue.model.TaskStatus;
import com.starflash.rescue.model.TrendPoint;
import com.starflash.rescue.model.Victim;
import com.starflash.rescue.model.VitalSigns;
import com.starflash.rescue.model.CloudVictimSample;
import org.springframework.beans.factory.annotation.Value;
import org.springframework.stereotype.Service;

import java.time.LocalDateTime;
import java.time.format.DateTimeFormatter;
import java.util.ArrayList;
import java.util.Comparator;
import java.util.EnumMap;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Optional;
import java.util.concurrent.ConcurrentHashMap;

@Service
public class RescueDataService {
    private final String siteId;
    private final String siteName;
    private final CloudVictimSampleRepository cloudVictimSampleRepository;
    private final Map<String, Victim> victims = new ConcurrentHashMap<>();
    private final Map<String, Rescuer> rescuers = new ConcurrentHashMap<>();
    private final Map<String, Device> devices = new ConcurrentHashMap<>();
    private final Map<String, AlertEvent> alerts = new ConcurrentHashMap<>();
    private final Map<String, RescueTask> tasks = new ConcurrentHashMap<>();
    private final List<TrendPoint> trends = new ArrayList<>();

    public RescueDataService(
        @Value("${rescue.site-id}") String siteId,
        @Value("${rescue.site-name}") String siteName,
        CloudVictimSampleRepository cloudVictimSampleRepository
    ) {
        this.siteId = siteId;
        this.siteName = siteName;
        this.cloudVictimSampleRepository = cloudVictimSampleRepository;
        seed();
    }

    public SiteOverview overview() {
        List<Victim> sortedVictims = victims();
        List<AlertEvent> latestAlerts = alerts().stream().limit(6).toList();
        List<Rescuer> activeRescuers = rescuers().stream()
            .filter(rescuer -> rescuer.status() != DeviceStatus.OFFLINE)
            .toList();

        long red = sortedVictims.stream().filter(v -> v.level() == RiskLevel.RED).count();
        long orange = sortedVictims.stream().filter(v -> v.level() == RiskLevel.ORANGE).count();
        long activeAlertCount = alerts.values().stream().filter(a -> !"RESOLVED".equals(a.status())).count();
        long onlineDevices = devices.values().stream().filter(d -> d.status() != DeviceStatus.OFFLINE).count();

        List<MetricCard> metrics = List.of(
            new MetricCard("受灾人员", String.valueOf(sortedVictims.size()), "人", "真实样本优先", red > 0 ? RiskLevel.RED : RiskLevel.GREEN),
            new MetricCard("高危待救", String.valueOf(red + orange), "人", "红色 " + red + " / 橙色 " + orange, red > 0 ? RiskLevel.RED : RiskLevel.ORANGE),
            new MetricCard("在线设备", String.valueOf(onlineDevices), "台", "链路稳定率 94%", RiskLevel.GREEN),
            new MetricCard("活跃告警", String.valueOf(activeAlertCount), "条", "2 条环境风险", activeAlertCount > 4 ? RiskLevel.RED : RiskLevel.YELLOW)
        );

        Map<RiskLevel, Long> distribution = new EnumMap<>(RiskLevel.class);
        for (RiskLevel level : RiskLevel.values()) {
            distribution.put(level, sortedVictims.stream().filter(v -> v.level() == level).count());
        }

        return new SiteOverview(
            siteId,
            siteName,
            "72 小时黄金救援 / 第 18 小时",
            LocalDateTime.now(),
            metrics,
            distribution,
            trends,
            sortedVictims.stream().limit(8).toList(),
            latestAlerts,
            activeRescuers
        );
    }

    public List<Victim> victims() {
        return currentVictims().stream()
            .sorted(Comparator.comparing(Victim::priorityScore).reversed())
            .toList();
    }

    public Optional<Victim> victim(String id) {
        return currentVictims().stream()
            .filter(victim -> victim.id().equals(id))
            .findFirst();
    }

    public List<Rescuer> rescuers() {
        return rescuers.values().stream()
            .sorted(Comparator.comparing(Rescuer::id))
            .toList();
    }

    public List<Device> devices() {
        return devices.values().stream()
            .sorted(Comparator.comparing(Device::status).thenComparing(Device::id))
            .toList();
    }

    public List<AlertEvent> alerts() {
        return alerts.values().stream()
            .sorted(Comparator.comparing(AlertEvent::lastSeenAt).reversed())
            .toList();
    }

    public List<RescueTask> tasks() {
        return currentTasks().stream()
            .sorted(Comparator.comparing(RescueTask::priorityScore).reversed())
            .toList();
    }

    public Optional<RescueTask> assignTask(String taskId, String rescuerId) {
        RescueTask current = tasks.get(taskId);
        Rescuer rescuer = rescuers.get(rescuerId);
        if (current == null || rescuer == null) {
            return Optional.empty();
        }

        RescueTask updatedTask = new RescueTask(
            current.id(),
            current.victimId(),
            rescuerId,
            TaskStatus.ASSIGNED,
            current.priority(),
            current.priorityScore(),
            current.reason(),
            current.createdAt(),
            LocalDateTime.now(),
            current.completedAt()
        );
        tasks.put(taskId, updatedTask);

        Victim victim = victims.get(current.victimId());
        if (victim != null) {
            victims.put(victim.id(), new Victim(
                victim.id(),
                victim.name(),
                victim.age(),
                victim.gender(),
                victim.deviceId(),
                victim.level(),
                victim.priorityScore(),
                victim.vitals(),
                victim.location(),
                rescuerId,
                taskId,
                "已派发",
                victim.reasons(),
                victim.lastSeenAt()
            ));
        }

        rescuers.put(rescuer.id(), new Rescuer(
            rescuer.id(),
            rescuer.name(),
            rescuer.team(),
            rescuer.deviceId(),
            rescuer.status(),
            rescuer.location(),
            rescuer.environment(),
            taskId,
            current.victimId(),
            rescuer.battery(),
            LocalDateTime.now()
        ));

        return Optional.of(updatedTask);
    }

    public Map<String, Object> ingestRescueTerminal(RescueTerminalReport report) {
        Victim matched = victims.values().stream()
            .filter(victim -> victim.deviceId().equals(report.victimDeviceId()))
            .findFirst()
            .orElse(null);
        if (matched == null) {
            return Map.of(
                "accepted", false,
                "reason", "victim device not found",
                "victimDeviceId", report.victimDeviceId()
            );
        }

        VitalSigns oldVitals = matched.vitals();
        VitalSigns newVitals = new VitalSigns(
            report.spo2() == null ? oldVitals.spo2() : report.spo2(),
            report.heartRate() == null ? oldVitals.heartRate() : report.heartRate(),
            report.systolic() == null ? oldVitals.systolic() : report.systolic(),
            oldVitals.diastolic(),
            oldVitals.temperature(),
            Boolean.TRUE.equals(report.sos()) ? "SOS" : oldVitals.motionState(),
            oldVitals.signalQuality(),
            report.battery() == null ? oldVitals.battery() : report.battery()
        );

        LocationPoint oldLocation = matched.location();
        LocationPoint newLocation = new LocationPoint(
            oldLocation.x(),
            oldLocation.y(),
            oldLocation.z(),
            report.distance() == null ? oldLocation.distanceToNearestRescuer() : report.distance(),
            oldLocation.accuracy(),
            oldLocation.azimuth()
        );

        List<String> reasons = new ArrayList<>(matched.reasons());
        if (report.trend() != null) {
            reasons.add("救援端趋势: " + report.trend());
        }
        if (report.rssi() != null) {
            reasons.add("RSSI: " + report.rssi() + " dBm");
        }

        victims.put(matched.id(), new Victim(
            matched.id(),
            matched.name(),
            matched.age(),
            matched.gender(),
            matched.deviceId(),
            matched.level(),
            matched.priorityScore(),
            newVitals,
            newLocation,
            matched.assignedRescuerId(),
            matched.taskId(),
            Boolean.TRUE.equals(report.sos()) ? "SOS 上报" : "串口上报",
            reasons,
            LocalDateTime.now()
        ));

        return Map.of(
            "accepted", true,
            "victimId", matched.id(),
            "victimDeviceId", matched.deviceId(),
            "storedTables", List.of("vital_sample", "location_sample"),
            "mode", "SERIAL_OR_GATEWAY_INGEST"
        );
    }

    public Map<String, Object> mapPayload() {
        Map<String, Object> payload = new LinkedHashMap<>();
        payload.put("siteId", siteId);
        payload.put("name", "地下商场 A 区 - 负二层局部坍塌");
        payload.put("width", 120);
        payload.put("height", 76);
        payload.put("zones", List.of(
            Map.of("name", "A1 候检区", "x", 8, "y", 8, "w", 32, "h", 22, "risk", "YELLOW"),
            Map.of("name", "A2 商铺走廊", "x", 44, "y", 8, "w", 30, "h", 22, "risk", "RED"),
            Map.of("name", "B1 设备间", "x", 78, "y", 8, "w", 34, "h", 22, "risk", "ORANGE"),
            Map.of("name", "C1 停车区", "x", 8, "y", 38, "w", 48, "h", 28, "risk", "GREEN"),
            Map.of("name", "C2 管廊", "x", 62, "y", 38, "w", 50, "h", 28, "risk", "YELLOW")
        ));
        payload.put("victims", victims());
        payload.put("rescuers", rescuers());
        return payload;
    }

    private List<Victim> currentVictims() {
        List<Victim> result = new ArrayList<>(victims.values());
        latestRealSample().ifPresent(sample -> mergeRealVictimSample(result, sample));
        return result;
    }

    private List<RescueTask> currentTasks() {
        List<RescueTask> result = new ArrayList<>(tasks.values());
        latestRealSample().ifPresent(sample -> {
            Victim realVictim = realVictimFromSample(sample, null);
            if (result.stream().noneMatch(task -> task.victimId().equals(realVictim.id()))) {
                result.add(new RescueTask(
                    "T-REAL-001",
                    realVictim.id(),
                    null,
                    TaskStatus.PENDING,
                    realVictim.level(),
                    realVictim.priorityScore(),
                    "真实受灾端云端上报，优先按最新心率血氧和救援端距离规划",
                    sample.eventTime(),
                    null,
                    null
                ));
            }
        });
        return result;
    }

    private Optional<CloudVictimSample> latestRealSample() {
        try {
            List<CloudVictimSample> samples = cloudVictimSampleRepository.findLatest(80);
            return samples.stream()
                .filter(this::isVictimSample)
                .filter(this::isHuaweiShadowSample)
                .filter(this::hasUsefulSample)
                .findFirst()
                .or(() -> samples.stream()
                    .filter(this::isVictimSample)
                    .filter(this::hasValidVitals)
                    .findFirst())
                .or(() -> samples.stream()
                    .filter(this::isVictimSample)
                    .filter(this::hasUsefulSample)
                    .findFirst())
                .or(() -> samples.stream().filter(this::isVictimSample).findFirst());
        } catch (Exception ignored) {
            return Optional.empty();
        }
    }

    private boolean isHuaweiShadowSample(CloudVictimSample sample) {
        return "HUAWEI_SHADOW".equals(sample.source());
    }

    private boolean isVictimSample(CloudVictimSample sample) {
        return sample != null
            && (isVictimSample(sample.deviceId())
            || textRaw(sample.raw(), "victim_id", "victimId", "target_id") != null
            || isVictimSample(textRaw(sample.raw(), "_cloud_device_id", "cloud_device_id")));
    }

    private boolean hasUsefulSample(CloudVictimSample sample) {
        return isValidSpo2(sample.spo2())
            || isValidHeartRate(sample.heartRate())
            || sample.sos() != null
            || numericRaw(sample.raw(), "distance", "dist", "range", "distance_m", "distance_dm") != null;
    }

    private boolean hasValidVitals(CloudVictimSample sample) {
        return isValidSpo2(sample.spo2()) || isValidHeartRate(sample.heartRate());
    }

    private boolean isVictimSample(String deviceId) {
        return deviceId != null && (deviceId.contains("shouzai") || deviceId.contains("victim") || deviceId.contains("victim_state"));
    }

    private void mergeRealVictimSample(List<Victim> result, CloudVictimSample sample) {
        String sampleDeviceId = firstText(textRaw(sample.raw(), "victim_id", "victimId", "id"), sample.deviceId());
        for (int i = 0; i < result.size(); i++) {
            Victim existing = result.get(i);
            if (existing.deviceId().equals(sampleDeviceId) || existing.id().equals(sampleDeviceId)) {
                result.set(i, realVictimFromSample(sample, existing));
                return;
            }
        }
        result.add(realVictimFromSample(sample, null));
    }

    private Victim realVictimFromSample(CloudVictimSample sample, Victim base) {
        Integer spo2 = isValidSpo2(sample.spo2()) ? sample.spo2() : null;
        Integer heartRate = isValidHeartRate(sample.heartRate()) ? sample.heartRate() : null;
        Integer systolic = sample.systolic() != null && sample.systolic() > 40 ? sample.systolic() : null;
        Integer battery = sample.battery() != null && sample.battery() >= 0 ? sample.battery() : null;
        Double distance = numericRaw(sample.raw(), "distance", "dist", "range", "distance_m", "distance_dm");
        Double rssi = numericRaw(sample.raw(), "rssi", "RSSI");

        RiskLevel level = riskFromSample(sample, spo2, heartRate);
        double priorityScore = priorityScore(level, spo2, heartRate, Boolean.TRUE.equals(sample.sos()), distance);
        LocationPoint location = locationFromSample(base, distance, rssi);
        VitalSigns oldVitals = base == null ? null : base.vitals();
        VitalSigns vitals = new VitalSigns(
            spo2 == null ? 0 : spo2,
            heartRate == null ? 0 : heartRate,
            systolic == null ? 0 : systolic,
            oldVitals == null ? 0 : oldVitals.diastolic(),
            oldVitals == null ? 0 : oldVitals.temperature(),
            Boolean.TRUE.equals(sample.sos()) ? "SOS" : hasUsefulVitals(spo2, heartRate) ? "真实上报" : "传感器未稳定",
            signalQuality(rssi),
            battery == null ? 0 : battery
        );

        String victimId = base == null ? "V-REAL" : base.id();
        String victimName = base == null ? "受灾端实时上报" : base.name();
        String victimDeviceId = firstText(textRaw(sample.raw(), "victim_id", "victimId", "id"), sample.deviceId(), "UNKNOWN");
        String cloudDeviceId = textRaw(sample.raw(), "_cloud_device_id", "cloud_device_id");
        List<String> reasons = new ArrayList<>();
        reasons.add("真实云端样本 seq=" + (sample.seq() == null ? "--" : sample.seq()));
        reasons.add("来源 " + sample.source() + " / " + sample.serviceId());
        if (cloudDeviceId != null) {
            reasons.add("华为云设备 " + cloudDeviceId);
        }
        if (spo2 != null) {
            reasons.add("血氧 " + spo2 + "%");
        }
        if (heartRate != null) {
            reasons.add("心率 " + heartRate + " bpm");
        }
        if (distance != null) {
            reasons.add("救援端估算距离 " + String.format("%.1f", distance) + "m");
        }
        if (rssi != null) {
            reasons.add("RSSI " + String.format("%.0f", rssi) + " dBm");
        }

        return new Victim(
            victimId,
            victimName,
            base == null ? 0 : base.age(),
            base == null ? "--" : base.gender(),
            victimDeviceId,
            level,
            priorityScore,
            vitals,
            location,
            base == null ? null : base.assignedRescuerId(),
            base == null ? "T-REAL-001" : base.taskId(),
            "真实云端上报",
            reasons,
            sample.eventTime()
        );
    }

    private LocationPoint locationFromSample(Victim base, Double distance, Double rssi) {
        if (base != null) {
            LocationPoint old = base.location();
            return new LocationPoint(
                old.x(),
                old.y(),
                old.z(),
                distance == null ? old.distanceToNearestRescuer() : distance,
                distance == null ? old.accuracy() : Math.max(2.0, Math.min(8.0, distance * 0.18)),
                old.azimuth()
            );
        }

        Rescuer anchor = rescuers.values().stream()
            .filter(rescuer -> rescuer.status() != DeviceStatus.OFFLINE)
            .min(Comparator.comparing(rescuer -> rescuer.location().distanceToNearestRescuer()))
            .orElse(null);
        double baseX = anchor == null ? 24 : anchor.location().x();
        double baseY = anchor == null ? 48 : anchor.location().y();
        double meters = distance == null ? estimateDistanceFromRssi(rssi) : distance;
        double angle = Math.toRadians(35);
        double x = clamp(baseX + Math.cos(angle) * Math.min(meters, 34), 4, 116);
        double y = clamp(baseY - Math.sin(angle) * Math.min(meters, 34), 4, 72);
        return new LocationPoint(x, y, 0, meters, Math.max(2.0, Math.min(8.0, meters * 0.18)), 35);
    }

    private RiskLevel riskFromSample(CloudVictimSample sample, Integer spo2, Integer heartRate) {
        if (Boolean.TRUE.equals(sample.sos())) {
            return RiskLevel.RED;
        }
        if (spo2 != null && spo2 < 88) {
            return RiskLevel.RED;
        }
        if (heartRate != null && (heartRate > 135 || heartRate < 45)) {
            return RiskLevel.RED;
        }
        if (spo2 != null && spo2 < 92) {
            return RiskLevel.ORANGE;
        }
        if (heartRate != null && (heartRate > 120 || heartRate < 55)) {
            return RiskLevel.ORANGE;
        }
        if (spo2 == null && heartRate == null) {
            return RiskLevel.GRAY;
        }
        return RiskLevel.GREEN;
    }

    private double priorityScore(RiskLevel level, Integer spo2, Integer heartRate, boolean sos, Double distance) {
        double score = switch (level) {
            case RED -> 90;
            case ORANGE -> 75;
            case YELLOW -> 55;
            case GREEN -> 35;
            case GRAY -> 45;
        };
        if (sos) {
            score += 8;
        }
        if (spo2 != null && spo2 < 90) {
            score += (90 - spo2) * 1.2;
        }
        if (heartRate != null && heartRate > 120) {
            score += Math.min(8, (heartRate - 120) * 0.18);
        }
        if (distance != null && distance < 15) {
            score += 3;
        }
        return Math.min(99.9, score);
    }

    private boolean isValidSpo2(Integer value) {
        return value != null && value >= 50 && value <= 100;
    }

    private boolean isValidHeartRate(Integer value) {
        return value != null && value >= 30 && value <= 220;
    }

    private boolean hasUsefulVitals(Integer spo2, Integer heartRate) {
        return spo2 != null || heartRate != null;
    }

    private double signalQuality(Double rssi) {
        if (rssi == null) {
            return 0.72;
        }
        return clamp((rssi + 95) / 50, 0.08, 0.98);
    }

    private double estimateDistanceFromRssi(Double rssi) {
        if (rssi == null) {
            return 18.0;
        }
        return clamp(Math.pow(10.0, (-55.0 - rssi) / 22.0), 2.0, 42.0);
    }

    private Double numericRaw(Map<String, Object> raw, String... keys) {
        if (raw == null) {
            return null;
        }
        for (String key : keys) {
            Object value = raw.get(key);
            if (value instanceof Number number) {
                return number.doubleValue();
            }
            if (value instanceof String text && !text.isBlank()) {
                try {
                    return Double.parseDouble(text.trim());
                } catch (NumberFormatException ignored) {
                    return null;
                }
            }
        }
        return null;
    }

    private String textRaw(Map<String, Object> raw, String... keys) {
        if (raw == null) {
            return null;
        }
        for (String key : keys) {
            Object value = raw.get(key);
            if (value instanceof String text && !text.isBlank()) {
                return text.trim();
            }
            if (value instanceof Number number) {
                return String.valueOf(number);
            }
        }
        return null;
    }

    private String firstText(String... values) {
        for (String value : values) {
            if (value != null && !value.isBlank()) {
                return value;
            }
        }
        return "";
    }

    private double clamp(double value, double min, double max) {
        return Math.max(min, Math.min(max, value));
    }

    private void seed() {
        LocalDateTime now = LocalDateTime.now();
        addRescuers(now);
        addVictims(now);
        addDevices(now);
        addAlerts(now);
        addTasks(now);
        addTrends(now);
    }

    private void addRescuers(LocalDateTime now) {
        addRescuer(new Rescuer(
            "R-001", "陈序", "突入一组", "RD-001", DeviceStatus.ONLINE,
            new LocationPoint(24, 48, 0, 0, 1.8, 42),
            new EnvironmentSample(39.5, 76, 6.2, "%LEL", 18, 20.5, RiskLevel.GREEN),
            "T-0001", "V-003", 84, now.minusSeconds(8)
        ));
        addRescuer(new Rescuer(
            "R-002", "林骁", "突入一组", "RD-002", DeviceStatus.WARNING,
            new LocationPoint(64, 24, 0, 0, 2.3, 274),
            new EnvironmentSample(47.8, 88, 18.4, "%LEL", 42, 20.1, RiskLevel.ORANGE),
            "T-0002", "V-001", 67, now.minusSeconds(14)
        ));
        addRescuer(new Rescuer(
            "R-003", "周宁", "医疗转运组", "RD-003", DeviceStatus.ONLINE,
            new LocationPoint(18, 18, 0, 0, 1.5, 126),
            new EnvironmentSample(36.2, 72, 4.1, "%LEL", 12, 20.8, RiskLevel.GREEN),
            null, null, 91, now.minusSeconds(21)
        ));
        addRescuer(new Rescuer(
            "R-004", "邵远", "侦检组", "RD-004", DeviceStatus.ONLINE,
            new LocationPoint(92, 50, 0, 0, 2.7, 315),
            new EnvironmentSample(55.1, 90, 22.8, "%LEL", 64, 19.7, RiskLevel.RED),
            "T-0004", "V-006", 58, now.minusSeconds(5)
        ));
    }

    private void addVictims(LocalDateTime now) {
        addVictim(new Victim(
            "V-001", "受灾者 01", 62, "男", "VD-001", RiskLevel.RED, 94.5,
            new VitalSigns(84, 146, 88, 54, 36.2, "长时间静止", 0.78, 71),
            new LocationPoint(70, 20, 0, 18.4, 3.2, 38),
            "R-002", "T-0002", "已派发",
            List.of("SpO2 84% 严重偏低", "心率 146 bpm", "收缩压 88 mmHg", "距离 R-002 18.4m"),
            now.minusSeconds(18)
        ));
        addVictim(new Victim(
            "V-002", "受灾者 02", 34, "女", "VD-002", RiskLevel.ORANGE, 78.2,
            new VitalSigns(89, 122, 96, 61, 37.1, "微弱活动", 0.74, 63),
            new LocationPoint(48, 18, 0, 28.2, 2.8, 19),
            null, "T-0003", "待派发",
            List.of("SpO2 89% 偏低", "疑似受困但仍有微弱活动", "定位质量中等"),
            now.minusSeconds(34)
        ));
        addVictim(new Victim(
            "V-003", "受灾者 03", 27, "男", "VD-003", RiskLevel.RED, 91.0,
            new VitalSigns(86, 138, 92, 58, 38.5, "SOS", 0.86, 82),
            new LocationPoint(34, 46, 0, 11.7, 1.9, 344),
            "R-001", "T-0001", "救援中",
            List.of("主动 SOS", "SpO2 86% 偏低", "距离 R-001 11.7m"),
            now.minusSeconds(9)
        ));
        addVictim(new Victim(
            "V-004", "受灾者 04", 45, "女", "VD-004", RiskLevel.YELLOW, 54.4,
            new VitalSigns(93, 112, 108, 68, 36.9, "静止", 0.69, 55),
            new LocationPoint(18, 56, 0, 22.6, 3.8, 82),
            null, "T-0005", "待确认",
            List.of("生命体征轻度异常", "信号质量偏低，需要复测"),
            now.minusSeconds(75)
        ));
        addVictim(new Victim(
            "V-005", "受灾者 05", 19, "男", "VD-005", RiskLevel.GREEN, 28.1,
            new VitalSigns(97, 88, 116, 72, 36.6, "可移动", 0.91, 88),
            new LocationPoint(16, 16, 0, 14.2, 1.6, 231),
            null, null, "等待转移",
            List.of("生命体征稳定", "位置靠近安全通道"),
            now.minusSeconds(42)
        ));
        addVictim(new Victim(
            "V-006", "受灾者 06", 51, "男", "VD-006", RiskLevel.ORANGE, 82.6,
            new VitalSigns(91, 132, 102, 64, 37.9, "静止", 0.81, 47),
            new LocationPoint(96, 48, 0, 8.5, 2.2, 301),
            "R-004", "T-0004", "接近中",
            List.of("附近可燃气体 22.8% LEL", "心率 132 bpm", "最近救援员 8.5m"),
            now.minusSeconds(12)
        ));
        addVictim(new Victim(
            "V-007", "受灾者 07", 38, "女", "VD-007", RiskLevel.GRAY, 48.9,
            new VitalSigns(0, 0, 0, 0, 0, "信号中断", 0.12, 29),
            new LocationPoint(84, 18, 0, 41.5, 8.5, 0),
            null, "T-0006", "需复核",
            List.of("生命体征数据中断超过 120 秒", "设备低电量", "定位误差较大"),
            now.minusMinutes(3)
        ));
        addVictim(new Victim(
            "V-008", "受灾者 08", 56, "女", "VD-008", RiskLevel.YELLOW, 58.7,
            new VitalSigns(92, 118, 98, 59, 37.4, "微弱活动", 0.72, 64),
            new LocationPoint(104, 58, 0, 18.9, 3.1, 284),
            null, null, "待派发",
            List.of("血氧临界", "处于管廊风险区边缘"),
            now.minusSeconds(47)
        ));
    }

    private void addDevices(LocalDateTime now) {
        victims.values().forEach(v -> devices.put(v.deviceId(), new Device(
            v.deviceId(), "受灾人员星闪生命腕带 " + v.id().substring(2), "VICTIM_TAG", v.id(),
            v.level() == RiskLevel.GRAY ? DeviceStatus.WARNING : DeviceStatus.ONLINE,
            v.vitals().battery(), "v0.8.2", v.vitals().signalQuality() > 0.75 ? "强" : "中", v.lastSeenAt()
        )));
        rescuers.values().forEach(r -> devices.put(r.deviceId(), new Device(
            r.deviceId(), "救援人员星闪导航终端 " + r.id().substring(2), "RESCUER_TERMINAL", r.id(),
            r.status(), r.battery(), "v0.9.4", r.environment().riskLevel() == RiskLevel.RED ? "受干扰" : "强", r.lastSeenAt()
        )));
        devices.put("GW-001", new Device("GW-001", "便携星闪网关 A", "GATEWAY", "现场指挥组", DeviceStatus.ONLINE, 96, "v0.7.9", "强", now.minusSeconds(3)));
        devices.put("A-001", new Device("A-001", "星闪定位锚点 1", "ANCHOR", "A1 区", DeviceStatus.ONLINE, 78, "v0.6.1", "强", now.minusSeconds(22)));
        devices.put("A-002", new Device("A-002", "星闪定位锚点 2", "ANCHOR", "A2 区", DeviceStatus.ONLINE, 73, "v0.6.1", "中", now.minusSeconds(24)));
        devices.put("A-003", new Device("A-003", "星闪定位锚点 3", "ANCHOR", "C2 区", DeviceStatus.WARNING, 51, "v0.6.1", "中", now.minusSeconds(50)));
    }

    private void addAlerts(LocalDateTime now) {
        addAlert(new AlertEvent("AL-001", "VITAL_CRITICAL", RiskLevel.RED, "受灾者 01 低血氧高心率", "SpO2 84%，心率 146 bpm，建议立即救援并准备氧疗。", "V-001", "VD-001", "ACTIVE", now.minusMinutes(5), now.minusSeconds(18)));
        addAlert(new AlertEvent("AL-002", "SOS", RiskLevel.RED, "受灾者 03 主动 SOS", "腕带连续触发 SOS，距离救援员 R-001 约 11.7m。", "V-003", "VD-003", "ACKED", now.minusMinutes(3), now.minusSeconds(9)));
        addAlert(new AlertEvent("AL-003", "ENV_DANGER", RiskLevel.RED, "C2 管廊可燃气体危险", "R-004 检测到 22.8% LEL，可燃气体超过撤离阈值。", "R-004", "RD-004", "ACTIVE", now.minusMinutes(2), now.minusSeconds(5)));
        addAlert(new AlertEvent("AL-004", "DEVICE_LOW_BATTERY", RiskLevel.YELLOW, "受灾者 07 设备低电量", "VD-007 电量 29%，生命体征数据已中断超过 120 秒。", "V-007", "VD-007", "ACTIVE", now.minusMinutes(7), now.minusMinutes(3)));
        addAlert(new AlertEvent("AL-005", "ENV_WARNING", RiskLevel.ORANGE, "A2 商铺走廊气体升高", "R-002 检测到 18.4% LEL，建议侦检组确认。", "R-002", "RD-002", "ACKED", now.minusMinutes(4), now.minusSeconds(14)));
        addAlert(new AlertEvent("AL-006", "LOCATION_LOW_QUALITY", RiskLevel.YELLOW, "受灾者 04 定位质量偏低", "定位误差 3.8m，建议救援终端移动扫描复测。", "V-004", "VD-004", "ACTIVE", now.minusMinutes(6), now.minusSeconds(75)));
    }

    private void addTasks(LocalDateTime now) {
        addTask(new RescueTask("T-0001", "V-003", "R-001", TaskStatus.IN_PROGRESS, RiskLevel.RED, 91.0, "SOS + 低血氧，距离最近", now.minusMinutes(9), now.minusMinutes(8), null));
        addTask(new RescueTask("T-0002", "V-001", "R-002", TaskStatus.ASSIGNED, RiskLevel.RED, 94.5, "SpO2 84%，心率 146，血压偏低", now.minusMinutes(7), now.minusMinutes(6), null));
        addTask(new RescueTask("T-0003", "V-002", null, TaskStatus.PENDING, RiskLevel.ORANGE, 78.2, "血氧 89%，仍有微弱活动", now.minusMinutes(4), null, null));
        addTask(new RescueTask("T-0004", "V-006", "R-004", TaskStatus.PAUSED, RiskLevel.ORANGE, 82.6, "附近可燃气体超 20% LEL，等待侦检确认", now.minusMinutes(8), now.minusMinutes(7), null));
        addTask(new RescueTask("T-0005", "V-004", null, TaskStatus.PENDING, RiskLevel.YELLOW, 54.4, "生命体征轻度异常，定位质量偏低", now.minusMinutes(3), null, null));
        addTask(new RescueTask("T-0006", "V-007", null, TaskStatus.PENDING, RiskLevel.GRAY, 48.9, "设备失联，需复核", now.minusMinutes(11), null, null));
    }

    private void addTrends(LocalDateTime now) {
        DateTimeFormatter formatter = DateTimeFormatter.ofPattern("HH:mm");
        int[] spo2 = {96, 95, 94, 93, 91, 90, 88, 87, 86, 85, 86, 84};
        int[] hr = {88, 92, 96, 102, 110, 118, 126, 132, 136, 142, 138, 146};
        double[] gas = {3.2, 3.8, 4.4, 5.2, 7.8, 9.6, 11.2, 13.8, 15.6, 18.4, 21.2, 22.8};
        int[] alert = {1, 1, 2, 2, 3, 3, 4, 5, 5, 6, 6, 6};
        for (int i = 0; i < spo2.length; i++) {
            trends.add(new TrendPoint(now.minusMinutes((spo2.length - 1L - i) * 5).format(formatter), spo2[i], hr[i], gas[i], alert[i]));
        }
    }

    private void addVictim(Victim victim) {
        victims.put(victim.id(), victim);
    }

    private void addRescuer(Rescuer rescuer) {
        rescuers.put(rescuer.id(), rescuer);
    }

    private void addAlert(AlertEvent alert) {
        alerts.put(alert.id(), alert);
    }

    private void addTask(RescueTask task) {
        tasks.put(task.id(), task);
    }
}

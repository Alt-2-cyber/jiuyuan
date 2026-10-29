package com.starflash.rescue.service;

import com.starflash.rescue.model.DeviceStatus;
import com.starflash.rescue.model.LocationPoint;
import com.starflash.rescue.model.Rescuer;
import com.starflash.rescue.model.RiskLevel;
import com.starflash.rescue.model.RoutePlan;
import com.starflash.rescue.model.RouteWaypoint;
import com.starflash.rescue.model.Victim;
import org.springframework.stereotype.Service;

import java.time.LocalDateTime;
import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.Comparator;
import java.util.HashMap;
import java.util.HashSet;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Optional;
import java.util.PriorityQueue;
import java.util.Set;

@Service
public class RoutePlanningService {
    private static final int DEFAULT_WIDTH = 120;
    private static final int DEFAULT_HEIGHT = 76;

    private final RescueDataService dataService;

    public RoutePlanningService(RescueDataService dataService) {
        this.dataService = dataService;
    }

    public RoutePlan plan(String requestedVictimId, String requestedRescuerId) {
        List<Victim> victims = dataService.victims();
        List<Rescuer> rescuers = dataService.rescuers().stream()
            .filter(rescuer -> rescuer.status() != DeviceStatus.OFFLINE)
            .toList();

        if (victims.isEmpty() || rescuers.isEmpty()) {
            return emptyPlan();
        }

        Victim victim = chooseVictim(victims, requestedVictimId);
        Rescuer rescuer = chooseRescuer(rescuers, victim, requestedRescuerId);
        int width = DEFAULT_WIDTH;
        int height = DEFAULT_HEIGHT;
        List<Map<String, Object>> zones = zones();
        List<GridPoint> gridPath = astar(toGrid(rescuer.location(), width, height), toGrid(victim.location(), width, height), width, height, zones);
        List<RouteWaypoint> waypoints = simplify(gridPath).stream()
            .map(point -> new RouteWaypoint(point.x(), point.y(), label(point, rescuer, victim), type(point, gridPath), zoneRisk(point.x(), point.y(), zones)))
            .toList();

        double distance = pathDistance(gridPath);
        int minutes = Math.max(1, (int) Math.ceil(distance / 18.0));
        List<String> warnings = warnings(victim, zones, gridPath);
        String strategy = strategy(victim, rescuer, warnings, distance);

        return new RoutePlan(
            "RP-" + LocalDateTime.now().format(java.time.format.DateTimeFormatter.ofPattern("HHmmss")),
            rescuer.id(),
            rescuer.name(),
            victim.id(),
            victim.name(),
            isRealVictim(victim),
            round(distance),
            minutes,
            victim.level(),
            strategy,
            warnings,
            waypoints,
            LocalDateTime.now()
        );
    }

    private Victim chooseVictim(List<Victim> victims, String requestedVictimId) {
        if (requestedVictimId != null && !requestedVictimId.isBlank()) {
            Optional<Victim> requested = victims.stream()
                .filter(victim -> victim.id().equals(requestedVictimId) || victim.deviceId().equals(requestedVictimId))
                .findFirst();
            if (requested.isPresent()) {
                return requested.get();
            }
        }
        return victims.stream()
            .max(Comparator
                .comparing((Victim victim) -> isRealVictim(victim) ? 1 : 0)
                .thenComparing(Victim::priorityScore))
            .orElse(victims.get(0));
    }

    private Rescuer chooseRescuer(List<Rescuer> rescuers, Victim victim, String requestedRescuerId) {
        if (requestedRescuerId != null && !requestedRescuerId.isBlank()) {
            Optional<Rescuer> requested = rescuers.stream()
                .filter(rescuer -> rescuer.id().equals(requestedRescuerId) || rescuer.deviceId().equals(requestedRescuerId))
                .findFirst();
            if (requested.isPresent()) {
                return requested.get();
            }
        }

        return rescuers.stream()
            .min(Comparator.comparing(rescuer -> straightDistance(rescuer.location(), victim.location())
                + environmentPenalty(rescuer.environment().riskLevel())
                + batteryPenalty(rescuer.battery())))
            .orElse(rescuers.get(0));
    }

    @SuppressWarnings("unchecked")
    private List<Map<String, Object>> zones() {
        Object value = dataService.mapPayload().get("zones");
        if (value instanceof List<?> list) {
            List<Map<String, Object>> result = new ArrayList<>();
            for (Object item : list) {
                if (item instanceof Map<?, ?> map) {
                    Map<String, Object> zone = new LinkedHashMap<>();
                    map.forEach((key, val) -> zone.put(String.valueOf(key), val));
                    result.add(zone);
                }
            }
            return result;
        }
        return List.of();
    }

    private List<GridPoint> astar(GridPoint start, GridPoint goal, int width, int height, List<Map<String, Object>> zones) {
        PriorityQueue<SearchNode> open = new PriorityQueue<>(Comparator.comparingDouble(SearchNode::f));
        Map<GridPoint, GridPoint> cameFrom = new HashMap<>();
        Map<GridPoint, Double> cost = new HashMap<>();
        Set<GridPoint> closed = new HashSet<>();

        open.add(new SearchNode(start, 0, heuristic(start, goal)));
        cost.put(start, 0.0);

        int[][] dirs = {
            {1, 0}, {-1, 0}, {0, 1}, {0, -1},
            {1, 1}, {1, -1}, {-1, 1}, {-1, -1}
        };

        while (!open.isEmpty()) {
            SearchNode current = open.poll();
            if (current.point().equals(goal)) {
                return reconstruct(cameFrom, current.point());
            }
            if (!closed.add(current.point())) {
                continue;
            }

            for (int[] dir : dirs) {
                GridPoint next = new GridPoint(current.point().x() + dir[0], current.point().y() + dir[1]);
                if (next.x() < 0 || next.y() < 0 || next.x() > width || next.y() > height) {
                    continue;
                }
                double step = dir[0] == 0 || dir[1] == 0 ? 1.0 : 1.414;
                double nextCost = cost.get(current.point()) + step * zoneCost(next.x(), next.y(), zones);
                if (nextCost < cost.getOrDefault(next, Double.MAX_VALUE)) {
                    cameFrom.put(next, current.point());
                    cost.put(next, nextCost);
                    open.add(new SearchNode(next, nextCost, nextCost + heuristic(next, goal)));
                }
            }
        }

        return List.of(start, goal);
    }

    private List<GridPoint> reconstruct(Map<GridPoint, GridPoint> cameFrom, GridPoint end) {
        ArrayDeque<GridPoint> path = new ArrayDeque<>();
        GridPoint current = end;
        path.addFirst(current);
        while (cameFrom.containsKey(current)) {
            current = cameFrom.get(current);
            path.addFirst(current);
        }
        return new ArrayList<>(path);
    }

    private List<GridPoint> simplify(List<GridPoint> path) {
        if (path.size() <= 2) {
            return path;
        }
        List<GridPoint> result = new ArrayList<>();
        result.add(path.get(0));
        int previousDx = 0;
        int previousDy = 0;
        for (int i = 1; i < path.size(); i++) {
            GridPoint prev = path.get(i - 1);
            GridPoint current = path.get(i);
            int dx = Integer.compare(current.x() - prev.x(), 0);
            int dy = Integer.compare(current.y() - prev.y(), 0);
            if (i > 1 && (dx != previousDx || dy != previousDy)) {
                result.add(prev);
            }
            previousDx = dx;
            previousDy = dy;
        }
        result.add(path.get(path.size() - 1));
        return result;
    }

    private List<String> warnings(Victim victim, List<Map<String, Object>> zones, List<GridPoint> path) {
        List<String> warnings = new ArrayList<>();
        if (isRealVictim(victim)) {
            warnings.add("目标使用华为云/MySQL 最新真实样本");
        } else {
            warnings.add("目标为演示背景数据，等待真实设备同步");
        }
        if (victim.vitals().spo2() > 0 && victim.vitals().spo2() < 90) {
            warnings.add("血氧低于 90%，优先携带供氧设备");
        }
        if (victim.vitals().heartRate() > 120 || (victim.vitals().heartRate() > 0 && victim.vitals().heartRate() < 50)) {
            warnings.add("心率异常，抵达后先做生命体征复核");
        }
        long redSteps = path.stream().filter(point -> zoneRisk(point.x(), point.y(), zones) == RiskLevel.RED).count();
        if (redSteps > 0) {
            warnings.add("路线仍会擦过红色风险区边缘，现场需二次确认");
        }
        return warnings;
    }

    private String strategy(Victim victim, Rescuer rescuer, List<String> warnings, double distance) {
        String mode = warnings.stream().anyMatch(item -> item.contains("红色风险区")) ? "绕行高风险区域" : "最短安全路径";
        return rescuer.name() + " 从当前位置出发，采用" + mode + "接近 " + victim.name()
            + "，规划距离约 " + String.format("%.1f", distance) + "m。";
    }

    private GridPoint toGrid(LocationPoint point, int width, int height) {
        return new GridPoint((int) Math.round(clamp(point.x(), 0, width)), (int) Math.round(clamp(point.y(), 0, height)));
    }

    private String label(GridPoint point, Rescuer rescuer, Victim victim) {
        GridPoint start = toGrid(rescuer.location(), DEFAULT_WIDTH, DEFAULT_HEIGHT);
        GridPoint end = toGrid(victim.location(), DEFAULT_WIDTH, DEFAULT_HEIGHT);
        if (point.equals(start)) {
            return "救援端 " + rescuer.id();
        }
        if (point.equals(end)) {
            return "受灾端 " + victim.id();
        }
        return "转向点";
    }

    private String type(GridPoint point, List<GridPoint> path) {
        if (path.isEmpty()) {
            return "waypoint";
        }
        if (point.equals(path.get(0))) {
            return "start";
        }
        if (point.equals(path.get(path.size() - 1))) {
            return "target";
        }
        return "waypoint";
    }

    private double zoneCost(int x, int y, List<Map<String, Object>> zones) {
        return switch (zoneRisk(x, y, zones)) {
            case RED -> 8.5;
            case ORANGE -> 3.4;
            case YELLOW -> 1.7;
            case GREEN -> 1.0;
            case GRAY -> 2.0;
        };
    }

    private RiskLevel zoneRisk(double x, double y, List<Map<String, Object>> zones) {
        for (Map<String, Object> zone : zones) {
            double zx = number(zone.get("x"));
            double zy = number(zone.get("y"));
            double zw = number(zone.get("w"));
            double zh = number(zone.get("h"));
            if (x >= zx && x <= zx + zw && y >= zy && y <= zy + zh) {
                return RiskLevel.valueOf(String.valueOf(zone.getOrDefault("risk", "GRAY")));
            }
        }
        return RiskLevel.GREEN;
    }

    private double pathDistance(List<GridPoint> path) {
        double total = 0;
        for (int i = 1; i < path.size(); i++) {
            GridPoint a = path.get(i - 1);
            GridPoint b = path.get(i);
            total += Math.hypot(a.x() - b.x(), a.y() - b.y());
        }
        return total;
    }

    private double straightDistance(LocationPoint a, LocationPoint b) {
        return Math.hypot(a.x() - b.x(), a.y() - b.y());
    }

    private double heuristic(GridPoint a, GridPoint b) {
        return Math.hypot(a.x() - b.x(), a.y() - b.y());
    }

    private double environmentPenalty(RiskLevel level) {
        return switch (level) {
            case RED -> 30;
            case ORANGE -> 12;
            case YELLOW -> 5;
            case GREEN -> 0;
            case GRAY -> 10;
        };
    }

    private double batteryPenalty(int battery) {
        return battery < 40 ? 20 : battery < 60 ? 8 : 0;
    }

    private boolean isRealVictim(Victim victim) {
        return victim.id().contains("REAL") || victim.status().contains("真实");
    }

    private double number(Object value) {
        if (value instanceof Number number) {
            return number.doubleValue();
        }
        if (value instanceof String text && !text.isBlank()) {
            try {
                return Double.parseDouble(text);
            } catch (NumberFormatException ignored) {
                return 0;
            }
        }
        return 0;
    }

    private double round(double value) {
        return Math.round(value * 10.0) / 10.0;
    }

    private double clamp(double value, double min, double max) {
        return Math.max(min, Math.min(max, value));
    }

    private RoutePlan emptyPlan() {
        return new RoutePlan(
            "RP-EMPTY",
            "",
            "",
            "",
            "",
            false,
            0,
            0,
            RiskLevel.GRAY,
            "暂无可规划目标，请等待受灾端或救援端上线。",
            List.of("缺少受灾端或救援端数据"),
            List.of(),
            LocalDateTime.now()
        );
    }

    private record GridPoint(int x, int y) {
    }

    private record SearchNode(GridPoint point, double g, double f) {
    }
}

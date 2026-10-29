package com.starflash.rescue.api;

import com.starflash.rescue.model.AlertEvent;
import com.starflash.rescue.model.Device;
import com.starflash.rescue.model.RescueTask;
import com.starflash.rescue.model.RescueTerminalReport;
import com.starflash.rescue.model.Rescuer;
import com.starflash.rescue.model.SiteOverview;
import com.starflash.rescue.model.RoutePlan;
import com.starflash.rescue.model.RoutePlanRequest;
import com.starflash.rescue.model.TaskAssignRequest;
import com.starflash.rescue.model.Victim;
import com.starflash.rescue.service.RescueDataService;
import com.starflash.rescue.service.RoutePlanningService;
import jakarta.validation.Valid;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.PathVariable;
import org.springframework.web.bind.annotation.PostMapping;
import org.springframework.web.bind.annotation.RequestBody;
import org.springframework.web.bind.annotation.RequestMapping;
import org.springframework.web.bind.annotation.RestController;

import java.util.List;
import java.util.Map;

@RestController
@RequestMapping("/api")
public class RescueController {
    private final RescueDataService dataService;
    private final RoutePlanningService routePlanningService;

    public RescueController(RescueDataService dataService, RoutePlanningService routePlanningService) {
        this.dataService = dataService;
        this.routePlanningService = routePlanningService;
    }

    @GetMapping("/overview")
    public SiteOverview overview() {
        return dataService.overview();
    }

    @GetMapping("/map")
    public Map<String, Object> map() {
        return dataService.mapPayload();
    }

    @GetMapping("/victims")
    public List<Victim> victims() {
        return dataService.victims();
    }

    @GetMapping("/victims/{id}")
    public ResponseEntity<Victim> victim(@PathVariable String id) {
        return dataService.victim(id)
            .map(ResponseEntity::ok)
            .orElseGet(() -> ResponseEntity.notFound().build());
    }

    @GetMapping("/rescuers")
    public List<Rescuer> rescuers() {
        return dataService.rescuers();
    }

    @GetMapping("/devices")
    public List<Device> devices() {
        return dataService.devices();
    }

    @GetMapping("/alerts")
    public List<AlertEvent> alerts() {
        return dataService.alerts();
    }

    @GetMapping("/tasks")
    public List<RescueTask> tasks() {
        return dataService.tasks();
    }

    @PostMapping("/tasks/{taskId}/assign")
    public ResponseEntity<RescueTask> assign(
        @PathVariable String taskId,
        @Valid @RequestBody TaskAssignRequest request
    ) {
        return dataService.assignTask(taskId, request.rescuerId())
            .map(ResponseEntity::ok)
            .orElseGet(() -> ResponseEntity.notFound().build());
    }

    @PostMapping("/device-ingest/rescue-terminal")
    public Map<String, Object> ingestRescueTerminal(@Valid @RequestBody RescueTerminalReport report) {
        return dataService.ingestRescueTerminal(report);
    }

    @PostMapping("/route-plan")
    public RoutePlan routePlan(@RequestBody(required = false) RoutePlanRequest request) {
        String victimId = request == null ? null : request.victimId();
        String rescuerId = request == null ? null : request.rescuerId();
        return routePlanningService.plan(victimId, rescuerId);
    }
}

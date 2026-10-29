package com.starflash.rescue.api;

import com.starflash.rescue.model.CloudSyncResult;
import com.starflash.rescue.model.CloudVictimIngestRequest;
import com.starflash.rescue.model.CloudVictimSample;
import com.starflash.rescue.service.HuaweiCloudIotdaService;
import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.PostMapping;
import org.springframework.web.bind.annotation.RequestBody;
import org.springframework.web.bind.annotation.RequestMapping;
import org.springframework.web.bind.annotation.RequestParam;
import org.springframework.web.bind.annotation.RestController;

import java.util.Map;

@RestController
@RequestMapping("/api/cloud-victim-samples")
public class CloudVictimController {
    private final HuaweiCloudIotdaService cloudService;

    public CloudVictimController(HuaweiCloudIotdaService cloudService) {
        this.cloudService = cloudService;
    }

    @GetMapping
    public Map<String, Object> latest(@RequestParam(defaultValue = "100") int limit) {
        return cloudService.summary(limit);
    }

    @PostMapping("/sync")
    public CloudSyncResult sync() {
        return cloudService.syncLatestShadow();
    }

    @PostMapping("/ingest")
    public CloudVictimSample ingest(@RequestBody CloudVictimIngestRequest request) {
        return cloudService.ingest(request);
    }
}

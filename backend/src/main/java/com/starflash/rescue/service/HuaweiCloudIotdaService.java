package com.starflash.rescue.service;

import com.fasterxml.jackson.core.type.TypeReference;
import com.fasterxml.jackson.databind.ObjectMapper;
import com.huaweicloud.sdk.core.auth.BasicCredentials;
import com.huaweicloud.sdk.core.exception.ServiceResponseException;
import com.huaweicloud.sdk.core.region.Region;
import com.huaweicloud.sdk.iotda.v5.IoTDAClient;
import com.huaweicloud.sdk.iotda.v5.model.DeviceShadowData;
import com.huaweicloud.sdk.iotda.v5.model.DeviceShadowProperties;
import com.huaweicloud.sdk.iotda.v5.model.ShowDeviceShadowRequest;
import com.huaweicloud.sdk.iotda.v5.model.ShowDeviceShadowResponse;
import com.huaweicloud.sdk.iotda.v5.region.IoTDARegion;
import com.starflash.rescue.model.CloudSyncResult;
import com.starflash.rescue.model.CloudVictimIngestRequest;
import com.starflash.rescue.model.CloudVictimSample;
import org.springframework.beans.factory.annotation.Value;
import org.springframework.stereotype.Service;

import java.time.Instant;
import java.time.LocalDateTime;
import java.time.ZoneId;
import java.time.format.DateTimeFormatter;
import java.util.Arrays;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

@Service
public class HuaweiCloudIotdaService {
    private final CloudVictimSampleRepository repository;
    private final ObjectMapper objectMapper;
    private final boolean enabled;
    private final String ak;
    private final String sk;
    private final String projectId;
    private final String region;
    private final String endpoint;
    private final String deviceId;
    private final String deviceIds;
    private final String instanceId;
    private final String serviceId;

    private record ClientTarget(String name, String endpoint, boolean derivedAuth, boolean sendInstanceId, IoTDAClient client) {
    }

    public HuaweiCloudIotdaService(
        CloudVictimSampleRepository repository,
        ObjectMapper objectMapper,
        @Value("${huaweicloud.iotda.enabled:false}") boolean enabled,
        @Value("${huaweicloud.iotda.ak:}") String ak,
        @Value("${huaweicloud.iotda.sk:}") String sk,
        @Value("${huaweicloud.iotda.project-id:}") String projectId,
        @Value("${huaweicloud.iotda.region:cn-north-4}") String region,
        @Value("${huaweicloud.iotda.endpoint:}") String endpoint,
        @Value("${huaweicloud.iotda.device-id:}") String deviceId,
        @Value("${huaweicloud.iotda.device-ids:}") String deviceIds,
        @Value("${huaweicloud.iotda.instance-id:}") String instanceId,
        @Value("${huaweicloud.iotda.service-id:victim_state}") String serviceId
    ) {
        this.repository = repository;
        this.objectMapper = objectMapper;
        this.enabled = enabled;
        this.ak = ak;
        this.sk = sk;
        this.projectId = projectId;
        this.region = region;
        this.endpoint = endpoint;
        this.deviceId = deviceId;
        this.deviceIds = deviceIds;
        this.instanceId = instanceId;
        this.serviceId = serviceId;
    }

    public Map<String, Object> summary(int limit) {
        List<CloudVictimSample> samples = repository.findLatest(limit);
        Map<String, Object> result = new LinkedHashMap<>();
        result.put("total", repository.count());
        result.put("latest", samples.isEmpty() ? null : samples.get(0));
        result.put("items", samples);
        result.put("cloudEnabled", enabled);
        result.put("credentialAk", mask(ak));
        result.put("configuredDeviceId", mask(deviceId));
        result.put("configuredDeviceIds", configuredDeviceIds().stream().map(this::mask).toList());
        result.put("serviceId", serviceId);
        result.put("syncContext", syncContext());
        return result;
    }

    public CloudSyncResult syncLatestShadow() {
        String missing = missingConfig();
        if (missing != null) {
            return new CloudSyncResult(false, missing, 0, null);
        }

        try {
            List<ClientTarget> clientTargets = buildClientTargets();
            CloudVictimSample latest = null;
            int inserted = 0;
            List<String> ids = configuredDeviceIds();
            List<String> failures = new java.util.ArrayList<>();
            String requestInstanceId = effectiveInstanceId();

            for (String syncDeviceId : ids) {
                ShowDeviceShadowResponse response = null;
                String successTarget = "";
                List<String> attemptFailures = new java.util.ArrayList<>();

                for (ClientTarget target : clientTargets) {
                    try {
                        ShowDeviceShadowRequest request = new ShowDeviceShadowRequest().withDeviceId(syncDeviceId);
                        if (target.sendInstanceId() && !requestInstanceId.isBlank()) {
                            request.withInstanceId(requestInstanceId);
                        }
                        response = target.client().showDeviceShadow(request);
                        successTarget = target.name();
                        break;
                    } catch (ServiceResponseException ex) {
                        attemptFailures.add(targetFailureMessage(target, ex));
                    }
                }

                if (response == null) {
                    failures.add(deviceFailureMessage(syncDeviceId, attemptFailures));
                    continue;
                }

                List<DeviceShadowData> shadows = response.getShadow();
                if (shadows == null || shadows.isEmpty()) {
                    continue;
                }

                for (DeviceShadowData shadow : shadows) {
                    if (shadow.getReported() == null) {
                        continue;
                    }
                    if (!serviceId.isBlank() && shadow.getServiceId() != null && !serviceId.equals(shadow.getServiceId())) {
                        continue;
                    }

                    CloudVictimSample sample = toSample(
                        syncDeviceId,
                        response.getDeviceId(),
                        shadow.getServiceId(),
                        shadow.getReported(),
                        "HUAWEI_SHADOW",
                        successTarget
                    );
                    latest = repository.save(sample);
                    inserted++;
                }
            }

            if (inserted == 0) {
                if (!failures.isEmpty()) {
                    return new CloudSyncResult(false, "Sync failed: " + String.join(" | ", failures) + " (" + syncContext() + ")", 0, null);
                }
                return new CloudSyncResult(false, "No reported properties matched service_id=" + serviceId + " (" + syncContext() + ")", 0, null);
            }

            String warning = failures.isEmpty() ? "" : ", partial failures=" + failures.size();
            return new CloudSyncResult(true, "Synced Huawei IoTDA device shadow, devices=" + ids.size() + warning + " (" + syncContext() + ")", inserted, latest);
        } catch (ServiceResponseException ex) {
            return new CloudSyncResult(false, "Sync failed: httpStatus=" + ex.getHttpStatusCode()
                + ", errorCode=" + ex.getErrorCode()
                + ", errorMsg=" + ex.getErrorMsg()
                + ", requestId=" + ex.getRequestId()
                + " (" + syncContext() + ")", 0, null);
        } catch (Exception ex) {
            return new CloudSyncResult(false, "Sync failed: " + ex.getMessage() + " (" + syncContext() + ")", 0, null);
        }
    }

    public CloudVictimSample ingest(CloudVictimIngestRequest request) {
        Map<String, Object> raw = request.raw() == null ? new LinkedHashMap<>() : new LinkedHashMap<>(request.raw());
        raw.putIfAbsent("deviceId", request.deviceId());
        raw.putIfAbsent("serviceId", request.serviceId());
        raw.putIfAbsent("seq", request.seq());
        raw.putIfAbsent("spo2", request.spo2());
        raw.putIfAbsent("hr", request.hr() == null ? request.heartRate() : request.hr());
        raw.putIfAbsent("sbp", request.sbp() == null ? request.systolic() : request.sbp());
        raw.putIfAbsent("battery", request.battery());
        raw.putIfAbsent("sos", request.sos());

        CloudVictimSample sample = new CloudVictimSample(
            null,
            firstNonBlank(request.deviceId(), primaryDeviceId(), "UNKNOWN"),
            firstNonBlank(request.serviceId(), serviceId, "victim_state"),
            parseTime(request.eventTime(), LocalDateTime.now()),
            request.seq(),
            request.spo2(),
            request.hr() == null ? request.heartRate() : request.hr(),
            request.sbp() == null ? request.systolic() : request.sbp(),
            request.battery(),
            request.sos(),
            "HTTP_INGEST",
            raw,
            null
        );
        return repository.save(sample);
    }

    private List<ClientTarget> buildClientTargets() {
        List<ClientTarget> targets = new java.util.ArrayList<>();
        String configuredEndpoint = normalizeEndpoint(endpoint);

        if (!configuredEndpoint.isBlank() && isInstanceAppEndpoint(configuredEndpoint)) {
            addClientTarget(targets, "configured-app-derived-no-instance", configuredEndpoint, true, false);
            addClientTarget(targets, "configured-app-derived-with-instance", configuredEndpoint, true, true);
        } else if (!configuredEndpoint.isBlank()) {
            addClientTarget(targets, "configured-endpoint-derived", configuredEndpoint, true, true);
            addClientTarget(targets, "configured-endpoint-basic", configuredEndpoint, false, true);
        }

        String regionalEndpoint = regionalApiEndpoint();
        if (!regionalEndpoint.isBlank()) {
            addClientTarget(targets, "regional-api-derived", regionalEndpoint, true, true);
        }

        addClientTarget(targets, "region-default", "", false, true);
        return targets;
    }

    private void addClientTarget(List<ClientTarget> targets, String name, String targetEndpoint, boolean derivedAuth, boolean sendInstanceId) {
        String normalized = normalizeEndpoint(targetEndpoint);
        for (ClientTarget target : targets) {
            if (normalizeEndpoint(target.endpoint()).equals(normalized)
                && target.derivedAuth() == derivedAuth
                && target.sendInstanceId() == sendInstanceId) {
                return;
            }
        }
        targets.add(new ClientTarget(name, normalized, derivedAuth, sendInstanceId, buildClient(normalized, derivedAuth)));
    }

    private IoTDAClient buildClient(String targetEndpoint, boolean derivedAuth) {
        BasicCredentials credentials = new BasicCredentials()
            .withAk(ak)
            .withSk(sk);
        if (derivedAuth) {
            credentials.withDerivedPredicate(BasicCredentials.DEFAULT_DERIVED_PREDICATE);
        }
        if (!projectId.isBlank()) {
            credentials.withProjectId(projectId);
        }

        var builder = IoTDAClient.newBuilder().withCredential(credentials);
        if (targetEndpoint == null || targetEndpoint.isBlank()) {
            builder.withRegion(IoTDARegion.valueOf(region));
        } else {
            builder.withRegion(new Region(region, targetEndpoint));
        }
        return builder.build();
    }

    private CloudVictimSample toSample(
        String syncDeviceId,
        String responseDeviceId,
        String shadowServiceId,
        DeviceShadowProperties reported,
        String source,
        String syncTarget
    ) {
        Map<String, Object> props = normalizeProperties(reported.getProperties());
        props.putIfAbsent("_cloud_device_id", firstNonBlank(responseDeviceId, syncDeviceId));
        props.putIfAbsent("_sync_target", syncTarget);

        String sampleDeviceId = firstNonBlank(responseDeviceId, syncDeviceId, primaryDeviceId(), "UNKNOWN");

        return new CloudVictimSample(
            null,
            sampleDeviceId,
            firstNonBlank(shadowServiceId, serviceId),
            parseTime(reported.getEventTime(), LocalDateTime.now()),
            intValue(props.get("seq")),
            intValue(props.get("spo2")),
            intValue(firstValue(props, "hr", "heartRate", "heart_rate")),
            intValue(firstValue(props, "sbp", "systolic")),
            intValue(props.get("battery")),
            boolValue(props.get("sos")),
            source,
            props,
            null
        );
    }

    private Map<String, Object> normalizeProperties(Object properties) {
        if (properties == null) {
            return new LinkedHashMap<>();
        }
        return objectMapper.convertValue(properties, new TypeReference<>() {
        });
    }

    private Object firstValue(Map<String, Object> map, String... keys) {
        for (String key : keys) {
            if (map.containsKey(key)) {
                return map.get(key);
            }
        }
        return null;
    }

    private Integer intValue(Object value) {
        if (value instanceof Number number) {
            return number.intValue();
        }
        if (value instanceof String text && !text.isBlank()) {
            try {
                return Integer.parseInt(text.trim());
            } catch (NumberFormatException ignored) {
                return null;
            }
        }
        return null;
    }

    private Boolean boolValue(Object value) {
        if (value instanceof Boolean bool) {
            return bool;
        }
        if (value instanceof Number number) {
            return number.intValue() != 0;
        }
        if (value instanceof String text) {
            return "true".equalsIgnoreCase(text) || "1".equals(text) || "SOS".equalsIgnoreCase(text);
        }
        return null;
    }

    private String stringValue(Object value) {
        return value == null ? null : String.valueOf(value);
    }

    private LocalDateTime parseTime(String text, LocalDateTime fallback) {
        if (text == null || text.isBlank()) {
            return fallback;
        }
        try {
            return Instant.parse(text).atZone(ZoneId.systemDefault()).toLocalDateTime();
        } catch (Exception ignored) {
            try {
                return LocalDateTime.parse(text, DateTimeFormatter.ofPattern("yyyy-MM-dd HH:mm:ss"));
            } catch (Exception ignoredAgain) {
                return fallback;
            }
        }
    }

    private String firstNonBlank(String... values) {
        for (String value : values) {
            if (value != null && !value.isBlank()) {
                return value;
            }
        }
        return "";
    }

    private String missingConfig() {
        if (!enabled) {
            return "Huawei IoTDA sync is disabled: set HUAWEI_IOTDA_ENABLED=true";
        }
        if (ak.isBlank() || sk.isBlank()) {
            return "Missing Huawei AK/SK: set HUAWEICLOUD_SDK_AK and HUAWEICLOUD_SDK_SK";
        }
        if (configuredDeviceIds().isEmpty()) {
            return "Missing device ID: set HUAWEI_IOTDA_DEVICE_IDS";
        }
        if (region.isBlank() && endpoint.isBlank()) {
            return "Missing IoTDA region or endpoint: set HUAWEI_IOTDA_REGION or HUAWEI_IOTDA_ENDPOINT";
        }
        return null;
    }

    private List<String> configuredDeviceIds() {
        String value = firstNonBlank(deviceIds, deviceId);
        if (value.isBlank()) {
            return List.of();
        }
        return Arrays.stream(value.split(","))
            .map(String::trim)
            .filter(item -> !item.isBlank())
            .distinct()
            .toList();
    }

    private String primaryDeviceId() {
        List<String> ids = configuredDeviceIds();
        return ids.isEmpty() ? "" : ids.get(0);
    }

    private String syncContext() {
        String configuredEndpoint = endpoint.isBlank() ? "not-set" : normalizeEndpoint(endpoint);
        String preferredEndpoint = regionalApiEndpoint().isBlank() ? "region-default" : regionalApiEndpoint();
        String currentInstanceId = effectiveInstanceId().isBlank() ? "not-set" : mask(effectiveInstanceId());
        String currentProjectId = projectId.isBlank() ? "auto" : mask(projectId);
        return "region=" + region
            + ", configuredEndpoint=" + configuredEndpoint
            + ", preferredApiEndpoint=" + preferredEndpoint
            + ", instanceId=" + currentInstanceId
            + ", projectId=" + currentProjectId;
    }

    private String effectiveInstanceId() {
        if (instanceId != null && !instanceId.isBlank()) {
            return instanceId.trim();
        }
        String host = hostOf(endpoint);
        if (!host.contains(".iotda-app.")) {
            return "";
        }
        String prefix = host.split("\\.", 2)[0];
        if (prefix.isBlank() || "iotda".equals(prefix)) {
            return "";
        }
        return prefix;
    }

    private String deviceFailureMessage(String deviceId, List<String> attemptFailures) {
        return "device=" + mask(deviceId)
            + ", attempts=[" + String.join("; ", attemptFailures) + "]";
    }

    private String targetFailureMessage(ClientTarget target, ServiceResponseException ex) {
        String targetEndpoint = target.endpoint().isBlank() ? "region-default" : target.endpoint();
        return target.name() + "@" + targetEndpoint
            + ", auth=" + (target.derivedAuth() ? "derived" : "basic")
            + ", instanceHeader=" + target.sendInstanceId()
            + ", httpStatus=" + ex.getHttpStatusCode()
            + ", errorCode=" + ex.getErrorCode()
            + ", errorMsg=" + ex.getErrorMsg()
            + ", requestId=" + ex.getRequestId()
            + iotdaHint(ex);
    }

    private String iotdaHint(ServiceResponseException ex) {
        if (ex.getHttpStatusCode() == 500) {
            return ", hint=IoTDA 500: check endpoint/instanceId/projectId/deviceId. Backend tried regional API and configured endpoint.";
        }
        if (ex.getHttpStatusCode() == 403) {
            return ", hint=IoTDA 403: check IAM IoTDA permission and instance subscription.";
        }
        if (ex.getHttpStatusCode() == 404) {
            return ", hint=IoTDA 404: check whether deviceId belongs to this project and IoTDA instance.";
        }
        return "";
    }

    private String regionalApiEndpoint() {
        if (region == null || region.isBlank()) {
            return "";
        }
        return "https://iotda." + region.trim() + ".myhuaweicloud.com";
    }

    private boolean isInstanceAppEndpoint(String value) {
        return hostOf(value).contains(".iotda-app.");
    }

    private String normalizeEndpoint(String value) {
        if (value == null) {
            return "";
        }
        return value.trim().replaceAll("/+$", "");
    }

    private String hostOf(String value) {
        if (value == null || value.isBlank()) {
            return "";
        }
        return value.trim()
            .replaceFirst("^https?://", "")
            .split("/", 2)[0]
            .toLowerCase();
    }

    private String mask(String value) {
        if (value == null || value.isBlank()) {
            return "";
        }
        if (value.length() <= 8) {
            return "****";
        }
        return value.substring(0, 4) + "****" + value.substring(value.length() - 4);
    }
}

package com.starflash.rescue.service;

import com.fasterxml.jackson.core.type.TypeReference;
import com.fasterxml.jackson.databind.ObjectMapper;
import com.starflash.rescue.model.CloudVictimSample;
import org.springframework.jdbc.core.JdbcTemplate;
import org.springframework.jdbc.support.GeneratedKeyHolder;
import org.springframework.jdbc.support.KeyHolder;
import org.springframework.stereotype.Repository;

import java.sql.PreparedStatement;
import java.sql.Statement;
import java.sql.Timestamp;
import java.time.LocalDateTime;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

@Repository
public class CloudVictimSampleRepository {
    private final JdbcTemplate jdbcTemplate;
    private final ObjectMapper objectMapper;

    public CloudVictimSampleRepository(JdbcTemplate jdbcTemplate, ObjectMapper objectMapper) {
        this.jdbcTemplate = jdbcTemplate;
        this.objectMapper = objectMapper;
    }

    public CloudVictimSample save(CloudVictimSample sample) {
        KeyHolder keyHolder = new GeneratedKeyHolder();
        jdbcTemplate.update(connection -> {
            PreparedStatement statement = connection.prepareStatement("""
                INSERT INTO cloud_victim_sample
                  (device_id, service_id, event_time, seq, spo2, heart_rate, systolic, battery, sos, source, raw_json)
                VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
                """, Statement.RETURN_GENERATED_KEYS);
            statement.setString(1, sample.deviceId());
            statement.setString(2, sample.serviceId());
            statement.setTimestamp(3, Timestamp.valueOf(sample.eventTime()));
            setNullableInt(statement, 4, sample.seq());
            setNullableInt(statement, 5, sample.spo2());
            setNullableInt(statement, 6, sample.heartRate());
            setNullableInt(statement, 7, sample.systolic());
            setNullableInt(statement, 8, sample.battery());
            if (sample.sos() == null) {
                statement.setObject(9, null);
            } else {
                statement.setBoolean(9, sample.sos());
            }
            statement.setString(10, sample.source());
            statement.setString(11, toJson(sample.raw()));
            return statement;
        }, keyHolder);

        Number key = keyHolder.getKey();
        return new CloudVictimSample(
            key == null ? null : key.longValue(),
            sample.deviceId(),
            sample.serviceId(),
            sample.eventTime(),
            sample.seq(),
            sample.spo2(),
            sample.heartRate(),
            sample.systolic(),
            sample.battery(),
            sample.sos(),
            sample.source(),
            sample.raw(),
            LocalDateTime.now()
        );
    }

    public List<CloudVictimSample> findLatest(int limit) {
        int safeLimit = Math.max(1, Math.min(limit, 500));
        return jdbcTemplate.query("""
            SELECT id, device_id, service_id, event_time, seq, spo2, heart_rate, systolic, battery, sos,
                   source, raw_json, created_at
            FROM cloud_victim_sample
            ORDER BY event_time DESC, id DESC
            LIMIT ?
            """, (rs, rowNum) -> new CloudVictimSample(
            rs.getLong("id"),
            rs.getString("device_id"),
            rs.getString("service_id"),
            rs.getTimestamp("event_time").toLocalDateTime(),
            getNullableInt(rs.getObject("seq")),
            getNullableInt(rs.getObject("spo2")),
            getNullableInt(rs.getObject("heart_rate")),
            getNullableInt(rs.getObject("systolic")),
            getNullableInt(rs.getObject("battery")),
            rs.getObject("sos") == null ? null : rs.getBoolean("sos"),
            rs.getString("source"),
            fromJson(rs.getString("raw_json")),
            rs.getTimestamp("created_at").toLocalDateTime()
        ), safeLimit);
    }

    public long count() {
        Long count = jdbcTemplate.queryForObject("SELECT COUNT(*) FROM cloud_victim_sample", Long.class);
        return count == null ? 0L : count;
    }

    private void setNullableInt(PreparedStatement statement, int index, Integer value) throws java.sql.SQLException {
        if (value == null) {
            statement.setObject(index, null);
        } else {
            statement.setInt(index, value);
        }
    }

    private Integer getNullableInt(Object value) {
        return value instanceof Number number ? number.intValue() : null;
    }

    private String toJson(Map<String, Object> raw) {
        try {
            return objectMapper.writeValueAsString(raw == null ? Map.of() : raw);
        } catch (Exception ignored) {
            return "{}";
        }
    }

    private Map<String, Object> fromJson(String rawJson) {
        if (rawJson == null || rawJson.isBlank()) {
            return Map.of();
        }
        try {
            return objectMapper.readValue(rawJson, new TypeReference<>() {
            });
        } catch (Exception ignored) {
            return new LinkedHashMap<>();
        }
    }
}

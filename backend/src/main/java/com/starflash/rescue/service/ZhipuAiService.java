package com.starflash.rescue.service;

import com.fasterxml.jackson.databind.JsonNode;
import com.fasterxml.jackson.databind.ObjectMapper;
import com.starflash.rescue.model.AiChatMessage;
import com.starflash.rescue.model.AiChatRequest;
import com.starflash.rescue.model.AiResponse;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.beans.factory.annotation.Value;
import org.springframework.http.HttpHeaders;
import org.springframework.http.MediaType;
import org.springframework.stereotype.Service;
import org.springframework.web.reactive.function.client.WebClient;
import org.springframework.web.reactive.function.client.WebClientResponseException;
import reactor.core.publisher.Flux;

import java.time.Duration;
import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.concurrent.TimeoutException;

@Service
public class ZhipuAiService {
    private static final Logger LOGGER = LoggerFactory.getLogger(ZhipuAiService.class);
    private static final String CHAT_PATH = "/api/paas/v4/chat/completions";
    private static final String SYSTEM_PROMPT = """
        你是星闪灾后救援指挥系统的 AI 助手。回答要面向现场救援指挥，优先给出可执行建议。
        涉及伤情、环境风险和路径选择时，要提醒以现场专业人员判断为准，避免编造传感器没有提供的数据。
        """;

    private final WebClient webClient;
    private final ObjectMapper objectMapper;
    private final String apiKey;
    private final String model;

    public ZhipuAiService(
        WebClient.Builder builder,
        ObjectMapper objectMapper,
        @Value("${zhipu.api-key:${ZHIPU_API_KEY:}}") String apiKey,
        @Value("${zhipu.base-url:${ZHIPU_BASE_URL:https://open.bigmodel.cn}}") String baseUrl,
        @Value("${zhipu.model:${ZHIPU_MODEL:glm-4.7-flash}}") String model
    ) {
        this.webClient = builder.baseUrl(baseUrl).build();
        this.objectMapper = objectMapper;
        this.apiKey = apiKey == null ? "" : apiKey.trim();
        this.model = model;
    }

    public AiResponse chat(AiChatRequest request) {
        StringBuilder result = new StringBuilder();
        streamChat(request).toIterable().forEach(result::append);
        return new AiResponse("zhipu", result.toString());
    }

    public Flux<String> streamChat(AiChatRequest request) {
        if (apiKey.isBlank()) {
            return Flux.just("AI服务未完成后端配置，请联系管理员。");
        }

        Map<String, Object> payload = buildPayload(request, true);
        return webClient.post()
            .uri(CHAT_PATH)
            .header(HttpHeaders.AUTHORIZATION, "Bearer " + apiKey)
            .contentType(MediaType.APPLICATION_JSON)
            .accept(MediaType.TEXT_EVENT_STREAM)
            .bodyValue(payload)
            .retrieve()
            .bodyToFlux(String.class)
            .timeout(Duration.ofSeconds(90))
            .flatMap(this::parseSseChunk)
            .onErrorResume(WebClientResponseException.class, this::fallbackForHttpError)
            .onErrorResume(TimeoutException.class, ex -> Flux.just("AI服务响应超时，请稍后重试。"))
            .onErrorResume(Exception.class, ex -> {
                LOGGER.warn("Zhipu AI request failed: {}", ex.getMessage());
                return Flux.just("AI服务暂时不可用，请稍后重试。");
            });
    }

    private Flux<String> fallbackForHttpError(WebClientResponseException ex) {
        LOGGER.warn("Zhipu AI request failed with HTTP {}", ex.getStatusCode().value());
        int status = ex.getStatusCode().value();
        if (status == 401 || status == 403) {
            return Flux.just("AI服务认证失败，请检查后端配置。");
        }
        if (status == 429) {
            return Flux.just("AI服务请求过于频繁，请稍后再试。");
        }
        if (ex.getStatusCode().is5xxServerError()) {
            return Flux.just("AI服务繁忙，请稍后重试。");
        }
        return Flux.just("AI服务请求失败，请稍后重试。");
    }

    private Map<String, Object> buildPayload(AiChatRequest request, boolean stream) {
        Map<String, Object> payload = new LinkedHashMap<>();
        payload.put("model", model);
        payload.put("stream", stream);
        payload.put("temperature", 0.6);

        List<Map<String, String>> messages = new ArrayList<>();
        messages.add(message("system", SYSTEM_PROMPT));
        if (request.history() != null) {
            for (AiChatMessage item : request.history()) {
                if (item == null || item.content() == null || item.content().isBlank()) {
                    continue;
                }
                String role = "assistant".equals(item.role()) || "ai".equals(item.role()) ? "assistant" : "user";
                messages.add(message(role, item.content()));
            }
        }
        messages.add(message("user", request.message()));
        payload.put("messages", messages);
        return payload;
    }

    private Map<String, String> message(String role, String content) {
        Map<String, String> message = new LinkedHashMap<>();
        message.put("role", role);
        message.put("content", content);
        return message;
    }

    private Flux<String> parseSseChunk(String chunk) {
        if (chunk == null || chunk.isBlank() || "[DONE]".equals(chunk.trim())) {
            return Flux.empty();
        }

        List<String> pieces = new ArrayList<>();
        String[] lines = chunk.split("\\R");
        for (String line : lines) {
            String data = line.startsWith("data:") ? line.substring(5).trim() : line.trim();
            if (data.isBlank() || "[DONE]".equals(data)) {
                continue;
            }
            String content = parseDeltaContent(data);
            if (!content.isBlank()) {
                pieces.add(content);
            }
        }
        return Flux.fromIterable(pieces);
    }

    private String parseDeltaContent(String data) {
        try {
            JsonNode root = objectMapper.readTree(data);
            JsonNode choices = root.path("choices");
            if (!choices.isArray() || choices.isEmpty()) {
                return "";
            }
            JsonNode delta = choices.get(0).path("delta");
            JsonNode content = delta.path("content");
            if (content.isTextual()) {
                return content.asText();
            }
            JsonNode message = choices.get(0).path("message").path("content");
            return message.isTextual() ? message.asText() : "";
        } catch (Exception ignored) {
            return "";
        }
    }
}

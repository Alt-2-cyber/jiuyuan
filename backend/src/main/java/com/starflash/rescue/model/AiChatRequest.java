package com.starflash.rescue.model;

import jakarta.validation.constraints.NotBlank;

import java.util.List;

public record AiChatRequest(
    @NotBlank String message,
    List<AiChatMessage> history
) {
}

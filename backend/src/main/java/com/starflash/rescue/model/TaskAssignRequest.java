package com.starflash.rescue.model;

import jakarta.validation.constraints.NotBlank;

public record TaskAssignRequest(
    @NotBlank String rescuerId
) {
}

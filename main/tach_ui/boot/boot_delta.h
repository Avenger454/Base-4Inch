#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    const uint8_t *cursor, *end;
    uint16_t palette[256];
    uint16_t frames, period_ms, frame;
} boot_delta_t;
typedef struct { uint16_t x1, y1, x2, y2; bool changed; } boot_rect_t;
bool boot_delta_open(boot_delta_t *d, const uint8_t *data, size_t size);
bool boot_delta_step(boot_delta_t *d, uint16_t *pixels, boot_rect_t *dirty);

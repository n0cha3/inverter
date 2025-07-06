#pragma once
#include <obs-module.h>

typedef struct {
    uint32_t x;
    uint32_t y;
    uint32_t x_hotspot;
    uint32_t y_hotspot;
} position;

gs_texture_t *get_cursor_bitmap(position *position);
#pragma once
#include <obs-module.h>

typedef struct {
    uint32_t x;
    uint32_t y;
} position;

gs_texture_t *get_cursor_bitmap(position *position);
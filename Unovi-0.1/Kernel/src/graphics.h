#ifndef UNOVI_GRAPHICS_H
#define UNOVI_GRAPHICS_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint32_t width;
    uint32_t height;
} GraphicsInfo;

#define COLOR_RGB(r, g, b) \
    (((uint32_t)(r) << 16) | \
     ((uint32_t)(g) << 8)  | \
     ((uint32_t)(b)))

bool graphics_init(void);

GraphicsInfo graphics_get_info(void);

void graphics_clear(uint32_t color);

void graphics_fill_rect(
    int32_t x,
    int32_t y,
    uint32_t width,
    uint32_t height,
    uint32_t color
);

#endif

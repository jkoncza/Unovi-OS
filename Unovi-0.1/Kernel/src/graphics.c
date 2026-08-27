#include "graphics.h"

#include <limine.h>
#include <stdint.h>
#include <stdbool.h>

static volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST_ID,
    .revision = 0,
    .response = 0
};

static struct limine_framebuffer *framebuffer = 0;

static uint32_t framebuffer_width = 0;
static uint32_t framebuffer_height = 0;
static uint32_t framebuffer_pitch = 0;

bool graphics_init(void) {
    if (framebuffer_request.response == 0)
        return false;

    if (framebuffer_request.response->framebuffer_count == 0)
        return false;

    framebuffer =
        framebuffer_request.response->framebuffers[0];

    if (framebuffer->bpp != 32)
        return false;

    if (framebuffer->memory_model != LIMINE_FRAMEBUFFER_RGB)
        return false;

    framebuffer_width =
        (uint32_t)framebuffer->width;

    framebuffer_height =
        (uint32_t)framebuffer->height;

    framebuffer_pitch =
        (uint32_t)framebuffer->pitch;

    return true;
}

static void put_pixel(
    int32_t x,
    int32_t y,
    uint32_t color
) {
    if (framebuffer == 0)
        return;

    if (x < 0 || y < 0)
        return;

    if ((uint32_t)x >= framebuffer_width ||
        (uint32_t)y >= framebuffer_height)
        return;

    uint32_t *pixel =
        (uint32_t *)(
            (uint8_t *)framebuffer->address +
            ((uint32_t)y * framebuffer_pitch) +
            ((uint32_t)x * 4)
        );

    *pixel = color;
}

void graphics_clear(uint32_t color) {
    if (framebuffer == 0)
        return;

    for (uint32_t y = 0; y < framebuffer_height; y++) {
        for (uint32_t x = 0; x < framebuffer_width; x++) {
            put_pixel(
                (int32_t)x,
                (int32_t)y,
                color
            );
        }
    }
}

void graphics_fill_rect(
    int32_t x,
    int32_t y,
    uint32_t width,
    uint32_t height,
    uint32_t color
) {
    if (framebuffer == 0)
        return;

    for (uint32_t yy = 0; yy < height; yy++) {
        for (uint32_t xx = 0; xx < width; xx++) {
            put_pixel(
                x + (int32_t)xx,
                y + (int32_t)yy,
                color
            );
        }
    }
}

GraphicsInfo graphics_get_info(void) {
    GraphicsInfo info = {
        .width = framebuffer_width,
        .height = framebuffer_height
    };

    return info;
}

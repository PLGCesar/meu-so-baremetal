#!/bin/sh
set -e

# Update or create the BGIF specification header
cat << 'EOF' > include/bgif.h
#ifndef BGIF_H
#define BGIF_H

#include <stdint.h>
#include <stddef.h>

/*
 * Capivara OS - BGIF (Baremetal GIF) Frame & Delta Specification
 * Optimized for x86_64 baremetal framebuffer environments without stdlib.
 */

#define BGIF_MAGIC 0x46494742 // 'B', 'G', 'I', 'F' ASCII packed in little-endian

// BGIF Feature Flags
#define BGIF_FLAG_DELTA_ONLY      (1 << 0) // Frame relies solely on pixel diffs
#define BGIF_FLAG_SMOOTH_BLEND    (1 << 1) // Enable smooth color interpolation (anti-flicker)
#define BGIF_FLAG_HAS_TRANSPARENT (1 << 2) // Frame has transparent pixel keying

// Main BGIF File Header
typedef struct __attribute__((packed)) {
    uint32_t magic;           // Magic identifier: "BGIF"
    uint16_t width;           // Image width in pixels
    uint16_t height;          // Image height in pixels
    uint16_t frame_count;     // Total animation frames
    uint16_t flags;           // Global animation flags
    uint16_t default_delay;   // Default delay between frames (ms)
    uint32_t transparent_key; // Key color used for transparency
} bgif_header_t;

// Single Delta Pixel Structure (Packed delta coordinate + target color)
typedef struct __attribute__((packed)) {
    uint16_t x;     // X offset within canvas
    uint16_t y;     // Y offset within canvas
    uint32_t color; // ARGB/XRGB 32-bit pixel value
} bgif_delta_pixel_t;

// Per-frame Header
typedef struct __attribute__((packed)) {
    uint16_t delay_ms;      // Frame specific delay in milliseconds
    uint32_t delta_count;   // Number of changed pixels in this frame
} bgif_frame_header_t;

// Target Framebuffer Surface Configuration
typedef struct {
    uint32_t *base_addr;     // Framebuffer linear memory address
    uint32_t pitch_pixels;   // Scanline pitch in pixels (stride)
    uint16_t screen_width;   // Hardware screen max width constraint
    uint16_t screen_height;  // Hardware screen max height constraint
} bgif_render_target_t;

// Status & Error Codes
#define BGIF_OK                   0
#define BGIF_ERR_NULL_POINTER    -1
#define BGIF_ERR_INVALID_MAGIC   -2
#define BGIF_ERR_INVALID_SIZE    -3
#define BGIF_ERR_OUT_OF_BOUNDS   -4
#define BGIF_ERR_BUFFER_OVERFLOW -5

// Function Declarations
int bgif_validate_header(const bgif_header_t *hdr, size_t data_len);

int bgif_render_delta_frame(
    const bgif_render_target_t *target,
    uint16_t pos_x,
    uint16_t pos_y,
    const bgif_header_t *bgif_hdr,
    const bgif_frame_header_t *frame_hdr,
    const bgif_delta_pixel_t *deltas,
    size_t available_bytes,
    uint8_t blend_factor
);

#endif // BGIF_H
EOF

# Update or create the BGIF decoder and delta renderer implementation
cat << 'EOF' > src/bgif.c
#include "../include/bgif.h"

/*
 * Linear fixed-point RGB color interpolation (Anti-Flicker / Smooth Color Transitions)
 * Prevents abrupt color shifts ("não mudar muito a cor") between sequential delta updates.
 * Freestanding integer math: zero floats, safe against underflow/overflow.
 */
static inline uint32_t bgif_blend_rgb(uint32_t current_color, uint32_t new_color, uint8_t blend_factor) {
    if (blend_factor == 255) return new_color;
    if (blend_factor == 0)   return current_color;

    uint32_t r_cur = (current_color >> 16) & 0xFF;
    uint32_t g_cur = (current_color >> 8)  & 0xFF;
    uint32_t b_cur = current_color & 0xFF;

    uint32_t r_new = (new_color >> 16) & 0xFF;
    uint32_t g_new = (new_color >> 8)  & 0xFF;
    uint32_t b_new = new_color & 0xFF;

    int32_t r_diff = (int32_t)r_new - (int32_t)r_cur;
    int32_t g_diff = (int32_t)g_new - (int32_t)g_cur;
    int32_t b_diff = (int32_t)b_new - (int32_t)b_cur;

    uint32_t r_out = (uint32_t)((int32_t)r_cur + ((r_diff * blend_factor) >> 8));
    uint32_t g_out = (uint32_t)((int32_t)g_cur + ((g_diff * blend_factor) >> 8));
    uint32_t b_out = (uint32_t)((int32_t)b_cur + ((b_diff * blend_factor) >> 8));

    return (current_color & 0xFF000000) | ((r_out & 0xFF) << 16) | ((g_out & 0xFF) << 8) | (b_out & 0xFF);
}

int bgif_validate_header(const bgif_header_t *hdr, size_t data_len) {
    if (!hdr) {
        return BGIF_ERR_NULL_POINTER;
    }
    if (data_len < sizeof(bgif_header_t)) {
        return BGIF_ERR_BUFFER_OVERFLOW;
    }
    if (hdr->magic != BGIF_MAGIC) {
        return BGIF_ERR_INVALID_MAGIC;
    }
    if (hdr->width == 0 || hdr->height == 0) {
        return BGIF_ERR_INVALID_SIZE;
    }

    return BGIF_OK;
}

int bgif_render_delta_frame(
    const bgif_render_target_t *target,
    uint16_t pos_x,
    uint16_t pos_y,
    const bgif_header_t *bgif_hdr,
    const bgif_frame_header_t *frame_hdr,
    const bgif_delta_pixel_t *deltas,
    size_t available_bytes,
    uint8_t blend_factor
) {
    // Null pointer verification
    if (!target || !target->base_addr || !bgif_hdr || !frame_hdr || !deltas) {
        return BGIF_ERR_NULL_POINTER;
    }

    // Arithmetic overflow protection on total width/height position calculations
    uint32_t target_max_x = (uint32_t)pos_x + bgif_hdr->width;
    uint32_t target_max_y = (uint32_t)pos_y + bgif_hdr->height;

    if (target_max_x > target->screen_width || target_max_y > target->screen_height) {
        return BGIF_ERR_OUT_OF_BOUNDS;
    }

    // Multiplication overflow prevention when checking buffer payload bounds
    uint64_t required_bytes = (uint64_t)frame_hdr->delta_count * sizeof(bgif_delta_pixel_t);
    if (required_bytes > available_bytes) {
        return BGIF_ERR_BUFFER_OVERFLOW;
    }

    const uint32_t flags = bgif_hdr->flags;
    const uint32_t trans_key = bgif_hdr->transparent_key;
    const uint32_t pitch = target->pitch_pixels;
    uint32_t *fb = target->base_addr;

    // Delta-only processing loop (avoiding full-frame repaints)
    for (uint32_t i = 0; i < frame_hdr->delta_count; ++i) {
        const bgif_delta_pixel_t delta = deltas[i];

        // Bounds validation per delta coordinate to prevent heap/stack buffer corruption
        if (delta.x >= bgif_hdr->width || delta.y >= bgif_hdr->height) {
            continue;
        }

        uint32_t abs_x = (uint32_t)pos_x + delta.x;
        uint32_t abs_y = (uint32_t)pos_y + delta.y;

        size_t fb_index = (size_t)abs_y * pitch + abs_x;

        // Skip key color transparency
        if ((flags & BGIF_FLAG_HAS_TRANSPARENT) && (delta.color == trans_key)) {
            continue;
        }

        uint32_t current_pixel = fb[fb_index];

        // Skip write if destination pixel color is already identical
        if (current_pixel == delta.color) {
            continue;
        }

        // Apply smooth color transition if smooth blend flag is enabled or blend_factor < 255
        uint32_t final_color;
        if ((flags & BGIF_FLAG_SMOOTH_BLEND) || blend_factor < 255) {
            final_color = bgif_blend_rgb(current_pixel, delta.color, blend_factor);
        } else {
            final_color = delta.color;
        }

        fb[fb_index] = final_color;
    }

    return BGIF_OK;
}
EOF
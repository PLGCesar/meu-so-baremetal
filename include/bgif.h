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

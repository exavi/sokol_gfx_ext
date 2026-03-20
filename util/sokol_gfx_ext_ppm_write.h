#ifndef SOKOL_GFX_EXT_PPM_WRITE_INCLUDED
#define SOKOL_GFX_EXT_PPM_WRITE_INCLUDED (1)

#include <stdio.h>
#include <stdint.h>

typedef enum {
    SGEXT_PIXEL_FORMAT_BGRA,  // Metal format (BGRA8)
    SGEXT_PIXEL_FORMAT_RGBA   // OpenGL format (RGBA8)
} sgext_pixel_format_type;

// Helper function to write PPM file from pixel buffer
// Supports both BGRA and RGBA formats, with optional Y-axis flip
inline bool sgext_write_ppm_file(
    const char* filename,
    int width,
    int height,
    const uint8_t* pixels,
    sgext_pixel_format_type format,
    bool flip_y)
{
    FILE* f = fopen(filename, "w");
    if (!f)
        return false;

    fprintf(f, "P3\n%d %d\n255\n", width, height);

    // Write pixels in RGB format
    if (flip_y) {
        // Bottom-to-top (for OpenGL)
        for (int y = height - 1; y >= 0; y--) {
            for (int x = 0; x < width; x++) {
                int idx = (y * width + x) * 4;
                uint8_t r, g, b;
                if (format == SGEXT_PIXEL_FORMAT_BGRA) {
                    b = pixels[idx + 0];
                    g = pixels[idx + 1];
                    r = pixels[idx + 2];
                } else {  // RGBA
                    r = pixels[idx + 0];
                    g = pixels[idx + 1];
                    b = pixels[idx + 2];
                }
                fprintf(f, "%d %d %d ", r, g, b);
            }
            fprintf(f, "\n");
        }
    } else {
        // Top-to-bottom (for Metal)
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                int idx = (y * width + x) * 4;
                uint8_t r, g, b;
                if (format == SGEXT_PIXEL_FORMAT_BGRA) {
                    b = pixels[idx + 0];
                    g = pixels[idx + 1];
                    r = pixels[idx + 2];
                } else {  // RGBA
                    r = pixels[idx + 0];
                    g = pixels[idx + 1];
                    b = pixels[idx + 2];
                }
                fprintf(f, "%d %d %d ", r, g, b);
            }
            fprintf(f, "\n");
        }
    }

    fclose(f);
    return true;
}

#endif // SOKOL_GFX_EXT_PPM_WRITE_INCLUDED
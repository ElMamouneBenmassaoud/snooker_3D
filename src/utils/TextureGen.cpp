#include "TextureGen.h"
#include <stb_image_write.h>
#include <stb_perlin.h>
#include <vector>
#include <cmath>

static const int TEX_SIZE = 512;

void generateFeltTexture(const std::string& path) {
    std::vector<unsigned char> pixels(TEX_SIZE * TEX_SIZE * 3);

    for (int y = 0; y < TEX_SIZE; y++) {
        for (int x = 0; x < TEX_SIZE; x++) {
            float fx = x / (float)TEX_SIZE * 8.0f;
            float fy = y / (float)TEX_SIZE * 8.0f;

            float noise = stb_perlin_noise3(fx, fy, 0.0f, 0, 0, 0);
            noise += 0.5f * stb_perlin_noise3(fx * 2, fy * 2, 0.5f, 0, 0, 0);
            noise = noise * 0.5f + 0.5f;

            int r = (int)(18  + noise * 20);
            int g = (int)(110 + noise * 30);
            int b = (int)(18  + noise * 20);

            int idx = (y * TEX_SIZE + x) * 3;
            pixels[idx + 0] = (unsigned char)r;
            pixels[idx + 1] = (unsigned char)g;
            pixels[idx + 2] = (unsigned char)b;
        }
    }

    stbi_write_png(path.c_str(), TEX_SIZE, TEX_SIZE, 3, pixels.data(), TEX_SIZE * 3);
}

void generateWoodTexture(const std::string& path) {
    std::vector<unsigned char> pixels(TEX_SIZE * TEX_SIZE * 3);

    for (int y = 0; y < TEX_SIZE; y++) {
        for (int x = 0; x < TEX_SIZE; x++) {
            float fx = x / (float)TEX_SIZE * 6.0f;
            float fy = y / (float)TEX_SIZE * 6.0f;

            float grain = sin(fy * 20.0f + stb_perlin_noise3(fx, fy, 0.0f, 0, 0, 0) * 5.0f);
            grain = grain * 0.5f + 0.5f;

            float noise = stb_perlin_noise3(fx * 3, fy * 0.5f, 0.0f, 0, 0, 0) * 0.5f + 0.5f;
            float v = grain * 0.7f + noise * 0.3f;

            int r = (int)(100 + v * 60);
            int g = (int)(55  + v * 35);
            int b = (int)(10  + v * 15);

            int idx = (y * TEX_SIZE + x) * 3;
            pixels[idx + 0] = (unsigned char)r;
            pixels[idx + 1] = (unsigned char)g;
            pixels[idx + 2] = (unsigned char)b;
        }
    }

    stbi_write_png(path.c_str(), TEX_SIZE, TEX_SIZE, 3, pixels.data(), TEX_SIZE * 3);
}

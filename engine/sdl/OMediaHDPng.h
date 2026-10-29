// port: bounded PNG decoding for optional HD packs. No game data or GL needed.
#pragma once
#include <string>
#include <vector>
#include <stddef.h>
namespace omhd {
struct DecodedPng {
    unsigned int width, height;
    std::vector<unsigned char> pixels;
    DecodedPng() : width(0), height(0) {}
};
// Error text or NULL; never publishes a partial image on failure.
const char *decode_png(const std::string &file, DecodedPng &image, size_t budget);
}

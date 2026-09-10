#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace Orbit {

struct RGBAImage {
    int width = 0;
    int height = 0;
    std::vector<uint8_t> pixels;
};

struct Card {
    RGBAImage image;

    float x = 0.0f;
    float y = 0.0f;
    float radius = 0.0f;
};

bool loadPNG(const std::string& path, RGBAImage& out);

bool makeCircularCard(
    const RGBAImage& source,
    int diameter,
    Card& out
);

bool savePNG(
    const RGBAImage& image,
    const std::string& path
);


struct OverlayConfig {
    int screenWidth = 1920;
    int screenHeight = 1080;
    float centerX = 0.0f;
    float centerY = 0.0f;
    float orbitRadius = 340.0f;
    float cardRadius = 100.0f;
    int activeIndex = -1;
};


bool renderOverlay(
    const std::vector<Card>& cards,
    const OverlayConfig& config,
    RGBAImage& out
);

} // namespace Orbit

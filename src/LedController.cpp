// LedController.cpp
#include "LedController.h"

#include <algorithm>

// Single-strip constructor: map positions sequentially starting at basePixel
LedController::LedController(Adafruit_NeoPixel& strip, uint16_t basePixel) {
    strips_.push_back(&strip);
    for (size_t i = 0; i < POS_COUNT; ++i) {
        mapping_[i].strip = 0;
        mapping_[i].pixelIdx = basePixel + static_cast<uint16_t>(i);
    }
}

// Explicit mapping constructor
LedController::LedController(const std::vector<Adafruit_NeoPixel*>& strips,
                             const std::array<Location, POS_COUNT>& mapping)
    : strips_(strips), mapping_(mapping) {}

// Hardcoded mapping constructor
LedController::LedController(const std::vector<Adafruit_NeoPixel*>& strips, bool useHardcodedMapping)
    : strips_(strips)
{
    // Default to safe values
    for (size_t i = 0; i < POS_COUNT; ++i) {
        mapping_[i].strip = 0;
        mapping_[i].pixelIdx = 0;
    }

    if (!useHardcodedMapping) return;

    if (strips_.size() >= 2) {
        // Map A..O (0..14) -> strip 0, pixels 0..14
        for (size_t i = 0; i < 15 && i < POS_COUNT; ++i) {
            mapping_[i].strip = 0;
            mapping_[i].pixelIdx = static_cast<uint16_t>(i);
        }
        // Map P..Y (15..24) -> strip 1, pixels 0..9
        for (size_t i = 15; i < POS_COUNT; ++i) {
            mapping_[i].strip = 1;
            mapping_[i].pixelIdx = static_cast<uint16_t>(i - 15);
        }
    } else if (strips_.size() == 1) {
        // Single strip: map A..Y to pixels 0..24
        for (size_t i = 0; i < POS_COUNT; ++i) {
            mapping_[i].strip = 0;
            mapping_[i].pixelIdx = static_cast<uint16_t>(i);
        }
    }
}

int LedController::posIndexFromChar(char position) const {
    if (position >= 'A' && position <= 'Z') {
        int idx = position - 'A';
        if (idx >= 0 && idx < static_cast<int>(POS_COUNT)) return idx;
    }
    if (position >= 'a' && position <= 'z') {
        int idx = position - 'a';
        if (idx >= 0 && idx < static_cast<int>(POS_COUNT)) return idx;
    }
    return -1;
}

void LedController::setPixel(const Location& loc, uint32_t color) {
    if (loc.strip >= strips_.size()) return;
    Adafruit_NeoPixel* s = strips_[loc.strip];
    if (!s) return;
    if (loc.pixelIdx >= s->numPixels()) return;
    s->setPixelColor(loc.pixelIdx, color);
}

void LedController::applyShowsForModifiedStrips(const std::vector<bool>& modified) {
    for (size_t i = 0; i < strips_.size(); ++i) {
        if (modified[i] && strips_[i]) strips_[i]->show();
    }
}

void LedController::show(char position, uint8_t r, uint8_t g, uint8_t b) {
    int idx = posIndexFromChar(position);
    if (idx < 0) return;
    Location loc = mapping_[idx];
    std::vector<bool> modified(strips_.size(), false);
    setPixel(loc, strips_[loc.strip]->Color(r, g, b));
    modified[loc.strip] = true;
    applyShowsForModifiedStrips(modified);
}

void LedController::show(char position) {
    // default color: soft white
    show(position, 40, 40, 40);
}

void LedController::hide(char position, uint8_t radius) {
    int center = posIndexFromChar(position);
    if (center < 0) return;
    std::vector<bool> modified(strips_.size(), false);
    int start = std::max(0, center - static_cast<int>(radius));
    int end = std::min(static_cast<int>(POS_COUNT) - 1, center + static_cast<int>(radius));
    for (int i = start; i <= end; ++i) {
        Location loc = mapping_[i];
        setPixel(loc, 0); // off
        modified[loc.strip] = true;
    }
    applyShowsForModifiedStrips(modified);
}

void LedController::clearAll() {
    std::vector<bool> modified(strips_.size(), false);
    for (size_t i = 0; i < POS_COUNT; ++i) {
        Location loc = mapping_[i];
        setPixel(loc, 0);
        modified[loc.strip] = true;
    }
    applyShowsForModifiedStrips(modified);
}

void LedController::success(char position) {
    int center = posIndexFromChar(position);
    if (center < 0) return;

    // Colors and timing
    const uint8_t centerR = 0, centerG = 150, centerB = 0; // bright green
    const uint8_t ringR = 0, ringG = 80, ringB = 0;
    const unsigned long stepDelay = 70; // ms
    const int maxRadius = 5;

    // Expand outward step by step; set pixels, show, then continue
    for (int step = 0; step <= maxRadius; ++step) {
        int start = std::max(0, center - step);
        int end = std::min(static_cast<int>(POS_COUNT) - 1, center + step);
        std::vector<bool> modified(strips_.size(), false);
        for (int i = start; i <= end; ++i) {
            Location loc = mapping_[i];
            if (i == center) {
                setPixel(loc, strips_[loc.strip]->Color(centerR, centerG, centerB));
            } else {
                setPixel(loc, strips_[loc.strip]->Color(ringR, ringG, ringB));
            }
            modified[loc.strip] = true;
        }
        applyShowsForModifiedStrips(modified);
        delay(stepDelay);
    }

    // Hold briefly then clear the expanded region
    delay(200);
    hide(position, maxRadius);
}

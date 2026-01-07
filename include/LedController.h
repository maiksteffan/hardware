// LedController.h
#ifndef LEDCONTROLLER_H
#define LEDCONTROLLER_H

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <vector>
#include <array>

class LedController {
public:
    struct Location {
        uint8_t strip;     // index into strips_ vector
        uint16_t pixelIdx; // pixel index on that strip
    };

    static constexpr size_t POS_COUNT = 25; // positions A..Y

    // Construct for a single strip: positions map sequentially from basePixel
    LedController(Adafruit_NeoPixel& strip, uint16_t basePixel = 0);

    // Construct with explicit strips vector and mapping for each position
    LedController(const std::vector<Adafruit_NeoPixel*>& strips,
                  const std::array<Location, POS_COUNT>& mapping);

    // Construct with explicit strips and a hardcoded mapping for positions A..Y.
    // If two strips are provided, positions A..O map to strip 0 indices 0..14
    // and P..Y map to strip 1 indices 0..9. If only one strip is provided,
    // positions map sequentially to that strip 0..24.
    LedController(const std::vector<Adafruit_NeoPixel*>& strips, bool useHardcodedMapping);

    // Light a single logical position with provided RGB
    void show(char position, uint8_t r, uint8_t g, uint8_t b);
    void show(char position); // default white-ish color

    // Success animation: expand 5 positions left and right from position
    void success(char position);

    // Hide (clear) the position and surrounding positions within radius
    void hide(char position, uint8_t radius = 1);

    // Clear all managed positions on all strips
    void clearAll();

private:
    std::vector<Adafruit_NeoPixel*> strips_;
    std::array<Location, POS_COUNT> mapping_;

    // Helpers
    int posIndexFromChar(char position) const; // returns 0..24 or -1
    void setPixel(const Location& loc, uint32_t color);
    void applyShowsForModifiedStrips(const std::vector<bool>& modified);
};

#endif // LEDCONTROLLER_H

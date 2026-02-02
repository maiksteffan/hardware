# MockPi - Arduino Command Tester

This folder contains a testing utility for verifying the Arduino's command handling without a real Raspberry Pi.

## Overview

MockPi simulates a Raspberry Pi by:
1. Directly polling the TouchController for touch states
2. Injecting commands into the CommandController
3. Running sequences for testing

## Two Programs

### 1. Play Mode
Plays a predefined sequence like `"A,B,C,D+E,F"`:
- Shows LEDs
- Waits for touches
- Plays SUCCESS animations
- Signals release with BLINK
- Waits for releases
- Hides LEDs
- Plays SEQUENCE_COMPLETED at the end

### 2. Record Mode
Records your touches and plays them back:
1. Touch positions on the board
2. SUCCESS animation plays for visual feedback
3. Release positions (simultaneous releases within 500ms become pairs)
4. Wait 2 seconds with no touches to finalize
5. Recorded sequence auto-plays

## Configuration

Edit `MockPiConfig.h`:

```cpp
// Select mode
#define MOCK_PI_MODE MOCK_PI_MODE_PLAY   // or MOCK_PI_MODE_RECORD

// For PLAY mode - set the sequence
#define MOCK_PI_SEQUENCE "A,B,C,D+E,F,G+H,I,J,K"
```

## Sequence Format

- Single positions: `A,B,C`
- Simultaneous pairs: `A+B,C+D`
- Mixed: `A,B,C+D,E,F+G,H`

## Integration

To use MockPi in your build, you need to:
1. Include the Arduino headers
2. Create instances of MockPi, CommandController, and TouchController
3. Call `mockPi.tick()` in your main loop

This module is intended for development/testing only and should be removed for production deployment on the Raspberry Pi.

## Note

The MockPi folder will eventually be removed from this project when the real Raspberry Pi implementation is done. The Arduino firmware in the `Arduino/` folder will continue to work the same way - it only cares about serial commands, regardless of whether they come from MockPi or a real Raspberry Pi.

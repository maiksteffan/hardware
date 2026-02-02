/**
 * @file MockPiRecorder.h
 * @brief Records touch sequences for later playback
 * 
 * Recording logic (based on RELEASES, not touches):
 * - Touch positions → Show SUCCESS animation (visual feedback only)
 * - Release a position → Start release window, record when window closes
 * - Release another position within window → Add to same token (pair)
 * - After IDLE_TIMEOUT_MS with no active touches → Finalize sequence
 * 
 * Example movement: touch A, touch B, release A, touch C, release B...
 *   → Records based on release order: "A,B,..."
 * 
 * Simultaneous release: release A+B within 500ms
 *   → Records "A+B" as a pair token
 * 
 * Usage:
 *   MockPI::MockPiRecorder recorder;
 *   recorder.begin(&Serial, &commandController, &touchController);
 *   recorder.startRecording();
 *   // In loop:
 *   recorder.tick();
 *   if (recorder.isComplete()) {
 *       const char* seq = recorder.getRecordedSequence();
 *   }
 */

#ifndef MOCK_PI_RECORDER_H
#define MOCK_PI_RECORDER_H

#include <Arduino.h>
#include "Types.h"

// Forward declarations
class CommandController;
class TouchController;

namespace MockPI {

// ============================================================================
// Configuration
// ============================================================================

constexpr uint32_t RECORD_SIMUL_WINDOW_MS = 500;   // Time window for simultaneous releases
constexpr uint32_t RECORD_IDLE_TIMEOUT_MS = 2000;  // Time with no active touches to finalize
constexpr size_t   MAX_SEQUENCE_LENGTH = 128;      // Max recorded sequence string length
constexpr uint8_t  MAX_SIMUL_RELEASES = 4;         // Max simultaneous releases in one token

// ============================================================================
// Recorder State
// ============================================================================

enum class RecorderState : uint8_t {
    IDLE,              // Not recording
    WAITING_TOUCH,     // Waiting for first touch
    TRACKING,          // Tracking active touches
    RELEASE_WINDOW,    // Within release window, collecting releases
    COMPLETE           // Recording complete, sequence available
};

// ============================================================================
// MockPiRecorder Class
// ============================================================================

class MockPiRecorder {
public:
    MockPiRecorder();
    
    /**
     * @brief Initialize
     * @param serial For logging
     * @param cmdController For injecting LED commands
     * @param touchController For polling touch state
     */
    void begin(Stream* serial, CommandController* cmdController, TouchController* touchController);
    
    /**
     * @brief Start recording a new sequence
     */
    void startRecording();
    
    /**
     * @brief Stop recording (finalize current sequence)
     */
    void stopRecording();
    
    /**
     * @brief Update - call every loop iteration
     */
    void tick();
    
    /**
     * @brief Check if recording is in progress
     */
    bool isRecording() const;
    
    /**
     * @brief Check if recording is complete and sequence is available
     */
    bool isComplete() const;
    
    /**
     * @brief Get the recorded sequence string
     * @return Sequence string (e.g., "A,B,C+D,E") or empty if not complete
     */
    const char* getRecordedSequence() const;
    
    /**
     * @brief Clear the recorded sequence
     */
    void clearSequence();
    
    /**
     * @brief Enable/disable verbose logging
     */
    void setVerbose(bool v) { _verbose = v; }

private:
    Stream* _serial;
    CommandController* _cmdController;
    TouchController* _touchController;
    
    RecorderState _state;
    bool _verbose;
    
    // Recorded sequence string
    char _sequence[MAX_SEQUENCE_LENGTH];
    size_t _sequenceLen;
    
    // Touch state tracking
    uint32_t _prevTouchedMask;      // Previous loop's touch state
    uint32_t _currentTouchedMask;   // Positions currently touched
    uint32_t _shownSuccessMask;     // Positions that have shown SUCCESS (to avoid double animation)
    
    // Release window tracking
    uint8_t _pendingReleases[MAX_SIMUL_RELEASES];  // Positions released in current window
    uint8_t _pendingReleaseCount;
    uint32_t _releaseWindowStart;   // Time when release window started
    
    // Timing
    uint32_t _lastReleaseTime;      // Time of last release (for idle detection)
    
    // === Helpers ===
    void log(const char* msg);
    void logf(const char* fmt, ...);
    
    // === Touch handling ===
    void updateTouchMask();
    bool isTouched(uint8_t pos) const;
    bool justTouched(uint8_t pos) const;
    bool justReleased(uint8_t pos) const;
    uint8_t countActiveTouches() const;
    
    // === Recording logic ===
    void handleNewTouch(uint8_t pos);
    void handleRelease(uint8_t pos);
    void commitPendingReleases();
    void appendToSequence(const char* token);
    void finalize();
    
    // === LED commands ===
    void sendCommand(const char* cmd, uint8_t pos);
};

} // namespace MockPI

#endif // MOCK_PI_RECORDER_H

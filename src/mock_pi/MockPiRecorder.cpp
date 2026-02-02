/**
 * @file MockPiRecorder.cpp
 * @brief Implementation of sequence recorder (release-based recording)
 * 
 * Records sequence based on when positions are RELEASED, not when touched.
 * This matches how the player expects sequences to work:
 * - Touch A, Touch B, Release A → Records A
 * - Touch C, Release B → Records B
 * - Release C+D simultaneously → Records C+D
 */

#include "MockPiRecorder.h"
#include "CommandController.h"
#include "TouchController.h"
#include <stdarg.h>

namespace MockPI {

// ============================================================================
// Constructor
// ============================================================================

MockPiRecorder::MockPiRecorder()
    : _serial(nullptr)
    , _cmdController(nullptr)
    , _touchController(nullptr)
    , _state(RecorderState::IDLE)
    , _verbose(true)
    , _sequenceLen(0)
    , _prevTouchedMask(0)
    , _currentTouchedMask(0)
    , _shownSuccessMask(0)
    , _pendingReleaseCount(0)
    , _releaseWindowStart(0)
    , _lastReleaseTime(0)
{
    _sequence[0] = '\0';
    for (uint8_t i = 0; i < MAX_SIMUL_RELEASES; i++) {
        _pendingReleases[i] = INVALID_POS;
    }
}

// ============================================================================
// Public Methods
// ============================================================================

void MockPiRecorder::begin(Stream* serial, CommandController* cmdController, TouchController* touchController) {
    _serial = serial;
    _cmdController = cmdController;
    _touchController = touchController;
    _state = RecorderState::IDLE;
    
    if (_verbose && _serial) {
        _serial->println("RECORDER> Initialized (release-based recording)");
    }
}

void MockPiRecorder::startRecording() {
    clearSequence();
    _state = RecorderState::WAITING_TOUCH;
    _prevTouchedMask = 0;
    _currentTouchedMask = 0;
    _shownSuccessMask = 0;
    _pendingReleaseCount = 0;
    _releaseWindowStart = 0;
    _lastReleaseTime = 0;
    
    for (uint8_t i = 0; i < MAX_SIMUL_RELEASES; i++) {
        _pendingReleases[i] = INVALID_POS;
    }
    
    log("Recording started (release-based)");
    log("Touch positions, releases are recorded");
    log("Simultaneous releases (within 500ms) = pair");
    log("Release all and wait 2s to finalize");
}

void MockPiRecorder::stopRecording() {
    if (_state != RecorderState::IDLE && _state != RecorderState::COMPLETE) {
        finalize();
    }
}

void MockPiRecorder::tick() {
    if (_state == RecorderState::IDLE || _state == RecorderState::COMPLETE) {
        return;
    }
    
    // Update touch state
    updateTouchMask();
    
    // Process new touches (show SUCCESS animation)
    for (uint8_t i = 0; i < NUM_POSITIONS; i++) {
        if (justTouched(i)) {
            handleNewTouch(i);
        }
    }
    
    // Process releases (add to pending releases)
    for (uint8_t i = 0; i < NUM_POSITIONS; i++) {
        if (justReleased(i)) {
            handleRelease(i);
        }
    }
    
    // Check release window timeout
    if (_state == RecorderState::RELEASE_WINDOW) {
        if (millis() - _releaseWindowStart >= RECORD_SIMUL_WINDOW_MS) {
            // Window closed - commit pending releases
            commitPendingReleases();
        }
    }
    
    // Check for idle timeout (no touches and last release was > 2s ago)
    if (_state == RecorderState::TRACKING || _state == RecorderState::WAITING_TOUCH) {
        if (_sequenceLen > 0 && countActiveTouches() == 0 && _lastReleaseTime > 0) {
            if (millis() - _lastReleaseTime >= RECORD_IDLE_TIMEOUT_MS) {
                finalize();
            }
        }
    }
}

bool MockPiRecorder::isRecording() const {
    return _state != RecorderState::IDLE && _state != RecorderState::COMPLETE;
}

bool MockPiRecorder::isComplete() const {
    return _state == RecorderState::COMPLETE;
}

const char* MockPiRecorder::getRecordedSequence() const {
    return _sequence;
}

void MockPiRecorder::clearSequence() {
    _sequence[0] = '\0';
    _sequenceLen = 0;
    _pendingReleaseCount = 0;
    for (uint8_t i = 0; i < MAX_SIMUL_RELEASES; i++) {
        _pendingReleases[i] = INVALID_POS;
    }
    _state = RecorderState::IDLE;
}

// ============================================================================
// Touch State Tracking
// ============================================================================

void MockPiRecorder::updateTouchMask() {
    _prevTouchedMask = _currentTouchedMask;
    _currentTouchedMask = 0;
    
    for (uint8_t i = 0; i < NUM_POSITIONS; i++) {
        if (_touchController && _touchController->isTouched(i)) {
            _currentTouchedMask |= (1UL << i);
        }
    }
}

bool MockPiRecorder::isTouched(uint8_t pos) const {
    if (pos >= NUM_POSITIONS) return false;
    return (_currentTouchedMask & (1UL << pos)) != 0;
}

bool MockPiRecorder::justTouched(uint8_t pos) const {
    if (pos >= NUM_POSITIONS) return false;
    uint32_t mask = 1UL << pos;
    return ((_currentTouchedMask & mask) != 0) && ((_prevTouchedMask & mask) == 0);
}

bool MockPiRecorder::justReleased(uint8_t pos) const {
    if (pos >= NUM_POSITIONS) return false;
    uint32_t mask = 1UL << pos;
    return ((_currentTouchedMask & mask) == 0) && ((_prevTouchedMask & mask) != 0);
}

uint8_t MockPiRecorder::countActiveTouches() const {
    uint8_t count = 0;
    for (uint8_t i = 0; i < NUM_POSITIONS; i++) {
        if (_currentTouchedMask & (1UL << i)) {
            count++;
        }
    }
    return count;
}

// ============================================================================
// Recording Logic
// ============================================================================

void MockPiRecorder::handleNewTouch(uint8_t pos) {
    // Only show SUCCESS if we haven't already for this position
    if (!(_shownSuccessMask & (1UL << pos))) {
        logf("Touch %c - showing SUCCESS", posToChar(pos));
        sendCommand("SUCCESS", pos);
        _shownSuccessMask |= (1UL << pos);
    }
    
    // Transition to tracking state
    if (_state == RecorderState::WAITING_TOUCH) {
        _state = RecorderState::TRACKING;
    }
}

void MockPiRecorder::handleRelease(uint8_t pos) {
    // Only record if this position had shown SUCCESS (was actively touched)
    if (!(_shownSuccessMask & (1UL << pos))) {
        logf("Ignoring release of %c - was not actively tracked", posToChar(pos));
        return;
    }
    
    logf("Release %c", posToChar(pos));
    
    // Clear from shown mask
    _shownSuccessMask &= ~(1UL << pos);
    
    // Hide the LED
    sendCommand("HIDE", pos);
    
    // Add to pending releases
    if (_pendingReleaseCount < MAX_SIMUL_RELEASES) {
        // Check if already in pending (shouldn't happen, but be safe)
        bool alreadyPending = false;
        for (uint8_t i = 0; i < _pendingReleaseCount; i++) {
            if (_pendingReleases[i] == pos) {
                alreadyPending = true;
                break;
            }
        }
        
        if (!alreadyPending) {
            _pendingReleases[_pendingReleaseCount++] = pos;
            
            // Start or extend release window
            if (_state != RecorderState::RELEASE_WINDOW) {
                _releaseWindowStart = millis();
                _state = RecorderState::RELEASE_WINDOW;
                logf("Release window started for %c", posToChar(pos));
            } else {
                logf("Added %c to release window (count=%d)", posToChar(pos), _pendingReleaseCount);
            }
        }
    } else {
        logf("WARNING: Too many simultaneous releases, ignoring %c", posToChar(pos));
    }
    
    _lastReleaseTime = millis();
}

void MockPiRecorder::commitPendingReleases() {
    if (_pendingReleaseCount == 0) {
        _state = RecorderState::TRACKING;
        return;
    }
    
    // Sort positions alphabetically for consistency
    for (uint8_t i = 0; i < _pendingReleaseCount - 1; i++) {
        for (uint8_t j = i + 1; j < _pendingReleaseCount; j++) {
            if (_pendingReleases[j] < _pendingReleases[i]) {
                uint8_t tmp = _pendingReleases[i];
                _pendingReleases[i] = _pendingReleases[j];
                _pendingReleases[j] = tmp;
            }
        }
    }
    
    // Build token string
    char token[16];
    size_t tokenLen = 0;
    
    for (uint8_t i = 0; i < _pendingReleaseCount && tokenLen < sizeof(token) - 2; i++) {
        if (i > 0) {
            token[tokenLen++] = '+';
        }
        token[tokenLen++] = posToChar(_pendingReleases[i]);
    }
    token[tokenLen] = '\0';
    
    logf("Committing release token: %s", token);
    
    // Append to sequence
    appendToSequence(token);
    
    // Clear pending releases
    for (uint8_t i = 0; i < MAX_SIMUL_RELEASES; i++) {
        _pendingReleases[i] = INVALID_POS;
    }
    _pendingReleaseCount = 0;
    
    // Return to tracking state
    _state = RecorderState::TRACKING;
}

void MockPiRecorder::appendToSequence(const char* token) {
    size_t tokenLen = strlen(token);
    
    // Check if we have room
    size_t needed = tokenLen + (_sequenceLen > 0 ? 1 : 0);
    if (_sequenceLen + needed >= MAX_SEQUENCE_LENGTH - 1) {
        log("WARNING: Sequence too long, cannot append");
        return;
    }
    
    // Add comma separator if not first token
    if (_sequenceLen > 0) {
        _sequence[_sequenceLen++] = ',';
    }
    
    // Append token
    strcpy(&_sequence[_sequenceLen], token);
    _sequenceLen += tokenLen;
    
    logf("Sequence so far: %s", _sequence);
}

void MockPiRecorder::finalize() {
    // Commit any pending releases
    if (_pendingReleaseCount > 0) {
        commitPendingReleases();
    }
    
    _state = RecorderState::COMPLETE;
    
    log("======================================");
    logf("Recording complete!");
    logf("Recorded sequence: %s", _sequence);
    log("======================================");
}

// ============================================================================
// LED Commands
// ============================================================================

void MockPiRecorder::sendCommand(const char* cmd, uint8_t pos) {
    if (!_cmdController) return;
    
    char buf[32];
    snprintf(buf, sizeof(buf), "%s %c", cmd, posToChar(pos));
    
    _cmdController->injectCommand(buf);
    
    if (_verbose && _serial) {
        _serial->print("RECORDER> TX: ");
        _serial->println(buf);
    }
}

// ============================================================================
// Logging
// ============================================================================

void MockPiRecorder::log(const char* msg) {
    if (_verbose && _serial) {
        _serial->print("RECORDER> ");
        _serial->println(msg);
    }
}

void MockPiRecorder::logf(const char* fmt, ...) {
    if (!_verbose || !_serial) return;
    
    char buf[128];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    
    _serial->print("RECORDER> ");
    _serial->println(buf);
}

} // namespace MockPI

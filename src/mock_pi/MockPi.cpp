/**
 * @file MockPi.cpp
 * @brief Mock Pi implementation using direct hardware polling
 * 
 * This directly polls TouchController::isTouched() for touch detection
 * and injects commands into CommandController.
 */

#include "MockPi.h"
#include "CommandController.h"
#include "TouchController.h"
#include <stdarg.h>

namespace MockPI {

MockPi::MockPi()
    : _serial(nullptr)
    , _cmdController(nullptr)
    , _touchController(nullptr)
    , _state(MockPiState::IDLE)
    , _currentIdx(0)
    , _stateTime(0)
    , _cmdId(100)  // Start at 100 to distinguish from Arduino's IDs
    , _verbose(true)
    , _got1(false)
    , _got2(false)
    , _firstTouchTime(0)
    , _releaseHead(0)
    , _releaseTail(0)
    , _releasePos1(INVALID_POS)
    , _releasePos2(INVALID_POS)
    , _releaseActive(false)
    , _releaseGot1(false)
    , _releaseGot2(false)
    , _releaseFirstTime(0)
    , _prevTouchedMask(0)
    , _heldPositionsMask(0)
{
    queueClear();
    _sequenceStr[0] = '\0';
}

void MockPi::begin(Stream* serial, CommandController* cmdController, TouchController* touchController) {
    _serial = serial;
    _cmdController = cmdController;
    _touchController = touchController;
    _state = MockPiState::IDLE;
    queueClear();
    
    if (_verbose && _serial) {
        _serial->println("MOCKPI> Initialized (direct hardware mode)");
    }
}

bool MockPi::startSequence(const char* sequence) {
    if (!_cmdController || !_touchController) {
        log("ERROR: Controllers not set");
        return false;
    }
    
    if (!_parser.parse(sequence)) {
        log("ERROR: Failed to parse sequence");
        return false;
    }
    
    // Store sequence for potential restart
    strncpy(_sequenceStr, sequence, sizeof(_sequenceStr) - 1);
    _sequenceStr[sizeof(_sequenceStr) - 1] = '\0';
    
    if (_verbose && _serial) {
        _serial->print("MOCKPI> Parsed: ");
        _parser.print(*_serial);
    }
    
    _currentIdx = 0;
    _state = MockPiState::TOKEN_SHOW;
    _stateTime = millis();
    _prevTouchedMask = 0;
    _heldPositionsMask = 0;
    queueClear();
    _releaseActive = false;
    
    log("Starting sequence");
    return true;
}

void MockPi::tick() {
    if (_state == MockPiState::IDLE || _state == MockPiState::FINISHED || _state == MockPiState::HALTED) {
        return;
    }
    
    // Handle WAIT_FAIL_ANIM state - wait then restart
    if (_state == MockPiState::WAIT_FAIL_ANIM) {
        // Wait 500ms for the fail animation, then restart
        if (millis() - _stateTime >= 500) {
            log("Restarting sequence after fail");
            // Re-parse and restart
            _parser.parse(_sequenceStr);
            _currentIdx = 0;
            _state = MockPiState::TOKEN_SHOW;
            _stateTime = millis();
            _prevTouchedMask = 0;
            _heldPositionsMask = 0;
            queueClear();
            _releaseActive = false;
        }
        return;
    }
    
    // Update touch mask for edge detection
    updateTouchMask();
    
    // Check for wrong release (only if we have held positions and release is active)
    if (_releaseActive && _heldPositionsMask != 0) {
        if (checkWrongRelease()) {
            return;  // Sequence failed, don't process further
        }
    }
    
    // Tick release state machine (runs concurrently)
    if (_releaseActive) {
        tickRelease();
    }
    
    // Tick main token state machine
    tickToken();
}

bool MockPi::isRunning() const {
    return _state != MockPiState::IDLE && 
           _state != MockPiState::FINISHED && 
           _state != MockPiState::HALTED;
}

bool MockPi::isFinished() const {
    return _state == MockPiState::FINISHED || _state == MockPiState::HALTED;
}

// ============================================================================
// Logging
// ============================================================================

void MockPi::log(const char* msg) {
    if (_verbose && _serial) {
        _serial->print("MOCKPI> ");
        _serial->println(msg);
    }
}

void MockPi::logf(const char* fmt, ...) {
    if (_verbose && _serial) {
        char buf[128];
        va_list args;
        va_start(args, fmt);
        vsnprintf(buf, sizeof(buf), fmt, args);
        va_end(args);
        _serial->print("MOCKPI> ");
        _serial->println(buf);
    }
}

// ============================================================================
// Command Sending
// ============================================================================

void MockPi::sendCommand(const char* cmd, uint8_t pos) {
    if (!_cmdController || pos >= NUM_POSITIONS) return;
    
    char buf[64];
    uint32_t id = nextCmdId();
    snprintf(buf, sizeof(buf), "%s %c #%lu", cmd, posToChar(pos), (unsigned long)id);
    
    _cmdController->injectCommand(buf);
    
    if (_verbose && _serial) {
        _serial->print("MOCKPI> TX: ");
        _serial->println(buf);
    }
}

void MockPi::sendCommandNoPos(const char* cmd) {
    if (!_cmdController) return;
    
    char buf[64];
    uint32_t id = nextCmdId();
    snprintf(buf, sizeof(buf), "%s #%lu", cmd, (unsigned long)id);
    
    _cmdController->injectCommand(buf);
    
    if (_verbose && _serial) {
        _serial->print("MOCKPI> TX: ");
        _serial->println(buf);
    }
}

// ============================================================================
// Release Queue
// ============================================================================

void MockPi::queueClear() {
    _releaseHead = 0;
    _releaseTail = 0;
    for (uint8_t i = 0; i < RELEASE_QUEUE_SIZE; i++) {
        _releaseQueue[i] = INVALID_POS;
    }
}

void MockPi::queuePush(uint8_t pos) {
    if (queueCount() >= RELEASE_QUEUE_SIZE) {
        log("WARNING: Release queue full");
        return;
    }
    _releaseQueue[_releaseTail] = pos;
    _releaseTail = (_releaseTail + 1) % RELEASE_QUEUE_SIZE;
    logf("Queue push %c (count=%d)", posToChar(pos), queueCount());
}

uint8_t MockPi::queuePop() {
    if (queueCount() == 0) return INVALID_POS;
    uint8_t pos = _releaseQueue[_releaseHead];
    _releaseQueue[_releaseHead] = INVALID_POS;
    _releaseHead = (_releaseHead + 1) % RELEASE_QUEUE_SIZE;
    logf("Queue pop %c (count=%d)", posToChar(pos), queueCount());
    return pos;
}

uint8_t MockPi::queueCount() const {
    if (_releaseTail >= _releaseHead) {
        return _releaseTail - _releaseHead;
    }
    return RELEASE_QUEUE_SIZE - _releaseHead + _releaseTail;
}

// ============================================================================
// Touch Polling
// ============================================================================

bool MockPi::isTouched(uint8_t pos) const {
    if (!_touchController || pos >= NUM_POSITIONS) return false;
    return _touchController->isTouched(pos);
}

void MockPi::updateTouchMask() {
    _prevTouchedMask = 0;
    for (uint8_t i = 0; i < NUM_POSITIONS; i++) {
        if (isTouched(i)) {
            _prevTouchedMask |= (1UL << i);
        }
    }
}

// ============================================================================
// Pre-Recalibration
// ============================================================================

void MockPi::recalibratePosition(uint8_t pos) {
    if (!_touchController || pos >= NUM_POSITIONS) return;
    
    if (_touchController->recalibrate(pos)) {
        logf("Recalibrated sensor %c", posToChar(pos));
    }
}

void MockPi::recalibrateFuturePositions() {
    // Recalibrate sensors RECALIBRATE_STEPS_AHEAD steps in the future
    if (RECALIBRATE_STEPS_AHEAD == 0) return;
    
    uint8_t futureIdx = _currentIdx + RECALIBRATE_STEPS_AHEAD;
    if (futureIdx >= _parser.count()) return;
    
    const Token& futureToken = _parser.getToken(futureIdx);
    
    logf("Pre-recalibrating for token %d (current=%d)", futureIdx, _currentIdx);
    
    recalibratePosition(futureToken.pos1);
    if (futureToken.isPair()) {
        recalibratePosition(futureToken.pos2);
    }
}

// ============================================================================
// Main Token State Machine
// ============================================================================

void MockPi::tickToken() {
    const Token& token = _parser.getToken(_currentIdx);
    
    switch (_state) {
        // ----------------------------------------------------------------
        case MockPiState::TOKEN_SHOW:
        {
            logf("Token %d: SHOW %c%s%c", _currentIdx, 
                 posToChar(token.pos1),
                 token.isPair() ? "+" : "",
                 token.isPair() ? posToChar(token.pos2) : ' ');
            
            sendCommand("SHOW", token.pos1);
            if (token.isPair()) {
                sendCommand("SHOW", token.pos2);
            }
            
            // Pre-recalibrate sensors that will be needed in future steps
            recalibrateFuturePositions();
            
            _state = MockPiState::TOKEN_EXPECT;
            _stateTime = millis();
            break;
        }
        
        // ----------------------------------------------------------------
        case MockPiState::TOKEN_EXPECT:
        {
            // Small delay between commands
            if (millis() - _stateTime < INTER_CMD_DELAY_MS) return;
            
            sendCommand("EXPECT", token.pos1);
            if (token.isPair()) {
                sendCommand("EXPECT", token.pos2);
            }
            
            _got1 = false;
            _got2 = false;
            _firstTouchTime = 0;
            
            // Start release if we're at idx >= 2
            if (_currentIdx >= 2 && !_releaseActive) {
                startRelease();
            }
            
            _state = MockPiState::TOKEN_WAIT_TOUCH;
            _stateTime = millis();
            break;
        }
        
        // ----------------------------------------------------------------
        case MockPiState::TOKEN_WAIT_TOUCH:
        {
            // Check for timeout
            if (millis() - _stateTime > TOUCH_TIMEOUT_MS) {
                logf("Token %d: TIMEOUT waiting for touch", _currentIdx);
                _state = MockPiState::HALTED;
                return;
            }
            
            // Poll touch state
            if (token.isSingle()) {
                // Single: just need pos1 touched
                if (isTouched(token.pos1)) {
                    logf("Token %d: TOUCHED %c", _currentIdx, posToChar(token.pos1));
                    _state = MockPiState::TOKEN_SUCCESS;
                }
            } else {
                // Pair: need both within SIMUL_WINDOW_MS
                bool t1 = isTouched(token.pos1);
                bool t2 = isTouched(token.pos2);
                
                if (t1 && !_got1) {
                    _got1 = true;
                    if (_firstTouchTime == 0) _firstTouchTime = millis();
                    logf("Token %d: TOUCHED %c (first)", _currentIdx, posToChar(token.pos1));
                }
                if (t2 && !_got2) {
                    _got2 = true;
                    if (_firstTouchTime == 0) _firstTouchTime = millis();
                    logf("Token %d: TOUCHED %c (first)", _currentIdx, posToChar(token.pos2));
                }
                
                if (_got1 && _got2) {
                    // Both touched - check simultaneity
                    uint32_t elapsed = millis() - _firstTouchTime;
                    if (elapsed <= SIMUL_WINDOW_MS) {
                        logf("Token %d: Pair SUCCESS (within %lums)", _currentIdx, elapsed);
                        _state = MockPiState::TOKEN_SUCCESS;
                    } else {
                        // Both touched but NOT simultaneous - show MISTAKE
                        logf("Token %d: Pair FAIL - not simultaneous (took %lums > %lums)", _currentIdx, elapsed, SIMUL_WINDOW_MS);
                        sendCommand("MISTAKE", token.pos1);
                        sendCommand("MISTAKE", token.pos2);
                        // Still advance
                        _state = MockPiState::TOKEN_QUEUE_PUSH;
                        _stateTime = millis();
                    }
                }
                // If only one touched, keep waiting for the other (no early MISTAKE)
            }
            break;
        }
        
        // ----------------------------------------------------------------
        case MockPiState::TOKEN_SUCCESS:
        {
            logf("Token %d: SUCCESS", _currentIdx);
            sendCommand("SUCCESS", token.pos1);
            if (token.isPair()) {
                sendCommand("SUCCESS", token.pos2);
            }
            
            // Mark these positions as held
            _heldPositionsMask |= (1UL << token.pos1);
            if (token.isPair()) {
                _heldPositionsMask |= (1UL << token.pos2);
            }
            
            _state = MockPiState::TOKEN_QUEUE_PUSH;
            _stateTime = millis();
            break;
        }
        
        // ----------------------------------------------------------------
        case MockPiState::TOKEN_QUEUE_PUSH:
        {
            // Wait for SUCCESS animation to complete before continuing
            if (millis() - _stateTime < SUCCESS_ANIM_MS) return;
            
            // Push current positions to release queue
            queuePush(token.pos1);
            if (token.isPair()) {
                queuePush(token.pos2);
            }
            
            _state = MockPiState::TOKEN_ADVANCE;
            break;
        }
        
        // ----------------------------------------------------------------
        case MockPiState::TOKEN_ADVANCE:
        {
            _currentIdx++;
            
            if (_currentIdx >= _parser.count()) {
                // All tokens done - finish releases and complete
                log("All tokens processed");
                _state = MockPiState::COMPLETING;
                _stateTime = millis();
            } else {
                logf("Advancing to token %d", _currentIdx);
                _state = MockPiState::TOKEN_SHOW;
            }
            break;
        }
        
        // ----------------------------------------------------------------
        case MockPiState::COMPLETING:
        {
            // Wait for release to finish
            if (_releaseActive) return;
            
            // Drain release queue (simplified - just hide remaining)
            while (queueCount() > 0) {
                uint8_t pos = queuePop();
                if (pos != INVALID_POS) {
                    sendCommand("STOP_BLINK", pos);
                    sendCommand("HIDE", pos);
                }
            }
            
            log("Sending SEQUENCE_COMPLETED");
            sendCommandNoPos("SEQUENCE_COMPLETED");
            
            _state = MockPiState::FINISHED;
            log("Sequence finished!");
            break;
        }
        
        default:
            break;
    }
}

// ============================================================================
// Release State Machine (runs concurrently)
// ============================================================================

void MockPi::startRelease() {
    const Token& currentToken = _parser.getToken(_currentIdx);
    
    _releasePos1 = INVALID_POS;
    _releasePos2 = INVALID_POS;
    
    if (currentToken.isSingle()) {
        // Pop 1 position for single release
        _releasePos1 = queuePop();
        if (_releasePos1 == INVALID_POS) return;
        
        logf("Starting single release: %c", posToChar(_releasePos1));
    } else {
        // Pop 2 positions for pair release
        _releasePos1 = queuePop();
        _releasePos2 = queuePop();
        if (_releasePos1 == INVALID_POS) return;
        
        if (_releasePos2 != INVALID_POS) {
            logf("Starting pair release: %c+%c", posToChar(_releasePos1), posToChar(_releasePos2));
        } else {
            logf("Starting single release (only 1 in queue): %c", posToChar(_releasePos1));
        }
    }
    
    _releaseActive = true;
    _releaseGot1 = false;
    _releaseGot2 = false;
    _releaseFirstTime = 0;
    
    // Send BLINK commands
    sendCommand("BLINK", _releasePos1);
    if (_releasePos2 != INVALID_POS) {
        sendCommand("BLINK", _releasePos2);
    }
    
    // Send EXPECT_RELEASE commands
    sendCommand("EXPECT_RELEASE", _releasePos1);
    if (_releasePos2 != INVALID_POS) {
        sendCommand("EXPECT_RELEASE", _releasePos2);
    }
}

void MockPi::tickRelease() {
    if (!_releaseActive) return;
    
    bool isSingle = (_releasePos2 == INVALID_POS);
    
    // Check release state by polling (released = NOT touched)
    if (isSingle) {
        if (!isTouched(_releasePos1)) {
            logf("Released %c", posToChar(_releasePos1));
            sendCommand("STOP_BLINK", _releasePos1);
            sendCommand("HIDE", _releasePos1);
            // Remove from held positions
            _heldPositionsMask &= ~(1UL << _releasePos1);
            _releaseActive = false;
        }
    } else {
        // Pair release
        bool r1 = !isTouched(_releasePos1);
        bool r2 = !isTouched(_releasePos2);
        
        if (r1 && !_releaseGot1) {
            _releaseGot1 = true;
            if (_releaseFirstTime == 0) _releaseFirstTime = millis();
            logf("Released %c (waiting for %c)", posToChar(_releasePos1), posToChar(_releasePos2));
        }
        if (r2 && !_releaseGot2) {
            _releaseGot2 = true;
            if (_releaseFirstTime == 0) _releaseFirstTime = millis();
            logf("Released %c (waiting for %c)", posToChar(_releasePos2), posToChar(_releasePos1));
        }
        
        if (_releaseGot1 && _releaseGot2) {
            uint32_t elapsed = millis() - _releaseFirstTime;
            if (elapsed <= SIMUL_WINDOW_MS) {
                logf("Pair release SUCCESS (within %lums)", elapsed);
            } else {
                logf("Pair release FAIL (took %lums)", elapsed);
                sendCommand("MISTAKE", _releasePos1);
                sendCommand("MISTAKE", _releasePos2);
            }
            // Finish release regardless
            sendCommand("STOP_BLINK", _releasePos1);
            sendCommand("HIDE", _releasePos1);
            sendCommand("STOP_BLINK", _releasePos2);
            sendCommand("HIDE", _releasePos2);
            // Remove from held positions
            _heldPositionsMask &= ~(1UL << _releasePos1);
            _heldPositionsMask &= ~(1UL << _releasePos2);
            _releaseActive = false;
        } else if (_releaseFirstTime != 0 && (millis() - _releaseFirstTime > SIMUL_WINDOW_MS)) {
            // Window expired
            logf("Pair release TIMEOUT");
            sendCommand("MISTAKE", _releasePos1);
            sendCommand("MISTAKE", _releasePos2);
            sendCommand("STOP_BLINK", _releasePos1);
            sendCommand("HIDE", _releasePos1);
            sendCommand("STOP_BLINK", _releasePos2);
            sendCommand("HIDE", _releasePos2);
            // Remove from held positions
            _heldPositionsMask &= ~(1UL << _releasePos1);
            _heldPositionsMask &= ~(1UL << _releasePos2);
            _releaseActive = false;
        }
    }
}

// ============================================================================
// Wrong Release Detection
// ============================================================================

bool MockPi::checkWrongRelease() {
    // Check if any held position (that is NOT the expected release) was released
    // Expected release positions are _releasePos1 and _releasePos2
    
    for (uint8_t pos = 0; pos < NUM_POSITIONS; pos++) {
        // Skip if this position is not in held mask
        if (!(_heldPositionsMask & (1UL << pos))) continue;
        
        // Skip if this is an expected release position
        if (pos == _releasePos1 || pos == _releasePos2) continue;
        
        // Check if this position was just released (was touched, now not touched)
        bool wasTouched = (_prevTouchedMask & (1UL << pos)) != 0;
        bool nowTouched = isTouched(pos);
        
        // If it was touched before but NOT now, it was released
        // But we need the PREVIOUS state before updateTouchMask was called
        // Actually, let's check current state - if it's in held mask but not touched, it's wrong
        if (!nowTouched) {
            logf("WRONG RELEASE: Position %c released (expected %c%s%c)", 
                 posToChar(pos),
                 posToChar(_releasePos1),
                 (_releasePos2 != INVALID_POS) ? "+" : "",
                 (_releasePos2 != INVALID_POS) ? posToChar(_releasePos2) : ' ');
            triggerSequenceFail();
            return true;
        }
    }
    
    return false;
}

void MockPi::triggerSequenceFail() {
    log("SEQUENCE_FAIL - wrong position released");
    
    // Hide all LEDs
    hideAllPositions();
    
    // Send SEQUENCE_FAIL command
    sendCommandNoPos("SEQUENCE_FAIL");
    
    // Transition to wait state
    _state = MockPiState::WAIT_FAIL_ANIM;
    _stateTime = millis();
    _releaseActive = false;
}

void MockPi::hideAllPositions() {
    // Stop all blinking and hide all positions
    for (uint8_t i = 0; i < NUM_POSITIONS; i++) {
        if (_heldPositionsMask & (1UL << i)) {
            sendCommand("STOP_BLINK", i);
            sendCommand("HIDE", i);
        }
    }
    
    // Also hide release positions if active
    if (_releasePos1 != INVALID_POS) {
        sendCommand("STOP_BLINK", _releasePos1);
        sendCommand("HIDE", _releasePos1);
    }
    if (_releasePos2 != INVALID_POS) {
        sendCommand("STOP_BLINK", _releasePos2);
        sendCommand("HIDE", _releasePos2);
    }
    
    // Clear held mask
    _heldPositionsMask = 0;
}

} // namespace MockPI

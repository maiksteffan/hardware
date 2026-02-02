/**
 * @file MockPi.cpp
 * @brief Implementation of MockPi testing utility
 */

#include "MockPi.h"
#include "../Arduino/include/CommandController.h"
#include "../Arduino/include/TouchController.h"
#include <stdarg.h>

// ============================================================================
// SequencePlayer Implementation
// ============================================================================

SequencePlayer::SequencePlayer()
    : m_cmdController(nullptr)
    , m_touchController(nullptr)
    , m_tokenCount(0)
    , m_currentToken(0)
    , m_state(State::IDLE)
    , m_stateTime(0)
    , m_cmdId(100)
    , m_verbose(true)
    , m_got1(false)
    , m_got2(false)
    , m_firstTouchTime(0)
    , m_releaseGot1(false)
    , m_releaseGot2(false)
    , m_releaseFirstTime(0)
    , m_prevTouchedMask(0)
{
}

void SequencePlayer::begin(CommandController* cmdController, TouchController* touchController) {
    m_cmdController = cmdController;
    m_touchController = touchController;
    m_state = State::IDLE;
    m_prevTouchedMask = 0;
}

bool SequencePlayer::startSequence(const char* sequence) {
    if (!m_cmdController || !m_touchController) {
        log("ERROR: Controllers not set");
        return false;
    }
    
    if (!parseSequence(sequence)) {
        log("ERROR: Failed to parse sequence");
        return false;
    }
    
    m_currentToken = 0;
    m_state = State::TOKEN_SHOW;
    m_stateTime = millis();
    m_prevTouchedMask = 0;
    
    log("Starting sequence playback");
    return true;
}

bool SequencePlayer::parseSequence(const char* sequence) {
    m_tokenCount = 0;
    
    if (!sequence || *sequence == '\0') return false;
    
    const char* p = sequence;
    
    while (*p != '\0' && m_tokenCount < MOCK_PI_MAX_TOKENS) {
        // Skip whitespace and commas
        while (*p == ' ' || *p == ',') p++;
        if (*p == '\0') break;
        
        // Parse token
        uint8_t pos1 = charToPos(*p);
        if (pos1 == INVALID_POS) {
            p++;
            continue;
        }
        p++;
        
        // Check for pair (e.g., "A+B")
        if (*p == '+') {
            p++;
            uint8_t pos2 = charToPos(*p);
            if (pos2 != INVALID_POS) {
                m_tokens[m_tokenCount++] = Token(pos1, pos2);
                p++;
            } else {
                m_tokens[m_tokenCount++] = Token(pos1);
            }
        } else {
            m_tokens[m_tokenCount++] = Token(pos1);
        }
    }
    
    return m_tokenCount > 0;
}

void SequencePlayer::tick() {
    if (m_state == State::IDLE || m_state == State::FINISHED) {
        return;
    }
    
    updateTouchMask();
    
    const Token& token = m_tokens[m_currentToken];
    
    switch (m_state) {
        case State::TOKEN_SHOW:
            sendCommand("SHOW", token.pos1);
            if (token.isPair()) {
                sendCommand("SHOW", token.pos2);
            }
            m_state = State::TOKEN_EXPECT;
            m_stateTime = millis();
            break;
            
        case State::TOKEN_EXPECT:
            sendCommand("EXPECT", token.pos1);
            if (token.isPair()) {
                sendCommand("EXPECT", token.pos2);
            }
            m_got1 = false;
            m_got2 = !token.isPair();  // If single, consider pos2 as "got"
            m_firstTouchTime = 0;
            m_state = State::TOKEN_WAIT_TOUCH;
            m_stateTime = millis();
            break;
            
        case State::TOKEN_WAIT_TOUCH:
            // Check for touches
            if (!m_got1 && isTouched(token.pos1)) {
                m_got1 = true;
                if (m_firstTouchTime == 0) {
                    m_firstTouchTime = millis();
                }
                logf("Touched %c", posToChar(token.pos1));
            }
            
            if (token.isPair() && !m_got2 && isTouched(token.pos2)) {
                m_got2 = true;
                if (m_firstTouchTime == 0) {
                    m_firstTouchTime = millis();
                }
                logf("Touched %c", posToChar(token.pos2));
            }
            
            // Check if all touches received
            if (m_got1 && m_got2) {
                // For pairs, check they're within simultaneity window
                if (token.isPair() && m_firstTouchTime > 0) {
                    if (millis() - m_firstTouchTime > MOCK_PI_SIMUL_WINDOW_MS) {
                        log("WARN: Simultaneous touch window exceeded");
                    }
                }
                m_state = State::TOKEN_SUCCESS;
                m_stateTime = millis();
            }
            
            // Timeout check
            if (millis() - m_stateTime > MOCK_PI_TOUCH_TIMEOUT_MS) {
                log("ERROR: Touch timeout");
                m_state = State::FINISHED;
            }
            break;
            
        case State::TOKEN_SUCCESS:
            sendCommand("SUCCESS", token.pos1);
            if (token.isPair()) {
                sendCommand("SUCCESS", token.pos2);
            }
            m_state = State::TOKEN_WAIT_ANIM;
            m_stateTime = millis();
            break;
            
        case State::TOKEN_WAIT_ANIM:
            if (millis() - m_stateTime >= MOCK_PI_SUCCESS_ANIM_MS) {
                m_state = State::TOKEN_BLINK;
                m_stateTime = millis();
            }
            break;
            
        case State::TOKEN_BLINK:
            sendCommand("BLINK", token.pos1);
            if (token.isPair()) {
                sendCommand("BLINK", token.pos2);
            }
            m_state = State::TOKEN_EXPECT_RELEASE;
            m_stateTime = millis();
            break;
            
        case State::TOKEN_EXPECT_RELEASE:
            sendCommand("EXPECT_RELEASE", token.pos1);
            if (token.isPair()) {
                sendCommand("EXPECT_RELEASE", token.pos2);
            }
            m_releaseGot1 = false;
            m_releaseGot2 = !token.isPair();
            m_releaseFirstTime = 0;
            m_state = State::TOKEN_WAIT_RELEASE;
            m_stateTime = millis();
            break;
            
        case State::TOKEN_WAIT_RELEASE:
            // Check for releases
            if (!m_releaseGot1 && !isTouched(token.pos1)) {
                m_releaseGot1 = true;
                if (m_releaseFirstTime == 0) {
                    m_releaseFirstTime = millis();
                }
                logf("Released %c", posToChar(token.pos1));
            }
            
            if (token.isPair() && !m_releaseGot2 && !isTouched(token.pos2)) {
                m_releaseGot2 = true;
                if (m_releaseFirstTime == 0) {
                    m_releaseFirstTime = millis();
                }
                logf("Released %c", posToChar(token.pos2));
            }
            
            if (m_releaseGot1 && m_releaseGot2) {
                m_state = State::TOKEN_STOP_BLINK;
                m_stateTime = millis();
            }
            break;
            
        case State::TOKEN_STOP_BLINK:
            sendCommand("STOP_BLINK", token.pos1);
            if (token.isPair()) {
                sendCommand("STOP_BLINK", token.pos2);
            }
            m_state = State::TOKEN_HIDE;
            m_stateTime = millis();
            break;
            
        case State::TOKEN_HIDE:
            sendCommand("HIDE", token.pos1);
            if (token.isPair()) {
                sendCommand("HIDE", token.pos2);
            }
            m_state = State::TOKEN_ADVANCE;
            m_stateTime = millis();
            break;
            
        case State::TOKEN_ADVANCE:
            m_currentToken++;
            if (m_currentToken >= m_tokenCount) {
                m_state = State::COMPLETING;
                m_stateTime = millis();
            } else {
                m_state = State::TOKEN_SHOW;
                m_stateTime = millis();
            }
            break;
            
        case State::COMPLETING:
            sendCommandNoPos("SEQUENCE_COMPLETED");
            log("Sequence completed!");
            m_state = State::FINISHED;
            break;
            
        default:
            break;
    }
}

bool SequencePlayer::isRunning() const {
    return m_state != State::IDLE && m_state != State::FINISHED;
}

bool SequencePlayer::isFinished() const {
    return m_state == State::FINISHED;
}

void SequencePlayer::updateTouchMask() {
    uint32_t newMask = 0;
    for (uint8_t i = 0; i < NUM_POSITIONS; i++) {
        if (m_touchController && m_touchController->isTouched(i)) {
            newMask |= (1UL << i);
        }
    }
    m_prevTouchedMask = newMask;
}

bool SequencePlayer::isTouched(uint8_t pos) const {
    if (pos >= NUM_POSITIONS || !m_touchController) return false;
    return m_touchController->isTouched(pos);
}

bool SequencePlayer::justTouched(uint8_t pos) const {
    if (pos >= NUM_POSITIONS) return false;
    uint32_t mask = 1UL << pos;
    bool nowTouched = isTouched(pos);
    bool wasTouched = (m_prevTouchedMask & mask) != 0;
    return nowTouched && !wasTouched;
}

bool SequencePlayer::justReleased(uint8_t pos) const {
    if (pos >= NUM_POSITIONS) return false;
    uint32_t mask = 1UL << pos;
    bool nowTouched = isTouched(pos);
    bool wasTouched = (m_prevTouchedMask & mask) != 0;
    return !nowTouched && wasTouched;
}

void SequencePlayer::sendCommand(const char* cmd, uint8_t pos) {
    if (!m_cmdController) return;
    
    char buf[64];
    snprintf(buf, sizeof(buf), "%s %c #%lu", cmd, posToChar(pos), (unsigned long)nextCmdId());
    
    // Inject command directly (MockPi uses direct injection, not serial)
    // This would need to be implemented in CommandController
    // For now, we just print what we would send
    if (m_verbose) {
        Serial.print("MOCKPI> TX: ");
        Serial.println(buf);
    }
}

void SequencePlayer::sendCommandNoPos(const char* cmd) {
    if (!m_cmdController) return;
    
    char buf[64];
    snprintf(buf, sizeof(buf), "%s #%lu", cmd, (unsigned long)nextCmdId());
    
    if (m_verbose) {
        Serial.print("MOCKPI> TX: ");
        Serial.println(buf);
    }
}

void SequencePlayer::log(const char* msg) {
    if (m_verbose) {
        Serial.print("MOCKPI> ");
        Serial.println(msg);
    }
}

void SequencePlayer::logf(const char* fmt, ...) {
    if (m_verbose) {
        char buf[128];
        va_list args;
        va_start(args, fmt);
        vsnprintf(buf, sizeof(buf), fmt, args);
        va_end(args);
        Serial.print("MOCKPI> ");
        Serial.println(buf);
    }
}

// ============================================================================
// SequenceRecorder Implementation
// ============================================================================

SequenceRecorder::SequenceRecorder()
    : m_cmdController(nullptr)
    , m_touchController(nullptr)
    , m_state(State::IDLE)
    , m_verbose(true)
    , m_sequenceLen(0)
    , m_prevTouchedMask(0)
    , m_currentTouchedMask(0)
    , m_shownSuccessMask(0)
    , m_pendingReleaseCount(0)
    , m_releaseWindowStart(0)
    , m_lastReleaseTime(0)
{
    m_sequence[0] = '\0';
    for (uint8_t i = 0; i < 4; i++) {
        m_pendingReleases[i] = INVALID_POS;
    }
}

void SequenceRecorder::begin(CommandController* cmdController, TouchController* touchController) {
    m_cmdController = cmdController;
    m_touchController = touchController;
    m_state = State::IDLE;
}

void SequenceRecorder::startRecording() {
    m_sequence[0] = '\0';
    m_sequenceLen = 0;
    m_state = State::WAITING_TOUCH;
    m_prevTouchedMask = 0;
    m_currentTouchedMask = 0;
    m_shownSuccessMask = 0;
    m_pendingReleaseCount = 0;
    m_releaseWindowStart = 0;
    m_lastReleaseTime = 0;
    
    for (uint8_t i = 0; i < 4; i++) {
        m_pendingReleases[i] = INVALID_POS;
    }
    
    log("Recording started");
    log("Touch positions - releases are recorded");
    log("Simultaneous releases (within 500ms) = pair");
    log("Release all and wait 2s to finalize");
}

void SequenceRecorder::stopRecording() {
    if (m_state != State::IDLE && m_state != State::COMPLETE) {
        finalize();
    }
}

void SequenceRecorder::tick() {
    if (m_state == State::IDLE || m_state == State::COMPLETE) {
        return;
    }
    
    updateTouchMask();
    
    // Process new touches (show SUCCESS animation for feedback)
    for (uint8_t i = 0; i < NUM_POSITIONS; i++) {
        if (justTouched(i)) {
            handleNewTouch(i);
        }
    }
    
    // Process releases
    for (uint8_t i = 0; i < NUM_POSITIONS; i++) {
        if (justReleased(i)) {
            handleRelease(i);
        }
    }
    
    // Check release window timeout
    if (m_state == State::RELEASE_WINDOW) {
        if (millis() - m_releaseWindowStart >= MOCK_PI_RECORD_SIMUL_WINDOW_MS) {
            commitPendingReleases();
        }
    }
    
    // Check for idle timeout
    if ((m_state == State::TRACKING || m_state == State::WAITING_TOUCH) &&
        m_sequenceLen > 0 && countActiveTouches() == 0 && m_lastReleaseTime > 0) {
        if (millis() - m_lastReleaseTime >= MOCK_PI_RECORD_IDLE_TIMEOUT_MS) {
            finalize();
        }
    }
}

bool SequenceRecorder::isRecording() const {
    return m_state != State::IDLE && m_state != State::COMPLETE;
}

bool SequenceRecorder::isComplete() const {
    return m_state == State::COMPLETE;
}

const char* SequenceRecorder::getRecordedSequence() const {
    return m_sequence;
}

void SequenceRecorder::updateTouchMask() {
    m_prevTouchedMask = m_currentTouchedMask;
    m_currentTouchedMask = 0;
    
    for (uint8_t i = 0; i < NUM_POSITIONS; i++) {
        if (m_touchController && m_touchController->isTouched(i)) {
            m_currentTouchedMask |= (1UL << i);
        }
    }
}

bool SequenceRecorder::isTouched(uint8_t pos) const {
    if (pos >= NUM_POSITIONS) return false;
    return (m_currentTouchedMask & (1UL << pos)) != 0;
}

bool SequenceRecorder::justTouched(uint8_t pos) const {
    if (pos >= NUM_POSITIONS) return false;
    uint32_t mask = 1UL << pos;
    return ((m_currentTouchedMask & mask) != 0) && ((m_prevTouchedMask & mask) == 0);
}

bool SequenceRecorder::justReleased(uint8_t pos) const {
    if (pos >= NUM_POSITIONS) return false;
    uint32_t mask = 1UL << pos;
    return ((m_currentTouchedMask & mask) == 0) && ((m_prevTouchedMask & mask) != 0);
}

uint8_t SequenceRecorder::countActiveTouches() const {
    uint8_t count = 0;
    for (uint8_t i = 0; i < NUM_POSITIONS; i++) {
        if (m_currentTouchedMask & (1UL << i)) {
            count++;
        }
    }
    return count;
}

void SequenceRecorder::handleNewTouch(uint8_t pos) {
    // Show SUCCESS for visual feedback
    if (!(m_shownSuccessMask & (1UL << pos))) {
        logf("Touch %c - showing SUCCESS", posToChar(pos));
        sendCommand("SUCCESS", pos);
        m_shownSuccessMask |= (1UL << pos);
    }
    
    if (m_state == State::WAITING_TOUCH) {
        m_state = State::TRACKING;
    }
}

void SequenceRecorder::handleRelease(uint8_t pos) {
    logf("Released %c", posToChar(pos));
    m_lastReleaseTime = millis();
    
    // Clear the SUCCESS mask so it can be touched again
    m_shownSuccessMask &= ~(1UL << pos);
    
    // Hide the LED
    sendCommand("HIDE", pos);
    
    if (m_state == State::RELEASE_WINDOW) {
        // Add to pending releases
        if (m_pendingReleaseCount < 4) {
            m_pendingReleases[m_pendingReleaseCount++] = pos;
        }
    } else {
        // Start release window
        m_pendingReleases[0] = pos;
        m_pendingReleaseCount = 1;
        m_releaseWindowStart = millis();
        m_state = State::RELEASE_WINDOW;
    }
}

void SequenceRecorder::commitPendingReleases() {
    if (m_pendingReleaseCount == 0) {
        m_state = State::TRACKING;
        return;
    }
    
    if (m_pendingReleaseCount == 1) {
        appendToSequence(m_pendingReleases[0]);
    } else if (m_pendingReleaseCount == 2) {
        appendPairToSequence(m_pendingReleases[0], m_pendingReleases[1]);
    } else {
        // More than 2 - record as separate singles
        for (uint8_t i = 0; i < m_pendingReleaseCount; i++) {
            appendToSequence(m_pendingReleases[i]);
        }
    }
    
    // Reset
    m_pendingReleaseCount = 0;
    for (uint8_t i = 0; i < 4; i++) {
        m_pendingReleases[i] = INVALID_POS;
    }
    
    m_state = State::TRACKING;
}

void SequenceRecorder::appendToSequence(uint8_t pos) {
    if (m_sequenceLen >= MOCK_PI_MAX_SEQUENCE_LEN - 3) return;
    
    if (m_sequenceLen > 0) {
        m_sequence[m_sequenceLen++] = ',';
    }
    m_sequence[m_sequenceLen++] = posToChar(pos);
    m_sequence[m_sequenceLen] = '\0';
    
    logf("Recorded: %s", m_sequence);
}

void SequenceRecorder::appendPairToSequence(uint8_t pos1, uint8_t pos2) {
    if (m_sequenceLen >= MOCK_PI_MAX_SEQUENCE_LEN - 5) return;
    
    if (m_sequenceLen > 0) {
        m_sequence[m_sequenceLen++] = ',';
    }
    m_sequence[m_sequenceLen++] = posToChar(pos1);
    m_sequence[m_sequenceLen++] = '+';
    m_sequence[m_sequenceLen++] = posToChar(pos2);
    m_sequence[m_sequenceLen] = '\0';
    
    logf("Recorded: %s", m_sequence);
}

void SequenceRecorder::finalize() {
    // Commit any pending releases
    if (m_pendingReleaseCount > 0) {
        commitPendingReleases();
    }
    
    m_state = State::COMPLETE;
    
    log("Recording complete!");
    logf("Final sequence: %s", m_sequence);
}

void SequenceRecorder::sendCommand(const char* cmd, uint8_t pos) {
    if (m_verbose) {
        Serial.print("MOCKPI> TX: ");
        Serial.print(cmd);
        Serial.print(" ");
        Serial.println(posToChar(pos));
    }
}

void SequenceRecorder::log(const char* msg) {
    if (m_verbose) {
        Serial.print("MOCKPI> ");
        Serial.println(msg);
    }
}

void SequenceRecorder::logf(const char* fmt, ...) {
    if (m_verbose) {
        char buf[128];
        va_list args;
        va_start(args, fmt);
        vsnprintf(buf, sizeof(buf), fmt, args);
        va_end(args);
        Serial.print("MOCKPI> ");
        Serial.println(buf);
    }
}

// ============================================================================
// MockPi Main Class Implementation
// ============================================================================

MockPi::MockPi()
    : m_playMode(true)
    , m_recordingDone(false)
{
}

void MockPi::begin(CommandController* cmdController, TouchController* touchController) {
    m_player.begin(cmdController, touchController);
    m_recorder.begin(cmdController, touchController);
}

void MockPi::tick() {
    if (m_playMode) {
        m_player.tick();
    } else {
        // Record mode
        if (!m_recordingDone) {
            m_recorder.tick();
            
            if (m_recorder.isComplete()) {
                m_recordingDone = true;
                
                const char* seq = m_recorder.getRecordedSequence();
                if (strlen(seq) > 0) {
                    Serial.println("MOCKPI> ======================================");
                    Serial.println("MOCKPI> Recording complete! Starting playback...");
                    Serial.print("MOCKPI> Sequence: ");
                    Serial.println(seq);
                    Serial.println("MOCKPI> ======================================");
                    
                    delay(1000);
                    m_player.startSequence(seq);
                    m_playMode = true;
                }
            }
        } else {
            m_player.tick();
        }
    }
}

void MockPi::startPlayMode(const char* sequence) {
    m_playMode = true;
    m_recordingDone = true;
    m_player.startSequence(sequence);
}

void MockPi::startRecordMode() {
    m_playMode = false;
    m_recordingDone = false;
    m_recorder.startRecording();
}

bool MockPi::isFinished() const {
    if (m_playMode) {
        return m_player.isFinished();
    }
    return m_recordingDone && m_player.isFinished();
}

void MockPi::setVerbose(bool v) {
    m_player.setVerbose(v);
    m_recorder.setVerbose(v);
}

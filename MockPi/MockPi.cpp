/**
 * @file MockPi.cpp
 * @brief Implementation of MockPi testing utility
 * 
 * MockPi simulates the Raspberry Pi. It sends commands via a callback
 * and receives events via onEvent(). It knows nothing about Arduino hardware.
 */

#include "MockPi.h"
#include <stdarg.h>

// ============================================================================
// SequencePlayer Implementation
// ============================================================================

SequencePlayer::SequencePlayer()
    : m_sendCmd(nullptr)
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
{
}

void SequencePlayer::begin(CommandCallback sendCmd) {
    m_sendCmd = sendCmd;
    m_state = State::IDLE;
}

bool SequencePlayer::startSequence(const char* sequence) {
    if (!m_sendCmd) {
        log("ERROR: No command callback set");
        return false;
    }
    
    if (!parseSequence(sequence)) {
        log("ERROR: Failed to parse sequence");
        return false;
    }
    
    m_currentToken = 0;
    m_state = State::TOKEN_SHOW;
    m_stateTime = millis();
    
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

void SequencePlayer::onEvent(const char* eventLine) {
    // Debug: print all received events
    Serial.print("MOCKPI> RX EVENT: '");
    Serial.print(eventLine);
    Serial.print("' state=");
    Serial.println((int)m_state);
    
    // Parse events like "TOUCHED A" or "TOUCH_RELEASED B"
    if (m_state == State::TOKEN_WAIT_TOUCH) {
        if (strncmp(eventLine, "TOUCHED ", 8) == 0) {
            char pos = eventLine[8];
            uint8_t posIdx = charToPos(pos);
            const Token& token = m_tokens[m_currentToken];
            
            if (posIdx == token.pos1 && !m_got1) {
                m_got1 = true;
                if (m_firstTouchTime == 0) {
                    m_firstTouchTime = millis();
                }
                logf("Received TOUCHED %c", pos);
            }
            
            if (token.isPair() && posIdx == token.pos2 && !m_got2) {
                m_got2 = true;
                if (m_firstTouchTime == 0) {
                    m_firstTouchTime = millis();
                }
                logf("Received TOUCHED %c", pos);
            }
        }
    }
    else if (m_state == State::TOKEN_WAIT_RELEASE) {
        if (strncmp(eventLine, "TOUCH_RELEASED ", 15) == 0) {
            char pos = eventLine[15];
            uint8_t posIdx = charToPos(pos);
            const Token& token = m_tokens[m_currentToken];
            
            if (posIdx == token.pos1 && !m_releaseGot1) {
                m_releaseGot1 = true;
                if (m_releaseFirstTime == 0) {
                    m_releaseFirstTime = millis();
                }
                logf("Received TOUCH_RELEASED %c", pos);
            }
            
            if (token.isPair() && posIdx == token.pos2 && !m_releaseGot2) {
                m_releaseGot2 = true;
                if (m_releaseFirstTime == 0) {
                    m_releaseFirstTime = millis();
                }
                logf("Received TOUCH_RELEASED %c", pos);
            }
        }
    }
}

void SequencePlayer::tick() {
    if (m_state == State::IDLE || m_state == State::FINISHED) {
        return;
    }
    
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
            // Check if all touches received (via onEvent)
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
            // Check if all releases received (via onEvent)
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

void SequencePlayer::sendCommand(const char* cmd, uint8_t pos) {
    if (!m_sendCmd) return;
    
    char buf[64];
    snprintf(buf, sizeof(buf), "%s %c #%lu", cmd, posToChar(pos), (unsigned long)nextCmdId());
    
    if (m_verbose) {
        Serial.print("MOCKPI> TX: ");
        Serial.println(buf);
    }
    
    m_sendCmd(buf);
}

void SequencePlayer::sendCommandNoPos(const char* cmd) {
    if (!m_sendCmd) return;
    
    char buf[64];
    snprintf(buf, sizeof(buf), "%s #%lu", cmd, (unsigned long)nextCmdId());
    
    if (m_verbose) {
        Serial.print("MOCKPI> TX: ");
        Serial.println(buf);
    }
    
    m_sendCmd(buf);
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

/**
 * @file MockPi.h
 * @brief MockPi - Testing utility that simulates Raspberry Pi
 * 
 * MockPi simulates the Raspberry Pi by:
 *   1. Sending commands (SHOW, EXPECT, etc.) - via a callback
 *   2. Receiving events (TOUCHED, etc.) - via onEvent() calls
 * 
 * MockPi knows NOTHING about Arduino hardware. It only knows the protocol.
 * The main.cpp wires it up by:
 *   - Calling mockPi.onEvent() when events are emitted
 *   - Passing sent commands to CommandController
 */

#ifndef MOCK_PI_H
#define MOCK_PI_H

#include <Arduino.h>
#include "MockPiConfig.h"

// ============================================================================
// Constants
// ============================================================================

constexpr uint8_t MOCK_PI_NUM_POSITIONS = 25;
constexpr uint8_t INVALID_POS = 255;

// ============================================================================
// Command Callback Type
// ============================================================================

// Callback to send a command string (e.g., "SHOW A #101")
typedef void (*CommandCallback)(const char* command);

// ============================================================================
// Token Structure
// ============================================================================

struct Token {
    uint8_t pos1;
    uint8_t pos2;  // INVALID_POS if single
    
    Token() : pos1(INVALID_POS), pos2(INVALID_POS) {}
    Token(uint8_t p) : pos1(p), pos2(INVALID_POS) {}
    Token(uint8_t p1, uint8_t p2) : pos1(p1), pos2(p2) {}
    
    bool isSingle() const { return pos2 == INVALID_POS; }
    bool isPair() const { return pos2 != INVALID_POS; }
    bool isValid() const { return pos1 != INVALID_POS; }
};

// ============================================================================
// Utility Functions
// ============================================================================

inline char posToChar(uint8_t pos) {
    return (pos < MOCK_PI_NUM_POSITIONS) ? ('A' + pos) : '?';
}

inline uint8_t charToPos(char c) {
    if (c >= 'A' && c <= 'Y') return c - 'A';
    if (c >= 'a' && c <= 'y') return c - 'a';
    return INVALID_POS;
}

// ============================================================================
// SequencePlayer Class
// ============================================================================

class SequencePlayer {
public:
    SequencePlayer();
    
    void begin(CommandCallback sendCmd);
    bool startSequence(const char* sequence);
    void tick();
    
    // Called when Arduino emits an event (TOUCHED, TOUCH_RELEASED, etc.)
    void onEvent(const char* eventLine);
    
    bool isRunning() const;
    bool isFinished() const;
    void setVerbose(bool v) { m_verbose = v; }

private:
    enum class State {
        IDLE,
        TOKEN_SHOW,
        TOKEN_EXPECT,
        TOKEN_WAIT_TOUCH,
        TOKEN_SUCCESS,
        TOKEN_WAIT_ANIM,
        TOKEN_BLINK,
        TOKEN_EXPECT_RELEASE,
        TOKEN_WAIT_RELEASE,
        TOKEN_STOP_BLINK,
        TOKEN_HIDE,
        TOKEN_ADVANCE,
        COMPLETING,
        FINISHED
    };
    
    CommandCallback m_sendCmd;
    
    Token m_tokens[MOCK_PI_MAX_TOKENS];
    uint8_t m_tokenCount;
    uint8_t m_currentToken;
    
    State m_state;
    uint32_t m_stateTime;
    uint32_t m_cmdId;
    bool m_verbose;
    
    // Touch detection for current token
    bool m_got1;
    bool m_got2;
    uint32_t m_firstTouchTime;
    
    // Release detection
    bool m_releaseGot1;
    bool m_releaseGot2;
    uint32_t m_releaseFirstTime;
    
    bool parseSequence(const char* sequence);
    
    void sendCommand(const char* cmd, uint8_t pos);
    void sendCommandNoPos(const char* cmd);
    
    void log(const char* msg);
    void logf(const char* fmt, ...);
    
    uint32_t nextCmdId() { return ++m_cmdId; }
};

#endif // MOCK_PI_H

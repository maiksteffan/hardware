/**
 * @file MockPi.h
 * @brief MockPi - Testing utility for Arduino command protocol
 * 
 * This module simulates a Raspberry Pi sending commands to the Arduino.
 * It is used purely for testing that the Arduino handles commands correctly.
 * 
 * Two programs:
 *   1. SequencePlayer - Plays a predefined sequence
 *   2. SequenceRecorder - Records touches, then plays them back
 * 
 * The MockPi directly polls TouchController for touch state and
 * injects commands via CommandController. It does NOT use serial
 * communication - it's for on-device testing only.
 */

#ifndef MOCK_PI_H
#define MOCK_PI_H

#include <Arduino.h>
#include "MockPiConfig.h"

// Forward declarations
class CommandController;
class TouchController;

// ============================================================================
// Constants
// ============================================================================

constexpr uint8_t NUM_POSITIONS = 25;
constexpr uint8_t INVALID_POS = 255;

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
    return (pos < NUM_POSITIONS) ? ('A' + pos) : '?';
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
    
    void begin(CommandController* cmdController, TouchController* touchController);
    bool startSequence(const char* sequence);
    void tick();
    
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
    
    CommandController* m_cmdController;
    TouchController* m_touchController;
    
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
    
    // Touch tracking
    uint32_t m_prevTouchedMask;
    
    bool parseSequence(const char* sequence);
    void updateTouchMask();
    bool isTouched(uint8_t pos) const;
    bool justTouched(uint8_t pos) const;
    bool justReleased(uint8_t pos) const;
    
    void sendCommand(const char* cmd, uint8_t pos);
    void sendCommandNoPos(const char* cmd);
    
    void log(const char* msg);
    void logf(const char* fmt, ...);
    
    uint32_t nextCmdId() { return ++m_cmdId; }
};

// ============================================================================
// SequenceRecorder Class
// ============================================================================

class SequenceRecorder {
public:
    SequenceRecorder();
    
    void begin(CommandController* cmdController, TouchController* touchController);
    void startRecording();
    void stopRecording();
    void tick();
    
    bool isRecording() const;
    bool isComplete() const;
    const char* getRecordedSequence() const;
    void setVerbose(bool v) { m_verbose = v; }

private:
    enum class State {
        IDLE,
        WAITING_TOUCH,
        TRACKING,
        RELEASE_WINDOW,
        COMPLETE
    };
    
    CommandController* m_cmdController;
    TouchController* m_touchController;
    
    State m_state;
    bool m_verbose;
    
    char m_sequence[MOCK_PI_MAX_SEQUENCE_LEN];
    uint8_t m_sequenceLen;
    
    uint32_t m_prevTouchedMask;
    uint32_t m_currentTouchedMask;
    uint32_t m_shownSuccessMask;
    
    uint8_t m_pendingReleases[4];
    uint8_t m_pendingReleaseCount;
    uint32_t m_releaseWindowStart;
    uint32_t m_lastReleaseTime;
    
    void updateTouchMask();
    bool isTouched(uint8_t pos) const;
    bool justTouched(uint8_t pos) const;
    bool justReleased(uint8_t pos) const;
    uint8_t countActiveTouches() const;
    
    void handleNewTouch(uint8_t pos);
    void handleRelease(uint8_t pos);
    void commitPendingReleases();
    void appendToSequence(uint8_t pos);
    void appendPairToSequence(uint8_t pos1, uint8_t pos2);
    void finalize();
    
    void sendCommand(const char* cmd, uint8_t pos);
    void log(const char* msg);
    void logf(const char* fmt, ...);
};

// ============================================================================
// MockPi Main Class
// ============================================================================

class MockPi {
public:
    MockPi();
    
    void begin(CommandController* cmdController, TouchController* touchController);
    void tick();
    
    // For PLAY mode
    void startPlayMode(const char* sequence);
    
    // For RECORD mode
    void startRecordMode();
    
    bool isFinished() const;
    void setVerbose(bool v);

private:
    SequencePlayer m_player;
    SequenceRecorder m_recorder;
    
    bool m_playMode;
    bool m_recordingDone;
};

#endif // MOCK_PI_H

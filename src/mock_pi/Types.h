/**
 * @file Types.h
 * @brief Common types and constants for MockPI subsystem
 * 
 * This file defines positions, tokens, and other shared types.
 */

#ifndef MOCK_PI_TYPES_H
#define MOCK_PI_TYPES_H

#include <Arduino.h>

namespace MockPI {

// ============================================================================
// Configuration Constants
// ============================================================================

constexpr uint32_t SERIAL_BAUD = 115200;
constexpr uint16_t SIMUL_TOLERANCE_MS = 500;      // Window for simultaneous touch/release
constexpr uint32_t SINGLE_TIMEOUT_MS = 15000;     // Timeout for single waits
constexpr uint8_t  MAX_RETRY_COUNT = 3;           // Max retries on ERR busy
constexpr uint16_t RETRY_DELAYS_MS[] = {50, 100, 200}; // Backoff delays

constexpr uint8_t MAX_TOKENS = 32;                // Max tokens in a sequence
constexpr uint8_t MAX_LINE_LENGTH = 128;          // Max serial line length
constexpr uint8_t MAX_PENDING_COMMANDS = 16;      // Max pending command correlation entries

// ============================================================================
// Position Type (A-Y = 0-24)
// ============================================================================

constexpr uint8_t NUM_POSITIONS = 25;
constexpr uint8_t INVALID_POS = 255;

/**
 * Convert position index (0-24) to character ('A'-'Y')
 */
inline char posToChar(uint8_t pos) {
    if (pos >= NUM_POSITIONS) return '?';
    return 'A' + pos;
}

/**
 * Convert character ('A'-'Y') to position index (0-24)
 * Returns INVALID_POS if invalid
 */
inline uint8_t charToPos(char c) {
    if (c >= 'A' && c <= 'Y') return c - 'A';
    if (c >= 'a' && c <= 'y') return c - 'a';
    return INVALID_POS;
}

/**
 * Check if character is a valid position
 */
inline bool isValidPosChar(char c) {
    return (c >= 'A' && c <= 'Y') || (c >= 'a' && c <= 'y');
}

// ============================================================================
// Token Type (single or pair)
// ============================================================================

struct Token {
    uint8_t pos1;       // First position (always valid)
    uint8_t pos2;       // Second position (INVALID_POS if single)
    
    Token() : pos1(INVALID_POS), pos2(INVALID_POS) {}
    Token(uint8_t p) : pos1(p), pos2(INVALID_POS) {}
    Token(uint8_t p1, uint8_t p2) : pos1(p1), pos2(p2) {}
    
    bool isSingle() const { return pos2 == INVALID_POS; }
    bool isPair() const { return pos2 != INVALID_POS; }
    bool isValid() const { return pos1 != INVALID_POS; }
    
    uint8_t count() const { return isPair() ? 2 : (isValid() ? 1 : 0); }
};

// ============================================================================
// Event Types (parsed from Arduino responses)
// ============================================================================

enum class EventType : uint8_t {
    NONE,
    ACK,
    DONE,
    ERR,
    TOUCHED,
    TOUCH_RELEASED,
    TOUCH_DOWN,
    TOUCH_UP,
    INFO,
    UNKNOWN
};

/**
 * Convert event type to string for logging
 */
inline const char* eventTypeToStr(EventType t) {
    switch (t) {
        case EventType::ACK:            return "ACK";
        case EventType::DONE:           return "DONE";
        case EventType::ERR:            return "ERR";
        case EventType::TOUCHED:        return "TOUCHED";
        case EventType::TOUCH_RELEASED: return "TOUCH_RELEASED";
        case EventType::TOUCH_DOWN:     return "TOUCH_DOWN";
        case EventType::TOUCH_UP:       return "TOUCH_UP";
        case EventType::INFO:           return "INFO";
        default:                        return "UNKNOWN";
    }
}

// ============================================================================
// Parsed Event Structure
// ============================================================================

struct ParsedEvent {
    EventType type;
    char action[16];        // Action name for ACK/DONE (e.g., "SHOW", "SUCCESS")
    uint8_t position;       // Position (INVALID_POS if none)
    uint32_t commandId;     // Command ID (0 if none)
    char reason[16];        // Error reason for ERR events
    uint32_t timestamp;     // millis() when received
    
    ParsedEvent() : type(EventType::NONE), position(INVALID_POS), commandId(0), timestamp(0) {
        action[0] = '\0';
        reason[0] = '\0';
    }
    
    void clear() {
        type = EventType::NONE;
        position = INVALID_POS;
        commandId = 0;
        timestamp = 0;
        action[0] = '\0';
        reason[0] = '\0';
    }
};

// ============================================================================
// Error Reason Types
// ============================================================================

enum class ErrorReason : uint8_t {
    NONE,
    BAD_FORMAT,
    UNKNOWN_ACTION,
    UNKNOWN_POSITION,
    COMMAND_FAILED,
    BUSY,
    UNKNOWN
};

inline ErrorReason parseErrorReason(const char* str) {
    if (!str) return ErrorReason::UNKNOWN;
    if (strcmp(str, "bad_format") == 0) return ErrorReason::BAD_FORMAT;
    if (strcmp(str, "unknown_action") == 0) return ErrorReason::UNKNOWN_ACTION;
    if (strcmp(str, "unknown_position") == 0) return ErrorReason::UNKNOWN_POSITION;
    if (strcmp(str, "command_failed") == 0) return ErrorReason::COMMAND_FAILED;
    if (strcmp(str, "busy") == 0) return ErrorReason::BUSY;
    return ErrorReason::UNKNOWN;
}

// ============================================================================
// Runner State
// ============================================================================

enum class RunnerState : uint8_t {
    IDLE,
    INIT,           // Initial state, will ping/info
    RUNNING,        // Processing sequence
    COMPLETING,     // Sent SEQUENCE_COMPLETED, waiting for DONE
    HALTED,         // Error state, stopped
    FINISHED        // Sequence complete
};

inline const char* runnerStateToStr(RunnerState s) {
    switch (s) {
        case RunnerState::IDLE:       return "IDLE";
        case RunnerState::INIT:       return "INIT";
        case RunnerState::RUNNING:    return "RUNNING";
        case RunnerState::COMPLETING: return "COMPLETING";
        case RunnerState::HALTED:     return "HALTED";
        case RunnerState::FINISHED:   return "FINISHED";
        default:                      return "?";
    }
}

// ============================================================================
// Wait Group State
// ============================================================================

enum class WaitState : uint8_t {
    INACTIVE,
    WAITING_FIRST,      // Waiting for first event
    WAITING_SECOND,     // Pair: got first, waiting for second within tolerance
    SUCCESS,
    FAILURE,
    TIMEOUT
};

} // namespace MockPI

#endif // MOCK_PI_TYPES_H

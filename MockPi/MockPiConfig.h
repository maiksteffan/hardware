/**
 * @file MockPiConfig.h
 * @brief Configuration for MockPi testing module
 * 
 * MockPi is a testing utility that simulates Raspberry Pi commands
 * to test how the Arduino handles the protocol.
 * 
 * Two modes are available:
 *   1. PLAY - Play a predefined sequence like "A,B,C,D+E,F"
 *   2. RECORD - Record touches and then play them back
 */

#ifndef MOCK_PI_CONFIG_H
#define MOCK_PI_CONFIG_H

// ============================================================================
// Mode Selection
// ============================================================================

#define MOCK_PI_MODE_PLAY    1
#define MOCK_PI_MODE_RECORD  2

// *** SELECT MODE HERE ***
#define MOCK_PI_MODE MOCK_PI_MODE_PLAY

// ============================================================================
// Play Mode Configuration
// ============================================================================

// The sequence to play in PLAY mode
// Format: comma-separated positions, + for simultaneous touches
// Examples:
//   "A,B,C,D,E"         - Simple sequential
//   "A,B,C+D,E+F,G"     - With simultaneous pairs
#define MOCK_PI_SEQUENCE "A,B,C,D+E,F,G+H,I,J,K"

// ============================================================================
// Timing Configuration
// ============================================================================

// Time window for simultaneous touches (ms)
#define MOCK_PI_SIMUL_WINDOW_MS 500

// Timeout waiting for a touch (ms)
#define MOCK_PI_TOUCH_TIMEOUT_MS 30000

// Delay between commands (ms)
#define MOCK_PI_INTER_CMD_DELAY_MS 50

// Time to wait for SUCCESS animation (ms)
#define MOCK_PI_SUCCESS_ANIM_MS 500

// ============================================================================
// Record Mode Configuration
// ============================================================================

// Idle timeout to finalize recording (ms)
#define MOCK_PI_RECORD_IDLE_TIMEOUT_MS 2000

// Window for simultaneous releases (ms)
#define MOCK_PI_RECORD_SIMUL_WINDOW_MS 500

// ============================================================================
// General Configuration
// ============================================================================

// Enable verbose logging
#define MOCK_PI_VERBOSE true

// Maximum sequence length
#define MOCK_PI_MAX_SEQUENCE_LEN 128

// Maximum tokens in a sequence
#define MOCK_PI_MAX_TOKENS 32

#endif // MOCK_PI_CONFIG_H

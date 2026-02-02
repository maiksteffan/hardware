/**
 * @file MockPi.h
 * @brief Mock Pi sequence runner using direct hardware access
 * 
 * This implementation directly polls TouchController::isTouched() 
 * and injects commands via CommandController::injectCommand().
 * It does NOT use Serial for events (can't read own output).
 * 
 * Release queue logic (from pseudocode):
 * - Tokens 0,1: SHOW, wait touch, SUCCESS only
 * - From token 2: Start releasing token (idx-2) while processing current token
 *   - Single token: release 1 position from queue
 *   - Pair token: release 2 positions from queue as pair
 * 
 * Usage:
 *   MockPI::MockPi mockPi;
 *   mockPi.begin(&Serial, &commandController, &touchController);
 *   mockPi.startSequence("A,B,C,D,E+F,G+H,I,J,K");
 *   // In loop:
 *   mockPi.tick();
 */

#ifndef MOCK_PI_MOCK_PI_H
#define MOCK_PI_MOCK_PI_H

#include <Arduino.h>
#include "Types.h"
#include "SequenceParser.h"

// Forward declarations
class CommandController;
class TouchController;

namespace MockPI {

// ============================================================================
// Configuration
// ============================================================================

constexpr uint32_t SIMUL_WINDOW_MS = 500;       // Simultaneity tolerance
constexpr uint32_t TOUCH_TIMEOUT_MS = 30000;    // Timeout waiting for touch
constexpr uint32_t INTER_CMD_DELAY_MS = 50;     // Delay between commands

// SUCCESS_ANIM_MS can be overridden via MOCK_PI_SUCCESS_DELAY_MS in main.cpp
#ifndef MOCK_PI_SUCCESS_DELAY_MS
#define MOCK_PI_SUCCESS_DELAY_MS 500
#endif
constexpr uint32_t SUCCESS_ANIM_MS = MOCK_PI_SUCCESS_DELAY_MS;

constexpr uint8_t  RELEASE_QUEUE_SIZE = 8;      // Max pending releases

// ============================================================================
// State Machine States
// ============================================================================

enum class MockPiState : uint8_t {
    IDLE,
    
    // Per-token states
    TOKEN_SHOW,           // Send SHOW commands
    TOKEN_EXPECT,         // Send EXPECT commands  
    TOKEN_WAIT_TOUCH,     // Poll for touch
    TOKEN_SUCCESS,        // Send SUCCESS commands
    TOKEN_QUEUE_PUSH,     // Push to release queue
    TOKEN_ADVANCE,        // Move to next token
    
    // Release states (concurrent)
    RELEA
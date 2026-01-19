/**
 * @file MockPiPrograms.h
 * @brief Mock Raspberry Pi programs for on-device testing
 * 
 * Simulates the Pi "logic side" to test the Arduino protocol without 
 * a real Raspberry Pi. Runs entirely on the microcontroller.
 * 
 * Programs:
 *   1. playSequenceSimple     - Sequential show/expect/success for each position
 *   2. playSequenceSimultaneous - Supports multi-touch steps with timing tolerance
 *   3. recordThenPlayback     - Record touches, then play them back
 * 
 * Non-blocking state machine design - no delay() or blocking waits.
 * 
 * Integration:
 *   - Sends commands via CommandController::injectCommand()
 *   - Monitors touch state by directly polling TouchController::isTouched()
 *   - Can also parse event lines fed via feedEventLine()
 */

#ifndef MOCK_PI_PROGRAMS_H
#define MOCK_PI_PROGRAMS_H

#include <Arduino.h>
#include "Config.h"

// Forward declarations
class TouchController;
class CommandController;

// ============================================================================
// Configuration
// ============================================================================

// Maximum positions in a sequence
constexpr uint8_t MAX_SEQUENCE_LENGTH = 25;

// Maximum positions in a simultaneous group
constexpr uint8_t MAX_GROUP_SIZE = 5;

// Timing constants (ms)
constexpr uint32_t MOCK_PI_STEP_TIMEOUT_MS = 10000;       // 10s timeout per step
constexpr uint32_t MOCK_PI_SIMULTANEOUS_WINDOW_MS = 500;  // 500ms window for multi-touch
constexpr uint32_t MOCK_PI_IDLE_THRESHOLD_MS = 1000;      // 1s idle to end recording
constexpr uint32_t MOCK_PI_INTER_STEP_DELAY_MS = 100;     // Delay between steps
constexpr uint32_t MOCK_PI_ACK_TIMEOUT_MS = 500;          // Wait for ACK before proceeding

// ============================================================================
// Program Selection
// ============================================================================

enum class MockPiProgram : uint8_t {
    NONE = 0,
    SEQUENCE_SIMPLE,          // Program 1: Simple sequential
    SEQUENCE_SIMULTANEOUS,    // Program 2: With simultaneous steps
    RECORD_PLAYBACK,          // Program 3: Record then playback
    TWO_HAND_SEQUENCE         // Program 4: Two-hand overlapping sequence
};

// ============================================================================
// Step Types
// ============================================================================

enum class StepType : uint8_t {
    SINGLE,       // Single position touch
    SIMULTANEOUS  // Multiple positions must be touched together
};

// ============================================================================
// Step Structure
// ============================================================================

struct SequenceStep {
    StepType type;
    uint8_t positionCount;                    // Number of positions in this step
    char positions[MAX_GROUP_SIZE];           // Position letters (A-Y)
    uint32_t touchedMask;                     // Bitmask of touched positions (for simultaneous)
    uint32_t firstTouchTime;                  // When first position was touched
};

// ============================================================================
// Internal State Machine States
// ============================================================================

enum class MockPiState : uint8_t {
    IDLE,                     // Not running any program
    
    // Sequence states
    STEP_SHOW,                // Sending SHOW commands for current step
    STEP_EXPECT_DOWN,         // Sending EXPECT_DOWN commands
    STEP_WAIT_TOUCH,          // Waiting for touch(es)
    STEP_SUCCESS,             // Sending SUCCESS commands
    STEP_EXPECT_UP,           // Sending EXPECT_UP commands
    STEP_WAIT_RELEASE,        // Waiting for release(es)
    STEP_HIDE,                // Sending HIDE commands
    STEP_NEXT,                // Moving to next step
    SEQUENCE_COMPLETE,        // Sending SEQUENCE_COMPLETED
    
    // Two-hand sequence states (with blink)
    // Pattern: SHOW A -> wait touch A -> SUCCESS A -> SHOW B -> wait touch B -> 
    //          SUCCESS B -> BLINK A -> wait release A -> STOP_BLINK A -> HIDE A -> SHOW C -> ...
    TWO_HAND_SHOW,            // Show current position
    TWO_HAND_EXPECT_DOWN,     // Expect down on current position
    TWO_HAND_WAIT_TOUCH,      // Wait for touch on current position
    TWO_HAND_SUCCESS,         // Success on current position
    TWO_HAND_BLINK_OLD,       // Start blinking position N-1 after touching N+1
    TWO_HAND_EXPECT_UP_OLD,   // Expect up on the blinking position
    TWO_HAND_WAIT_RELEASE,    // Wait for release of blinking position
    TWO_HAND_STOP_BLINK_HIDE, // Stop blink and hide the released position, show next
    TWO_HAND_NEXT,            // Move to next position
    TWO_HAND_FINAL_CLEANUP,   // Release remaining held positions at end
    
    // Recording states
    RECORDING,                // Recording touches
    RECORDING_IDLE_CHECK,     // Checking if recording should end
    PLAYBACK                  // Playing back recorded sequence
};

// ============================================================================
// MockPiPrograms Class
// ============================================================================

class MockPiPrograms {
public:
    MockPiPrograms();
    
    /**
     * @brief Initialize the mock Pi
     */
    void begin();
    
    /**
     * @brief Set the touch controller reference for direct state polling
     * @param tc Pointer to TouchController
     */
    void setTouchController(TouchController* tc) { m_touchController = tc; }
    
    /**
     * @brief Set the command controller reference for command injection
     * @param cc Pointer to CommandController
     */
    void setCommandController(CommandController* cc) { m_commandController = cc; }
    
    /**
     * @brief Main update loop - call frequently from loop()
     */
    void update();
    
    /**
     * @brief Feed an event line to the mock Pi for parsing
     * Call this when the Arduino outputs a line (from EventQueue)
     * @param line The event line (e.g., "TOUCHED_DOWN A", "ACK SHOW A")
     */
    void feedEventLine(const char* line);
    
    /**
     * @brief Start Program 1: Simple sequential sequence
     * @param positions String of position letters (e.g., "ABCDE")
     */
    void startSequenceSimple(const char* positions);
    
    /**
     * @brief Start Program 2: Sequence with simultaneous steps
     * Format: "A,B,(C+D),(E+F)" - parentheses indicate simultaneous
     * @param spec Sequence specification string
     */
    void startSequenceSimultaneous(const char* spec);
    
    /**
     * @brief Start Program 3: Record then playback mode
     */
    void startRecordPlayback();
    
    /**
     * @brief Start Program 4: Two-hand overlapping sequence
     * Pattern for A,B,C,D:
     *   SHOW A -> touch A -> SUCCESS A
     *   SHOW B -> touch B -> SUCCESS B -> BLINK A
     *   wait release A -> STOP_BLINK A -> HIDE A -> SHOW C
     *   touch C -> SUCCESS C -> BLINK B
     *   wait release B -> STOP_BLINK B -> HIDE B -> SHOW D
     *   touch D -> SEQUENCE_COMPLETED
     * @param positions String of position letters (e.g., "ABCD")
     */
    void startTwoHandSequence(const char* positions);
    
    /**
     * @brief Stop current program
     */
    void stop();
    
    /**
     * @brief Check if a program is currently running
     */
    bool isRunning() const;
    
    /**
     * @brief Get current program
     */
    MockPiProgram currentProgram() const { return m_program; }
    
    /**
     * @brief Enable/disable verbose logging
     */
    void setVerbose(bool verbose) { m_verbose = verbose; }

private:
    // External references
    TouchController* m_touchController;
    CommandController* m_commandController;
    
    // Current program and state
    MockPiProgram m_program;
    MockPiState m_state;
    bool m_verbose;
    
    // Sequence data
    SequenceStep m_steps[MAX_SEQUENCE_LENGTH];
    uint8_t m_stepCount;
    uint8_t m_currentStep;
    
    // Timing
    uint32_t m_stateStartTime;
    uint32_t m_lastActionTime;
    uint32_t m_commandId;
    
    // Touch tracking (bitmask for 25 positions)
    uint32_t m_previousTouched;      // Previous poll's touched state
    uint32_t m_currentlyTouched;     // Currently held positions
    uint32_t m_stepTouchedMask;      // Positions touched in current step
    uint32_t m_firstTouchTime;       // When first touch occurred in current step
    
    // Recording buffer
    char m_recordedSequence[MAX_SEQUENCE_LENGTH + 1];
    uint8_t m_recordedCount;
    uint32_t m_lastTouchTime;        // For idle detection
    
    // Two-hand sequence tracking
    char m_twoHandPositions[MAX_SEQUENCE_LENGTH + 1];  // Position sequence
    uint8_t m_twoHandCount;                            // Total positions
    uint8_t m_twoHandCurrent;                          // Current position index
    uint8_t m_twoHandCleanupIndex;                     // For final cleanup
    
    // ACK tracking
    bool m_waitingForAck;
    uint8_t m_pendingCommands;
    
    // === Internal Methods ===
    
    /**
     * @brief Poll touch controller 
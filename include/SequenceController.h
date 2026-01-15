/**
 * @file SequenceController.h
 * @brief Controls a sequence of LED/touch interactions
 * 
 * Handles sequences like "A,B,C,D" with the pattern:
 *   SHOW current -> EXPECT current -> SUCCESS current -> HIDE (current-2)
 */

#ifndef SEQUENCE_CONTROLLER_H
#define SEQUENCE_CONTROLLER_H

#include <Arduino.h>

// Forward declarations
class LedController;
class TouchController;

// ============================================================================
// Configuration
// ============================================================================

// Maximum sequence length (number of positions)
constexpr uint8_t MAX_SEQUENCE_LENGTH = 25;

// ============================================================================
// Sequence State Machine
// ============================================================================

enum class SequenceState : uint8_t {
    IDLE = 0,           // No sequence running
    SHOWING,            // Just showed LED, waiting briefly
    EXPECTING,          // Waiting for touch
    COMPLETING          // Processing touch, moving to next
};

// ============================================================================
// SequenceController Class
// ============================================================================

class SequenceController {
public:
    /**
     * @brief Construct a new Sequence Controller
     * @param ledController Reference to LED controller
     * @param touchController Reference to touch controller
     */
    SequenceController(LedController& ledController, TouchController& touchController);

    /**
     * @brief Initialize the sequence controller
     */
    void begin();

    /**
     * @brief Update the sequence state machine (call every loop)
     */
    void update();

    /**
     * @brief Start a new sequence
     * @param sequence Comma-separated positions like "A,B,C,D"
     * @return true if sequence was valid and started
     */
    bool startSequence(const char* sequence);

    /**
     * @brief Stop the current sequence
     */
    void stop();

    /**
     * @brief Check if a sequence is currently running
     * @return true if sequence is active
     */
    bool isRunning() const;

    /**
     * @brief Handle touch event from TouchController
     * @param letter The letter that was touched (A-Y)
     */
    void onTouched(char letter);

private:
    LedController& m_ledController;
    TouchController& m_touchController;

    // Sequence storage
    char m_sequence[MAX_SEQUENCE_LENGTH];   // Array of position letters
    uint8_t m_sequenceLength;               // Number of positions in sequence
    uint8_t m_currentIndex;                 // Current position in sequence

    // State machine
    SequenceState m_state;
    uint32_t m_stateStartTime;              // When we entered current state

    /**
     * @brief Parse a comma-separated sequence string
     * @param sequence Input string like "A,B,C,D"
     * @return true if valid
     */
    bool parseSequence(const char* sequence);

    /**
     * @brief Show the LED for current position and set up expect
     */
    void showCurrentAndExpect();

    /**
     * @brief Handle successful touch at current position
     */
    void handleSuccess();

    /**
     * @brief Move to next position in sequence
     */
    void advanceSequence();

    /**
     * @brief Complete the sequence
     */
    void completeSequence();
};

#endif // SEQUENCE_CONTROLLER_H

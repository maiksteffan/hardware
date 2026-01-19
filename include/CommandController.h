/**
 * @file CommandController.h
 * @brief Serial command parser and executor for LED/Touch controller
 * 
 * Protocol v2: Non-blocking command processing with optional command IDs
 * for request-response correlation.
 * 
 * Commands:
 *   SHOW <pos> [#id]           - Turn on LED at position
 *   HIDE <pos> [#id]           - Turn off LED at position
 *   SUCCESS <pos> [#id]        - Play success animation
 *   BLINK <pos> [#id]          - Start blinking LED at position
 *   STOP_BLINK <pos> [#id]     - Stop blinking LED at position
 *   EXPECT_DOWN <pos> [#id]    - Wait for touch, then emit TOUCHED_DOWN
 *   EXPECT_UP <pos> [#id]      - Wait for release, then emit TOUCHED_UP
 *   RECALIBRATE <pos> [#id]    - Recalibrate single touch sensor
 *   RECALIBRATE_ALL [#id]      - Recalibrate all touch sensors
 *   SCAN [#id]                 - Scan I2C, return SCANNED[A,B,C,...]
 *   SEQUENCE_COMPLETED [#id]   - Play celebration animation on all LEDs
 *   INFO [#id]                 - Return firmware info
 *   PING [#id]                 - Respond with ACK
 */

#ifndef COMMAND_CONTROLLER_H
#define COMMAND_CONTROLLER_H

#include <Arduino.h>
#include "Config.h"

// Forward declarations
class LedController;
class TouchController;
class EventQueue;

// ============================================================================
// Command Types
// ============================================================================

enum class CommandAction : uint8_t {
    INVALID = 0,
    SHOW,
    HIDE,
    SUCCESS,
    BLINK,
    STOP_BLINK,
    EXPECT_DOWN,
    EXPECT_UP,
    RECALIBRATE,
    RECALIBRATE_ALL,
    SCAN,
    SEQUENCE_COMPLETED,
    INFO,
    PING
};

// ============================================================================
// Parsed Command Structure
// ============================================================================

struct ParsedCommand {
    CommandAction action;
    bool hasPosition;
    char position;          // 'A'-'Y'
    uint8_t positionIndex;  // 0-24
    bool hasId;
    uint32_t id;
    bool valid;
};

// ============================================================================
// Command Queue Entry (for long-running commands)
// ============================================================================

struct QueuedCommand {
    ParsedCommand command;
    bool active;
    uint32_t startTime;
    uint8_t state;          // Command-specific state machine state
    uint8_t scanAddress;    // For SCAN: current address being scanned
};

// ============================================================================
// CommandController Class
// ============================================================================

class CommandController {
public:
    /**
     * @brief Construct a new Command Controller
     * @param ledController Reference to LED controller
     * @param touchController Pointer to touch controller (can be nullptr)
     * @param eventQueue Reference to event queue for responses
     */
    CommandController(LedController& ledController, 
                      TouchController* touchController,
                      EventQueue& eventQueue);

    /**
     * @brief Initialize the command controller
     */
    void begin();

    /**
     * @brief Poll serial for incoming data (non-blocking)
     * Reads available bytes into ring buffer
     */
    void pollSerial();

    /**
     * @brief Process any complete lines in the buffer
     */
    void processCompletedLines();

    /**
     * @brief Tick the command executor for long-running commands
     */
    void tick();

    /**
     * @brief Check if the command queue is full
     * @return true if no room for more commands
     */
    bool isQueueFull() const;

    /**
     * @brief Inject a command directly without going through serial
     * Used for testing or internal command generation
     * @param line Command string (e.g., "SHOW A #123")
     */
    void injectCommand(const char* line);

private:
    // References
    LedController& m_ledController;
    TouchController* m_touchController;
    EventQueue& m_eventQueue;

    // Ring buffer for incoming serial data
    char m_rxBuffer[MAX_LINE_LEN * 2];
    uint8_t m_rxHead;
    uint8_t m_rxTail;

    // Line buffer for parsing
    char m_lineBuffer[MAX_LINE_LEN];
    uint8_t m_lineIndex;
    bool m_lineOverflow;

    // Command queue for long-running commands
    QueuedCommand m_commandQueue[COMMAND_QUEUE_SIZE];

    // === Serial/Parsing Methods ===

    /**
     * @brief Extract next complete line from ring buffer
     * @return true if a line was extracted
     */
    bool extractLine();

    /**
     * @brief Parse a command line into a ParsedCommand struct
     * @param line Null-terminated command string
     * @param cmd Output parsed command
     * @return true if parsing succeeded
     */
    bool parseLine(const char* line, ParsedCommand& cmd);

    /**
     * @brief Parse action string to enum
     * @param str Action string
     * @param len Length of string
     * @return CommandAction enum value
     */
    static CommandAction parseAction(const char* str, size_t len);

    /**
     * @brief Get action name string
     * @param action Action enum
     * @return Action name
     */
    static const char* actionToString(CommandAction action);

    /**
     * @brief Check if action requires a position argument
     * @param action Action enum
     * @return true if position is required
     */
    static bool actionRequiresPosition(CommandAction action);

    /**
     * @brief Check if action is long-running (needs DONE event)
     * @param action Action enum
     * @return true if long-running
     */
    static bool actionIsLongRunning(CommandAction action);

    // === Execution Methods ===

    /**
     * @brief Execute a parsed command
     * @param cmd Parsed command
     */
    void executeCommand(const ParsedCommand& cmd);

    /**
     * @brief Execute instant command (no queuing)
     * @param cmd Parsed command
     */
    void executeInstant(const ParsedCommand& cmd);

    /**
     * @brief Queue a long-running command
     * @param cmd Parsed command
     * @return true if queued successfully
     */
    bool queueCommand(const ParsedCommand& cmd);

    /**
     * @brief Tick a single queued command
     * @param qc Queued command entry
     */
    void tickCommand(QueuedCommand& qc);

    // === Utility Methods ===

    /**
     * @brief Skip whitespace in string
     * @param str Input string
     * @return Pointer to first non-whitespace
     */
    static const char* skipWhitespace(const char* str);

    /**
     * @brief Find end of current token
     * @param str Input string
     * @return Pointer to first whitespace/null after token
     */
    static const char* findTokenEnd(const char* str);

    /**
     * @brief Case-insensitive string comparison
     * @param a First string
     * @param b Second string
     * @param len Length to compare
     * @return true if match
     */
    static bool strcasecmpN(const char* a, const char* b, size_t len);

    /**
     * @brief Convert position character to index
     * @param c Character 
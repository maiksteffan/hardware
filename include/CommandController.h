/**
 * @file CommandController.h
 * @brief Serial command parser for LED strip control
 * 
 * Handles non-blocking serial input, parses commands, and dispatches
 * to LedController. Supports SHOW, HIDE, and SUCCESS commands.
 * 
 * Protocol:
 *   Input:  "<ACTION> <POSITION>\n"
 *   Output: "ACK <ACTION> <POSITION>\n" on success
 *           "ERR <reason>\n" on failure
 */

#ifndef COMMAND_CONTROLLER_H
#define COMMAND_CONTROLLER_H

#include <Arduino.h>

// Forward declarations to avoid circular dependency
class LedController;
class TouchController;

// ============================================================================
// Configuration
// ============================================================================

// Maximum length of a command line (including null terminator)
constexpr size_t MAX_COMMAND_LENGTH = 32;

// Timeout (ms) to process command if no newline received
constexpr uint32_t COMMAND_TIMEOUT_MS = 50;

// ============================================================================
// Command Types
// ============================================================================

enum class CommandAction : uint8_t {
    INVALID = 0,
    SHOW,
    HIDE,
    SUCCESS,
    EXPECT,      // Wait for specific touch sensor
    RECORD,      // Record first touched sensor
    SCAN,        // Scan I2C addresses
    RECALIBRATE  // Recalibrate touch sensors
};

// ============================================================================
// CommandController Class
// ============================================================================

class CommandController {
public:
    /**
     * @brief Construct a new Command Controller
     * @param ledController Reference to the LED controller to dispatch commands to
     * @param touchController Reference to the touch controller (optional, can be nullptr)
     */
    CommandController(LedController& ledController, TouchController* touchController = nullptr);

    /**
     * @brief Initialize the command controller
     * Call this in setup() after Serial.begin()
     */
    void begin();

    /**
     * @brief Process serial input and handle commands (non-blocking)
     * Call this every loop iteration
     */
    void update();

private:
    // Reference to LED controller
    LedController& m_ledController;
    
    // Pointer to touch controller (optional)
    TouchController* m_touchController;

    // Input buffer for building command lines
    char m_buffer[MAX_COMMAND_LENGTH];
    size_t m_bufferIndex;
    
    // Timestamp of last received character (for timeout processing)
    uint32_t m_lastCharTime;

    /**
     * @brief Process a complete command line
     * @param line Null-terminated command string
     */
    void processLine(const char* line);

    /**
     * @brief Parse action string to enum
     * @param str Action string (case-insensitive)
     * @return CommandAction enum value
     */
    static CommandAction parseAction(const char* str);

    /**
     * @brief Convert action enum to string for ACK output
     * @param action Action enum value
     * @return Action string (uppercase)
     */
    static const char* actionToString(CommandAction action);

    /**
     * @brief Send ACK response
     * @param action Action that was executed
     * @param position Position character (A-Y)
     */
    void sendAck(CommandAction action, char position);

    /**
     * @brief Send ERR response
     * @param reason Short error reason string
     */
    void sendError(const char* reason);

    /**
     * @brief Skip leading whitespace in a string
     * @param str Input string
     * @return Pointer to first non-whitespace character
     */
    static const char* skipWhitespace(const char* str);

    /**
     * @brief Find end of current token (word)
     * @param str Input string starting at token
     * @return Pointer to first whitespace/null after token
     */
    static const char* findTokenEnd(const char* str);

    /**
     * @brief Compare two strings case-insensitively
     * @param a First string
     * @param b Second string
     * @param len Maximum characters to compare
     * @return true if strings match (ignoring case)
     */
    static bool strcasecmp_n(const char* a, const char* b, size_t len);
};

#endif // COMMAND_CONTROLLER_H

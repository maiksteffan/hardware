/**
 * @file CommandController.cpp
 * @brief Implementation of serial command parser for LED strip control
 */

#include "CommandController.h"
#include "LedController.h"

// ============================================================================
// Constructor
// ============================================================================

CommandController::CommandController(LedController& ledController)
    : m_ledController(ledController)
    , m_bufferIndex(0)
{
}

// ============================================================================
// Public Methods
// ============================================================================

void CommandController::begin() {
    // Clear buffer
    m_bufferIndex = 0;
    memset(m_buffer, 0, sizeof(m_buffer));
}

void CommandController::update() {
    // Process all available serial data (non-blocking, handles burst input)
    while (Serial.available() > 0) {
        char c = Serial.read();
        
        // Handle line termination
        if (c == '\n' || c == '\r') {
            if (m_bufferIndex > 0) {
                // Null-terminate and process
                m_buffer[m_bufferIndex] = '\0';
                processLine(m_buffer);
                
                // Reset buffer
                m_bufferIndex = 0;
                memset(m_buffer, 0, sizeof(m_buffer));
            }
            continue;
        }
        
        // Add character to buffer if there's room
        if (m_bufferIndex < MAX_COMMAND_LENGTH - 1) {
            m_buffer[m_bufferIndex++] = c;
        } else {
            // Buffer overflow - discard and report error
            sendError("line_too_long");
            m_bufferIndex = 0;
            memset(m_buffer, 0, sizeof(m_buffer));
            
            // Discard rest of line
            while (Serial.available() > 0) {
                c = Serial.read();
                if (c == '\n' || c == '\r') {
                    break;
                }
            }
        }
    }
}

// ============================================================================
// Private Methods
// ============================================================================

void CommandController::processLine(const char* line) {
    // Skip leading whitespace
    const char* ptr = skipWhitespace(line);
    
    // Empty line?
    if (*ptr == '\0') {
        return;  // Silently ignore empty lines
    }
    
    // Find action token
    const char* actionStart = ptr;
    const char* actionEnd = findTokenEnd(ptr);
    size_t actionLen = actionEnd - actionStart;
    
    if (actionLen == 0) {
        sendError("bad_format");
        return;
    }
    
    // Parse action
    // Create temporary buffer for action (stack allocated, no dynamic memory)
    char actionBuf[16];
    if (actionLen >= sizeof(actionBuf)) {
        sendError("unknown_action");
        return;
    }
    memcpy(actionBuf, actionStart, actionLen);
    actionBuf[actionLen] = '\0';
    
    CommandAction action = parseAction(actionBuf);
    if (action == CommandAction::INVALID) {
        sendError("unknown_action");
        return;
    }
    
    // Skip whitespace to position
    ptr = skipWhitespace(actionEnd);
    
    if (*ptr == '\0') {
        sendError("bad_format");
        return;
    }
    
    // Find position token
    const char* posStart = ptr;
    const char* posEnd = findTokenEnd(ptr);
    size_t posLen = posEnd - posStart;
    
    // Position should be exactly 1 character (A-Y)
    if (posLen != 1) {
        sendError("unknown_position");
        return;
    }
    
    char posChar = *posStart;
    uint8_t position = LedController::charToPosition(posChar);
    
    if (position == 255) {
        sendError("unknown_position");
        return;
    }
    
    // Check for trailing garbage (ignoring whitespace)
    ptr = skipWhitespace(posEnd);
    if (*ptr != '\0') {
        sendError("bad_format");
        return;
    }
    
    // Execute command
    bool success = false;
    
    switch (action) {
        case CommandAction::SHOW:
            success = m_ledController.show(position);
            break;
            
        case CommandAction::HIDE:
            success = m_ledController.hide(position);
            break;
            
        case CommandAction::SUCCESS:
            success = m_ledController.success(position);
            break;
            
        default:
            sendError("unknown_action");
            return;
    }
    
    if (success) {
        // Convert position back to uppercase for ACK
        char posUpper = LedController::positionToChar(position);
        sendAck(action, posUpper);
    } else {
        sendError("command_failed");
    }
}

CommandAction CommandController::parseAction(const char* str) {
    size_t len = strlen(str);
    
    if (len == 4 && strcasecmp_n(str, "SHOW", 4)) {
        return CommandAction::SHOW;
    }
    if (len == 4 && strcasecmp_n(str, "HIDE", 4)) {
        return CommandAction::HIDE;
    }
    if (len == 7 && strcasecmp_n(str, "SUCCESS", 7)) {
        return CommandAction::SUCCESS;
    }
    
    return CommandAction::INVALID;
}

const char* CommandController::actionToString(CommandAction action) {
    switch (action) {
        case CommandAction::SHOW:    return "SHOW";
        case CommandAction::HIDE:    return "HIDE";
        case CommandAction::SUCCESS: return "SUCCESS";
        default:                     return "UNKNOWN";
    }
}

void CommandController::sendAck(CommandAction action, char position) {
    Serial.print("ACK ");
    Serial.print(actionToString(action));
    Serial.print(" ");
    Serial.println(position);
}

void CommandController::sendError(const char* reason) {
    Serial.print("ERR ");
    Serial.println(reason);
}

const char* CommandController::skipWhitespace(const char* str) {
    while (*str == ' ' || *str == '\t') {
        str++;
    }
    return str;
}

const char* CommandController::findTokenEnd(const char* str) {
    while (*str != '\0' && *str != ' ' && *str != '\t' && *str != '\n' && *str != '\r') {
        str++;
    }
    return str;
}

bool CommandController::strcasecmp_n(const char* a, const char* b, size_t len) {
    for (size_t i = 0; i < len; i++) {
        char ca = a[i];
        char cb = b[i];
        
        // Convert to uppercase for comparison
        if (ca >= 'a' && ca <= 'z') ca -= 32;
        if (cb >= 'a' && cb <= 'z') cb -= 32;
        
        if (ca != cb) {
            return false;
        }
    }
    return true;
}

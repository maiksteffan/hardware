/**
 * @file CommandController.cpp
 * @brief Implementation of serial command parser for LED strip control
 */

#include "CommandController.h"
#include "LedController.h"
#include "TouchController.h"
#include "SequenceController.h"

// ============================================================================
// Constructor
// ============================================================================

CommandController::CommandController(LedController& ledController, TouchController* touchController,
                                     SequenceController* sequenceController)
    : m_ledController(ledController)
    , m_touchController(touchController)
    , m_sequenceController(sequenceController)
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
    m_lastCharTime = 0;
}

void CommandController::update() {
    // Process all available serial data (non-blocking, handles burst input)
    while (Serial.available() > 0) {
        char c = Serial.read();
        m_lastCharTime = millis();  // Track when we received data
        
        // Debug: echo received character
        Serial.print("[RX:");
        Serial.print((int)c);
        Serial.print("]");
        
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
    
    // Timeout-based processing: if we have data in buffer and no new data for a while,
    // process the buffer even without a newline (handles senders that don't send \n)
    if (m_bufferIndex > 0 && m_lastCharTime > 0) {
        uint32_t elapsed = millis() - m_lastCharTime;
        if (elapsed >= COMMAND_TIMEOUT_MS) {
            // Null-terminate and process
            m_buffer[m_bufferIndex] = '\0';
            Serial.print("[TIMEOUT]");  // Debug
            processLine(m_buffer);
            
            // Reset buffer
            m_bufferIndex = 0;
            memset(m_buffer, 0, sizeof(m_buffer));
            m_lastCharTime = 0;
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
    
    // Handle SEQUENCE command: SEQUENCE(A,B,C,D)
    if (action == CommandAction::SEQUENCE) {
        // Find opening parenthesis
        ptr = actionEnd;
        while (*ptr == ' ' || *ptr == '\t') ptr++;
        
        if (*ptr != '(') {
            sendError("bad_format");
            return;
        }
        ptr++;  // Skip '('
        
        // Find closing parenthesis
        const char* seqStart = ptr;
        while (*ptr != '\0' && *ptr != ')') ptr++;
        
        if (*ptr != ')') {
            sendError("bad_format");
            return;
        }
        
        // Extract sequence string
        size_t seqLen = ptr - seqStart;
        char seqBuf[64];
        if (seqLen >= sizeof(seqBuf)) {
            sendError("sequence_too_long");
            return;
        }
        memcpy(seqBuf, seqStart, seqLen);
        seqBuf[seqLen] = '\0';
        
        // Start the sequence
        if (m_sequenceController) {
            if (m_sequenceController->startSequence(seqBuf)) {
                Serial.println("ACK SEQUENCE");
            }
            // Error messages are printed by SequenceController
        } else {
            sendError("no_sequence_controller");
        }
        return;
    }
    
    // Handle commands that don't require a position argument
    if (action == CommandAction::RECORD || action == CommandAction::SCAN) {
        // Skip whitespace after action
        ptr = skipWhitespace(actionEnd);
        
        // Check for trailing garbage (ignoring whitespace)
        if (*ptr != '\0') {
            sendError("bad_format");
            return;
        }
        
        bool success = false;
        
        if (action == CommandAction::RECORD) {
            if (m_touchController) {
                m_touchController->startRecording();
                success = true;
            } else {
                sendError("no_touch_controller");
                return;
            }
        } else if (action == CommandAction::SCAN) {
            if (m_touchController) {
                m_touchController->scanAddresses();
                success = true;
            } else {
                sendError("no_touch_controller");
                return;
            }
        }
        
        if (success) {
            Serial.print("ACK ");
            Serial.println(actionToString(action));
        } else {
            sendError("command_failed");
        }
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
            
        case CommandAction::EXPECT:
            if (m_touchController) {
                success = m_touchController->expectSensor(posChar);
            } else {
                sendError("no_touch_controller");
                return;
            }
            break;
            
        case CommandAction::RECALIBRATE:
            if (m_touchController) {
                success = m_touchController->recalibrate(position);
            } else {
                sendError("no_touch_controller");
                return;
            }
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
    if (len == 6 && strcasecmp_n(str, "EXPECT", 6)) {
        return CommandAction::EXPECT;
    }
    if (len == 6 && strcasecmp_n(str, "RECORD", 6)) {
        return CommandAction::RECORD;
    }
    if (len == 4 && strcasecmp_n(str, "SCAN", 4)) {
        return CommandAction::SCAN;
    }
    if (len == 11 && strcasecmp_n(str, "RECALIBRATE", 11)) {
        return CommandAction::RECALIBRATE;
    }
    if (len == 8 && strcasecmp_n(str, "SEQUENCE", 8)) {
        return CommandAction::SEQUENCE;
    }
    
    return CommandAction::INVALID;
}

const char* CommandController::actionToString(CommandAction action) {
    switch (action) {
        case CommandAction::SHOW:        return "SHOW";
        case CommandAction::HIDE:        return "HIDE";
        case CommandAction::SUCCESS:     return "SUCCESS";
        case CommandAction::EXPECT:      return "EXPECT";
        case CommandAction::RECORD:      return "RECORD";
        case CommandAction::SCAN:        return "SCAN";
        case CommandAction::RECALIBRATE: return "RECALIBRATE";
        case CommandAction::SEQUENCE:    return "SEQUENCE";
        default:                         return "UNKNOWN";
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
    while (*str != '\0' && *str != ' ' && *str != '\t' && *str != '\n' && *str != '\r' && *str != '(') {
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

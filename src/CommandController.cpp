/**
 * @file CommandController.cpp
 * @brief Implementation of serial command parser and executor
 * 
 * Protocol v2 implementation with:
 * - Non-blocking serial read via ring buffer
 * - Command ID support for request-response correlation
 * - Long-running command support (SCAN, SUCCESS animation)
 */

#include "CommandController.h"
#include "LedController.h"
#include "TouchController.h"
#include "EventQueue.h"

// ============================================================================
// Constructor
// ============================================================================

CommandController::CommandController(LedController& ledController, 
                                     TouchController* touchController,
                                     EventQueue& eventQueue)
    : m_ledController(ledController)
    , m_touchController(touchController)
    , m_eventQueue(eventQueue)
    , m_rxHead(0)
    , m_rxTail(0)
    , m_lineIndex(0)
    , m_lineOverflow(false)
{
}

// ============================================================================
// Public Methods
// ============================================================================

void CommandController::begin() {
    // Clear ring buffer
    m_rxHead = 0;
    m_rxTail = 0;
    
    // Clear line buffer
    m_lineIndex = 0;
    m_lineOverflow = false;
    memset(m_lineBuffer, 0, sizeof(m_lineBuffer));
    
    // Clear command queue
    for (uint8_t i = 0; i < COMMAND_QUEUE_SIZE; i++) {
        m_commandQueue[i].active = false;
    }
}

void CommandController::pollSerial() {
    // Read all available bytes into ring buffer (non-blocking)
    while (Serial.available() > 0) {
        char c = Serial.read();
        
        // Calculate next head position
        uint8_t nextHead = (m_rxHead + 1) % sizeof(m_rxBuffer);
        
        // Check for buffer full (leave one slot empty)
        if (nextHead != m_rxTail) {
            m_rxBuffer[m_rxHead] = c;
            m_rxHead = nextHead;
        }
        // If buffer full, silently drop characters
    }
}

void CommandController::processCompletedLines() {
    // Try to extract and process complete lines
    while (extractLine()) {
        // Check for overflow condition
        if (m_lineOverflow) {
            m_eventQueue.queueError("line_too_long", NO_COMMAND_ID);
            m_lineOverflow = false;
            continue;
        }
        
        // Skip empty lines
        const char* line = skipWhitespace(m_lineBuffer);
        if (*line == '\0') {
            continue;
        }
        
        // Parse the line
        ParsedCommand cmd;
        if (!parseLine(line, cmd)) {
            // Error already queued by parseLine
            continue;
        }
        
        // Execute the command
        executeCommand(cmd);
    }
}

void CommandController::tick() {
    // Tick all active long-running commands
    for (uint8_t i = 0; i < COMMAND_QUEUE_SIZE; i++) {
        if (m_commandQueue[i].active) {
            tickCommand(m_commandQueue[i]);
        }
    }
}

bool CommandController::isQueueFull() const {
    for (uint8_t i = 0; i < COMMAND_QUEUE_SIZE; i++) {
        if (!m_commandQueue[i].active) {
            return false;
        }
    }
    return true;
}

void CommandController::injectCommand(const char* line) {
    if (!line || *line == '\0') {
        return;
    }
    
    // Skip any prefix like "PI> "
    if (strncmp(line, "PI> ", 4) == 0) {
        line += 4;
    }
    
    // Parse the line
    ParsedCommand cmd;
    if (!parseLine(line, cmd)) {
        return; // Error already queued
    }
    
    // Execute the command
    executeCommand(cmd);
}

// ============================================================================
// Serial/Parsing Methods
// ============================================================================

bool CommandController::extractLine() {
    m_lineIndex = 0;
    m_lineOverflow = false;
    
    // Check if theres data in ring buffer
    if (m_rxHead == m_rxTail) {
        return false;  // Buffer empty
    }
    
    // Scan for newline
    uint8_t pos = m_rxTail;
    bool foundNewline = false;
    
    while (pos != m_rxHead) {
        char c = m_rxBuffer[pos];
        
        if (c == '\n' || c == '\r') {
            foundNewline = true;
            break;
        }
        
        pos = (pos + 1) % sizeof(m_rxBuffer);
    }
    
    if (!foundNewline) {
        // Check if we've accumulated too much without newline
        uint8_t pending = (m_rxHead >= m_rxTail) 
            ? (m_rxHead - m_rxTail)
            : (sizeof(m_rxBuffer) - m_rxTail + m_rxHead);
            
        if (pending >= MAX_LINE_LEN) {
            // Discard everything up to MAX_LINE_LEN
            m_lineOverflow = true;
            m_rxTail = (m_rxTail + MAX_LINE_LEN) % sizeof(m_rxBuffer);
            return true;  // Signal to caller that we have an overflow
        }
        
        return false;  // No complete line yet
    }
    
    // Extract characters up to newline
    while (m_rxTail != m_rxHead) {
        char c = m_rxBuffer[m_rxTail];
        m_rxTail = (m_rxTail + 1) % sizeof(m_rxBuffer);
        
        // Stop at newline
        if (c == '\n' || c == '\r') {
            // Skip any additional CR/LF
            while (m_rxTail != m_rxHead) {
                char next = m_rxBuffer[m_rxTail];
                if (next != '\n' && next != '\r') {
                    break;
                }
                m_rxTail = (m_rxTail + 1) % sizeof(m_rxBuffer);
            }
            break;
        }
        
        // Add to line buffer if room
        if (m_lineIndex < MAX_LINE_LEN - 1) {
            m_lineBuffer[m_lineIndex++] = c;
        } else {
            m_lineOverflow = true;
        }
    }
    
    // Null-terminate
    m_lineBuffer[m_lineIndex] = '\0';
    
    return true;
}

bool CommandController::parseLine(const char* line, ParsedCommand& cmd) {
    // Initialize command
    cmd.action = CommandAction::INVALID;
    cmd.hasPosition = false;
    cmd.position = 0;
    cmd.positionIndex = 255;
    cmd.hasId = false;
    cmd.id = NO_COMMAND_ID;
    cmd.valid = false;
    
    const char* ptr = skipWhitespace(line);
    
    // Check for and strip "PI> " prefix (commands from Pi/MockPi)
    if (strncmp(ptr, "PI>", 3) == 0) {
        ptr += 3;
        ptr = skipWhitespace(ptr);
    }
    
    // Find action token
    const char* actionStart = ptr;
    const char* actionEnd = findTokenEnd(ptr);
    size_t actionLen = actionEnd - actionStart;
    
    if (actionLen == 0) {
        m_eventQueue.queueError("bad_format", NO_COMMAND_ID);
        return false;
    }
    
    // Parse action
    cmd.action = parseAction(actionStart, actionLen);
    if (cmd.action == CommandAction::INVALID) {
        m_eventQueue.queueError("unknown_action", NO_COMMAND_ID);
        return false;
    }
    
    // Move past action
    ptr = skipWhitespace(actionEnd);
    
    // Check if action requires position
    bool needsPos = actionRequiresPosition(cmd.action);
    
    // Parse remaining tokens (position and/or #id)
    while (*ptr != '\0') {
        // Check for command ID token
        if (*ptr == '#') {
            ptr++;  // Skip #
            
            // Parse number
            uint32_t id = 0;
            bool hasDigits = false;
            
            while (*ptr >= '0' && *ptr <= '9') {
                id = id * 10 + (*ptr - '0');
                ptr++;
                hasDigits = true;
            }
            
            if (!hasDigits) {
                m_eventQueue.queueError("bad_format", NO_COMMAND_ID);
                return false;
            }
            
            cmd.hasId = true;
            cmd.id = id;
            
            ptr = skipWhitespace(ptr);
            continue;
        }
        
        // Must be position token
        const char* tokenStart = ptr;
        const char* tokenEnd = findTokenEnd(ptr);
        size_t tokenLen = tokenEnd - tokenStart;
        
        if (tokenLen == 1) {
            // Single character - could be position
            char c = *tokenStart;
            uint8_t idx = charToIndex(c);
            
            if (idx != 255) {
                cmd.hasPosition = true;
                cmd.position = (c >= 'a' && c <= 'z') ? (c - 32) : c;  // Uppercase
                cmd.positionIndex = idx;
            } else {
                m_eventQueue.queueError("unknown_position", cmd.hasId ? cmd.id : NO_COMMAND_ID);
                return false;
            }
        } else if (tokenLen > 1) {
            // Multi-character token that's not a command ID - error
            m_eventQueue.queueError("bad_format", cmd.hasId ? cmd.id : NO_COMMAND_ID);
            return false;
        }
        
        ptr = skipWhitespace(tokenEnd);
    }
    
    // Validate that position is present if required
    if (needsPos && !cmd.hasPosition) {
        m_eventQueue.queueError("bad_format", cmd.hasId ? cmd.id : NO_COMMAND_ID);
        return false;
    }
    
    cmd.valid = true;
    return true;
}

CommandAction CommandController::parseAction(const char* str, size_t len) {
    if (len == 4 && strcasecmpN(str, "SHOW", 4)) {
        return CommandAction::SHOW;
    }
    if (len == 4 && strcasecmpN(str, "HIDE", 4)) {
        return CommandAction::HIDE;
    }
    if (len == 7 && strcasecmpN(str, "SUCCESS", 7)) {
        return CommandAction::SUCCESS;
    }
    if (len == 5 && strcasecmpN(str, "BLINK", 5)) {
        return CommandAction::BLINK;
    }
    if (len == 10 && strcasecmpN(str, "STOP_BLINK", 10)) {
        return CommandAction::STOP_BLINK;
    }
    if (len == 11 && strcasecmpN(str, "EXPECT_DOWN", 11)) {
        return CommandAction::EXPECT_DOWN;
    }
    if (len == 9 && strcasecmpN(str, "EXPECT_UP", 9)) {
        return CommandAction::EXPECT_UP;
    }
    if (len == 11 && strcasecmpN(str, "RECALIBRATE", 11)) {
        return CommandAction::RECALIBRATE;
    }
    if (len == 15 && strcasecmpN(str, "RECALIBRATE_ALL", 15)) {
        return CommandAction::RECALIBRATE_ALL;
    }
    if (len == 4 && strcasecmpN(str, "SCAN", 4)) {
        return CommandAction::SCAN;
    }
    if (len == 18 && strcasecmpN(str, "SEQUENCE_COMPLETED", 18)) {
        return CommandAction::SEQUENCE_COMPLETED;
    }
    if (len == 4 && strcasecmpN(str, "INFO", 4)) {
        return CommandAction::INFO;
    }
    if (len == 4 && strcasecmpN(str, "PING", 4)) {
        return CommandAction::PING;
    }
    
    return CommandAction::INVALID;
}

const char* CommandController::actionToString(CommandAction action) {
    switch (action) {
        case CommandAction::SHOW:               return "SHOW";
        case CommandAction::HIDE:               return "HIDE";
        case CommandAction::SUCCESS:            return "SUCCESS";
        case CommandAction::BLINK:              return "BLINK";
        case CommandAction::STOP_BLINK:         return "STOP_BLINK";
        case CommandAction::EXPECT_DOWN:        return "EXPECT_DOWN";
        case CommandAction::EXPECT_UP:          return "EXPECT_UP";
        case CommandAction::RECALIBRATE:        return "RECALIBRATE";
        case CommandAction::RECALIBRATE_ALL:    return "RECALIBRATE_ALL";
        case CommandAction::SCAN:               return "SCAN";
        case CommandAction::SEQUENCE_COMPLETED: return "SEQUENCE_COMPLETED";
        case CommandAction::INFO:               return "INFO";
        case CommandAction::PING:               return "PING";
        default:                                return "UNKNOWN";
    }
}

bool CommandController::actionRequiresPosition(CommandAction action) {
    switch (action) {
        case CommandAction::SHOW:
        case CommandAction::HIDE:
        case CommandAction::SUCCESS:
        case CommandAction::BLINK:
        case CommandAction::STOP_BLINK:
        case CommandAction::EXPECT_DOWN:
        case CommandAction::EXPECT_UP:
        case CommandAction::RECALIBRATE:
            return true;
        default:
            return false;
    }
}

bool CommandController::actionIsLongRunning(CommandAction action) {
    switch (action) {
        case CommandAction::SUCCESS:
        case CommandAction::SCAN:
        case CommandAction::RECALIBRATE_ALL:
        case CommandAction::SEQUENCE_COMPLETED:
            return true;
        default:
            return false;
    }
}

// ============================================================================
// Execution Methods
// ============================================================================

void CommandController::executeCommand(const ParsedCommand& cmd) {
    uint32_t id = cmd.hasId ? cmd.id : NO_COMMAND_ID;
    
    if (actionIsLongRunning(cmd.action)) {
        // Queue for background execution
        if (!queueCommand(cmd)) {
            m_eventQueue.queueError("busy", id);
        }
        // ACK sent when queued
    } else {
        // Execute immediately
        executeInstant(cmd);
    }
}

void CommandController::executeInstant(const ParsedCommand& cmd) {
    uint32_t id = cmd.hasId ? cmd.id : NO_COMMAND_ID;
    const char* action = actionToString(cmd.action);
    bool success = false;
    
    switch (cmd.action) {
        case CommandAction::SHOW:
            success = m_ledController.show(cmd.positionIndex);
            if (success) {
                m_eventQueue.queueAck(action, cmd.position, id);
            } else {
                m_eventQueue.queueError("command_failed", id);
            }
            break;
            
        case CommandAction::HIDE:
            success = m_ledController.hide(cmd.positionIndex);
            if (success) {
                m_eventQueue.queueAck(action, cmd.position, id);
            } else {
                m_eventQueue.queueError("command_failed", id);
            }
            break;
            
        case CommandAction::BLINK:
            success = m_ledController.blink(cmd.positionIndex);
            if (success) {
                m_eventQueue.queueAck(action, cmd.position, id);
            } else {
                m_eventQueue.queueError("command_failed", id);
            }
            break;
            
        case CommandAction::STOP_BLINK:
            success = m_ledController.stopBlink(cmd.positionIndex);
            if (success) {
                m_eventQueue.queueAck(action, cmd.position, id);
            } else {
                m_eventQueue.queueError("command_failed", id);
            }
            break;
            
        case CommandAction::RECALIBRATE:
            if (m_touchController) {
                success = m_touchController->recalibrate(cmd.positionIndex);
                if (success) {
                    m_eventQueue.queueAck(action, cmd.position, id);
                    // Also emit RECALIBRATED event per protocol
                    m_eventQueue.queueRecalibrated(cmd.position, id);
                } else {
                    m_eventQueue.queueError("command_failed", id);
                }
            } else {
                m_eventQueue.queueError("no_touch_controller", id);
            }
            break;
            
        case CommandAction::EXPECT_DOWN:
            if (m_touchController) {
                m_touchController->setExpectDown(cmd.positionIndex, id);
                m_eventQueue.queueAck(action, cmd.position, id);
                // TOUCHED_DOWN will be emitted later when touch detected
            } else {
                m_eventQueue.queueError("no_touch_controller", id);
            }
            break;
            
        case CommandAction::EXPECT_UP:
            if (m_touchController) {
                m_touchController->setExpectUp(cmd.positionIndex, id);
                m_eventQueue.queueAck(action, cmd.position, id);
                // TOUCHED_UP will be emitted later when release detected
            } else {
                m_eventQueue.queueError("no_touch_controller", id);
            }
            break;
            
        case CommandAction::INFO:
            m_eventQueue.queueInfo(id);
            break;
            
        case CommandAction::PING:
            m_eventQueue.queueAck("PING", 0, id);
            break;
            
        default:
            m_eventQueue.queueError("unknown_action", id);
            break;
    }
}

bool CommandController::queueCommand(const ParsedCommand& cmd) {
    // Find empty slot
    for (uint8_t i = 0; i < COMMAND_QUEUE_SIZE; i++) {
        if (!m_commandQueue[i].active) {
            m_commandQueue[i].command = cmd;
            m_commandQueue[i].active = true;
            m_commandQueue[i].startTime = millis();
            m_commandQueue[i].state = 0;
            m_commandQueue[i].scanAddress = 0;  // Used as sensor index for RECALIBRATE_ALL
            
            // Send ACK immediately
            uint32_t id = cmd.hasId ? cmd.id : NO_COMMAND_ID;
            const char* action = actionToString(cmd.action);
            
            if (cmd.action == CommandAction::SUCCESS) {
                // Start the animation
                m_ledController.success(cmd.positionIndex);
                m_eventQueue.queueAck(action, cmd.position, id);
            } else if (cmd.action == CommandAction::SCAN) {
                if (m_touchController) {
                    m_eventQueue.queueAck(action, 0, id);
                } else {
                    m_eventQueue.queueError("no_touch_controller", id);
                    m_commandQueue[i].active = false;
                    return false;
                }
            } else if (cmd.action == CommandAction::RECALIBRATE_ALL) {
                if (m_touchController) {
                    m_eventQueue.queueAck(action, 0, id);
                } else {
                    m_eventQueue.queueError("no_touch_controller", id);
                    m_commandQueue[i].active = false;
                    return false;
                }
            } else if (cmd.action == CommandAction::SEQUENCE_COMPLETED) {
                // Start the celebration animation
                m_ledController.startSequenceCompletedAnimation();
                m_eventQueue.queueAck(action, 0, id);
            }
            
            return true;
        }
    }
    
    return false;  // Queue full
}

void CommandController::tickCommand(QueuedCommand& qc) {
    if (!qc.active) {
        return;
    }
    
    uint32_t id = qc.command.hasId ? qc.command.id : NO_COMMAND_ID;
    
    switch (qc.command.action) {
        case CommandAction::SUCCESS: {
            // Check if animation is complete
            if (m_ledController.isAnimationComplete(qc.command.positionIndex)) {
                m_eventQueue.queueDone("SUCCESS", qc.command.position, id);
                qc.active = false;
            }
            break;
        }
        
        case CommandAction::SCAN: {
            // Build list of active sensors and emit SCANNED[A,B,C,...]
            if (m_touchController) {
                char sensorList[52];
                m_touchController->buildActiveSensorList(sensorList, sizeof(sensorList));
                m_eventQueue.queueScanned(sensorList, id);
            }
            qc.active = false;
            break;
        }
        
        case CommandAction::RECALIBRATE_ALL: {
            // Non-blocking recalibration - recalibrate a few sensors per tick
            const uint8_t SENSORS_PER_TICK = 5;
            
            if (m_touchController) {
                for (uint8_t i = 0; i < SENSORS_PER_TICK && qc.scanAddress < NUM_TOUCH_SENSORS; i++) {
                    m_touchController->recalibrate(qc.scanAddress);
                    qc.scanAddress++;
                }
                
                // Check if complete
                if (qc.scanAddress >= NUM_TOUCH_SENSORS) {
                    // Emit RECALIBRATED ALL
                    m_eventQueue.queueRecalibrated(0, id);  // 0 = ALL
                    qc.active = false;
                }
            } else {
                qc.active = false;
            }
            break;
        }
        
        case CommandAction::SEQUENCE_COMPLETED: {
            // Check if animation is complete
            if (m_ledController.isSequenceCompletedAnimationComplete()) {
                m_eventQueue.queueDone("SEQUENCE_COMPLETED", 0, id);
                qc.active = false;
            }
            break;
        }
        
        default:
            // Unknown long-running command - mark complete
            qc.active = false;
            break;
    }
}

// ============================================================================
// Utility Methods
// ======================================================
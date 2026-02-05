/**
 * @file CommandController.cpp
 * @brief Serial command parser and executor implementation
 * 
 * Handles all serial commands from the Raspberry Pi using a clean
 * non-blocking architecture.
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
    , m_lastRxTime(0)
    , m_lineIndex(0)
    , m_lineOverflow(false)
{
    memset(m_rxBuffer, 0, sizeof(m_rxBuffer));
    memset(m_lineBuffer, 0, sizeof(m_lineBuffer));
    
    for (uint8_t i = 0; i < COMMAND_QUEUE_SIZE; i++) {
        m_commandQueue[i].active = false;
    }
}

// ============================================================================
// Public Methods
// ============================================================================

void CommandController::begin() {
    m_rxHead = 0;
    m_rxTail = 0;
    m_lastRxTime = 0;
    m_lineIndex = 0;
    m_lineOverflow = false;
    
    for (uint8_t i = 0; i < COMMAND_QUEUE_SIZE; i++) {
        m_commandQueue[i].active = false;
    }
}

void CommandController::pollSerial() {
    while (Serial.available() > 0) {
        char c = Serial.read();
        m_lastRxTime = millis();
        
        // Store in ring buffer
        uint8_t nextHead = (m_rxHead + 1) % sizeof(m_rxBuffer);
        if (nextHead != m_rxTail) {
            m_rxBuffer[m_rxHead] = c;
            m_rxHead = nextHead;
        }
    }
}

void CommandController::processCompletedLines() {
    while (extractLine()) {
        if (m_lineBuffer[0] != '\0') {
            ParsedCommand cmd;
            if (parseLine(m_lineBuffer, cmd)) {
                executeCommand(cmd);
            }
        }
    }
}

void CommandController::tick() {
    // Tick all active queued commands
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

// ============================================================================
// Line Extraction
// ============================================================================

bool CommandController::extractLine() {
    // Note: m_lineIndex and m_lineOverflow are preserved between calls
    // to handle partial line data that arrives across multiple loop iterations.
    
    while (m_rxTail != m_rxHead) {
        char c = m_rxBuffer[m_rxTail];
        m_rxTail = (m_rxTail + 1) % sizeof(m_rxBuffer);
        
        if (c == '\n' || c == '\r') {
            if (m_lineIndex > 0) {
                m_lineBuffer[m_lineIndex] = '\0';
                m_lineIndex = 0;  // Reset for next line
                m_lineOverflow = false;
                return true;
            }
            // Skip empty lines (consecutive \r\n or \n\r)
            continue;
        }
        
        if (m_lineIndex < MAX_LINE_LEN - 1) {
            m_lineBuffer[m_lineIndex++] = c;
        } else {
            m_lineOverflow = true;
        }
    }
    
    // Timeout-based line completion for terminals that don't send line endings
    // If we have data in the line buffer and no new data for 50ms, treat as complete
    if (m_lineIndex > 0 && (millis() - m_lastRxTime) > 50) {
        m_lineBuffer[m_lineIndex] = '\0';
        m_lineIndex = 0;
        m_lineOverflow = false;
        return true;
    }
    
    return false;
}

// ============================================================================
// Command Parsing
// ============================================================================

bool CommandController::parseLine(const char* line, ParsedCommand& cmd) {
    cmd.action = CommandAction::INVALID;
    cmd.hasPosition = false;
    cmd.position = 0;
    cmd.positionIndex = 255;
    cmd.hasId = false;
    cmd.id = NO_COMMAND_ID;
    cmd.extraValue = 0;
    cmd.r = 0;
    cmd.g = 0;
    cmd.b = 0;
    cmd.range = 0;
    cmd.valid = false;
    
    const char* p = skipWhitespace(line);
    if (*p == '\0') return false;
    
    // Parse action
    const char* actionStart = p;
    const char* actionEnd = findTokenEnd(p);
    size_t actionLen = actionEnd - actionStart;
    
    cmd.action = parseAction(actionStart, actionLen);
    if (cmd.action == CommandAction::INVALID) {
        m_eventQueue.queueError("unknown_action", NO_COMMAND_ID);
        return false;
    }
    
    p = skipWhitespace(actionEnd);
    
    // Special parsing for MENUE_CHANGE: <r,g,b> <range>
    if (cmd.action == CommandAction::MENUE_CHANGE) {
        // Parse R value
        uint16_t val = 0;
        while (*p >= '0' && *p <= '9') {
            val = val * 10 + (*p - '0');
            p++;
        }
        if (val > 255) { m_eventQueue.queueError("bad_format", NO_COMMAND_ID); return false; }
        cmd.r = (uint8_t)val;
        
        if (*p != ',') { m_eventQueue.queueError("bad_format", NO_COMMAND_ID); return false; }
        p++;
        
        // Parse G value
        val = 0;
        while (*p >= '0' && *p <= '9') {
            val = val * 10 + (*p - '0');
            p++;
        }
        if (val > 255) { m_eventQueue.queueError("bad_format", NO_COMMAND_ID); return false; }
        cmd.g = (uint8_t)val;
        
        if (*p != ',') { m_eventQueue.queueError("bad_format", NO_COMMAND_ID); return false; }
        p++;
        
        // Parse B value
        val = 0;
        while (*p >= '0' && *p <= '9') {
            val = val * 10 + (*p - '0');
            p++;
        }
        if (val > 255) { m_eventQueue.queueError("bad_format", NO_COMMAND_ID); return false; }
        cmd.b = (uint8_t)val;
        
        p = skipWhitespace(p);
        
        // Parse range
        val = 0;
        while (*p >= '0' && *p <= '9') {
            val = val * 10 + (*p - '0');
            p++;
        }
        if (val > 255) { m_eventQueue.queueError("bad_format", NO_COMMAND_ID); return false; }
        cmd.range = (uint8_t)val;
        
        p = skipWhitespace(p);
        cmd.valid = true;
        return true;
    }
    
    // Parse position (if applicable)
    if (actionRequiresPosition(cmd.action)) {
        if (*p == '\0' || *p == '#') {
            m_eventQueue.queueError("bad_format", NO_COMMAND_ID);
            return false;
        }
        
        cmd.position = (*p >= 'a' && *p <= 'z') ? (*p - 32) : *p;
        cmd.positionIndex = charToIndex(cmd.position);
        
        if (cmd.positionIndex == 255) {
            m_eventQueue.queueError("unknown_position", NO_COMMAND_ID);
            return false;
        }
        
        cmd.hasPosition = true;
        p = skipWhitespace(p + 1);
    }
    
    // Parse extra numeric value if needed (e.g., sensitivity level)
    if (cmd.action == CommandAction::SET_SENSITIVITY) {
        if (*p == '\0' || *p == '#') {
            m_eventQueue.queueError("bad_format", NO_COMMAND_ID);
            return false;
        }
        
        uint8_t val = 0;
        while (*p >= '0' && *p <= '9') {
            val = val * 10 + (*p - '0');
            p++;
        }
        
        if (val > 7) {
            m_eventQueue.queueError("invalid_level", NO_COMMAND_ID);
            return false;
        }
        
        cmd.extraValue = val;
        p = skipWhitespace(p);
    }
    
    // Parse optional command ID (#number)
    if (*p == '#') {
        p++;
        cmd.id = 0;
        while (*p >= '0' && *p <= '9') {
            cmd.id = cmd.id * 10 + (*p - '0');
            p++;
        }
        cmd.hasId = true;
    }
    
    cmd.valid = true;
    return true;
}

CommandAction CommandController::parseAction(const char* str, size_t len) {
    if (strcasecmpN(str, "SHOW", len)) return CommandAction::SHOW;
    if (strcasecmpN(str, "HIDE_ALL", len)) return CommandAction::HIDE_ALL;
    if (strcasecmpN(str, "HIDE", len)) return CommandAction::HIDE;
    if (strcasecmpN(str, "SUCCESS", len)) return CommandAction::SUCCESS;
    if (strcasecmpN(str, "FAIL", len)) return CommandAction::FAIL;
    if (strcasecmpN(str, "CONTRACT", len)) return CommandAction::CONTRACT;
    if (strcasecmpN(str, "BLINK", len)) return CommandAction::BLINK;
    if (strcasecmpN(str, "STOP_BLINK", len)) return CommandAction::STOP_BLINK;
    if (strcasecmpN(str, "EXPAND_STEP", len)) return CommandAction::EXPAND_STEP;
    if (strcasecmpN(str, "CONTRACT_STEP", len)) return CommandAction::CONTRACT_STEP;
    if (strcasecmpN(str, "MENUE_CHANGE", len)) return CommandAction::MENUE_CHANGE;
    if (strcasecmpN(str, "EXPECT", len)) return CommandAction::EXPECT;
    if (strcasecmpN(str, "EXPECT_RELEASE", len)) return CommandAction::EXPECT_RELEASE;
    if (strcasecmpN(str, "RECALIBRATE", len)) return CommandAction::RECALIBRATE;
    if (strcasecmpN(str, "RECALIBRATE_ALL", len)) return CommandAction::RECALIBRATE_ALL;
    if (strcasecmpN(str, "VALUE", len)) return CommandAction::VALUE;
    if (strcasecmpN(str, "SET_SENSITIVITY", len)) return CommandAction::SET_SENSITIVITY;
    if (strcasecmpN(str, "SCAN", len)) return CommandAction::SCAN;
    if (strcasecmpN(str, "SEQUENCE_COMPLETED", len)) return CommandAction::SEQUENCE_COMPLETED;
    if (strcasecmpN(str, "INFO", len)) return CommandAction::INFO;
    if (strcasecmpN(str, "PING", len)) return CommandAction::PING;
    return CommandAction::INVALID;
}

const char* CommandController::actionToString(CommandAction action) {
    switch (action) {
        case CommandAction::SHOW: return "SHOW";
        case CommandAction::HIDE: return "HIDE";
        case CommandAction::HIDE_ALL: return "HIDE_ALL";
        case CommandAction::SUCCESS: return "SUCCESS";
        case CommandAction::FAIL: return "FAIL";
        case CommandAction::CONTRACT: return "CONTRACT";
        case CommandAction::BLINK: return "BLINK";
        case CommandAction::STOP_BLINK: return "STOP_BLINK";
        case CommandAction::EXPAND_STEP: return "EXPAND_STEP";
        case CommandAction::CONTRACT_STEP: return "CONTRACT_STEP";
        case CommandAction::MENUE_CHANGE: return "MENUE_CHANGE";
        case CommandAction::EXPECT: return "EXPECT";
        case CommandAction::EXPECT_RELEASE: return "EXPECT_RELEASE";
        case CommandAction::RECALIBRATE: return "RECALIBRATE";
        case CommandAction::RECALIBRATE_ALL: return "RECALIBRATE_ALL";
        case CommandAction::VALUE: return "VALUE";
        case CommandAction::SET_SENSITIVITY: return "SET_SENSITIVITY";
        case CommandAction::SCAN: return "SCAN";
        case CommandAction::SEQUENCE_COMPLETED: return "SEQUENCE_COMPLETED";
        case CommandAction::INFO: return "INFO";
        case CommandAction::PING: return "PING";
        default: return "INVALID";
    }
}

bool CommandController::actionRequiresPosition(CommandAction action) {
    switch (action) {
        case CommandAction::SHOW:
        case CommandAction::HIDE:
        case CommandAction::SUCCESS:
        case CommandAction::FAIL:
        case CommandAction::CONTRACT:
        case CommandAction::BLINK:
        case CommandAction::STOP_BLINK:
        case CommandAction::EXPAND_STEP:
        case CommandAction::CONTRACT_STEP:
        case CommandAction::EXPECT:
        case CommandAction::EXPECT_RELEASE:
        case CommandAction::RECALIBRATE:
        case CommandAction::VALUE:
        case CommandAction::SET_SENSITIVITY:
            return true;
        default:
            return false;
    }
}

bool CommandController::actionIsLongRunning(CommandAction action) {
    switch (action) {
        case CommandAction::SUCCESS:
        case CommandAction::CONTRACT:
        case CommandAction::SEQUENCE_COMPLETED:
        case CommandAction::MENUE_CHANGE:
            return true;
        default:
            return false;
    }
}

// ============================================================================
// Command Execution
// ============================================================================

void CommandController::executeCommand(const ParsedCommand& cmd) {
    if (!cmd.valid) return;
    
    uint32_t cmdId = cmd.hasId ? cmd.id : NO_COMMAND_ID;
    
    if (actionIsLongRunning(cmd.action)) {
        if (!queueCommand(cmd)) {
            m_eventQueue.queueError("busy", cmdId);
            return;
        }
    } else {
        executeInstant(cmd);
    }
}

void CommandController::executeInstant(const ParsedCommand& cmd) {
    uint32_t cmdId = cmd.hasId ? cmd.id : NO_COMMAND_ID;
    const char* actionStr = actionToString(cmd.action);
    
    switch (cmd.action) {
        case CommandAction::SHOW:
            if (m_ledController.show(cmd.positionIndex)) {
                m_eventQueue.queueAck(actionStr, cmd.position, cmdId);
            } else {
                m_eventQueue.queueError("command_failed", cmdId);
            }
            break;
            
        case CommandAction::HIDE:
            if (m_ledController.hide(cmd.positionIndex)) {
                m_eventQueue.queueAck(actionStr, cmd.position, cmdId);
            } else {
                m_eventQueue.queueError("command_failed", cmdId);
            }
            break;
            
        case CommandAction::HIDE_ALL:
            m_ledController.hideAll();
            m_eventQueue.queueAck(actionStr, 0, cmdId);
            break;
            
        case CommandAction::FAIL:
            if (m_ledController.fail(cmd.positionIndex)) {
                m_eventQueue.queueAck(actionStr, cmd.position, cmdId);
            } else {
                m_eventQueue.queueError("command_failed", cmdId);
            }
            break;
            
        case CommandAction::BLINK:
            if (m_ledController.blink(cmd.positionIndex)) {
                m_eventQueue.queueAck(actionStr, cmd.position, cmdId);
            } else {
                m_eventQueue.queueError("command_failed", cmdId);
            }
            break;
            
        case CommandAction::STOP_BLINK:
            if (m_ledController.stopBlink(cmd.positionIndex)) {
                m_eventQueue.queueAck(actionStr, cmd.position, cmdId);
            } else {
                m_eventQueue.queueError("command_failed", cmdId);
            }
            break;
            
        case CommandAction::EXPAND_STEP:
            if (m_ledController.expandStep(cmd.positionIndex)) {
                m_eventQueue.queueAck(actionStr, cmd.position, cmdId);
            } else {
                m_eventQueue.queueError("command_failed", cmdId);
            }
            break;
            
        case CommandAction::
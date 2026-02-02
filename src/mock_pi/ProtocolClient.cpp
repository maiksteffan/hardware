/**
 * @file ProtocolClient.cpp
 * @brief Implementation of protocol client
 * 
 * Commands are injected directly into CommandController (same device).
 * Events are read from Serial output.
 */

#include "ProtocolClient.h"
#include "CommandController.h"
#include <string.h>

namespace MockPI {

// Arduino event prefix
static const char* ARDUINO_PREFIX = "ARDUINO> ";
static const uint8_t ARDUINO_PREFIX_LEN = 9;

ProtocolClient::ProtocolClient()
    : _serial(nullptr)
    , _cmdController(nullptr)
    , _listener(nullptr)
    , _nextCommandId(1)
    , _verbose(true)
{
}

void ProtocolClient::begin(Stream* serial, CommandController* cmdController) {
    _serial = serial;
    _cmdController = cmdController;
    _lineReader.begin(serial);
    _nextCommandId = 1;
    
    // Clear pending
    for (uint8_t i = 0; i < MAX_PENDING_COMMANDS; i++) {
        _pending[i].active = false;
    }
}

void ProtocolClient::setListener(EventListener* listener) {
    _listener = listener;
}

void ProtocolClient::setVerbose(bool verbose) {
    _verbose = verbose;
}

void ProtocolClient::tick() {
    _lineReader.poll();
    
    while (_lineReader.hasLine()) {
        const char* line = _lineReader.getLine();
        
        // Log received line
        if (_verbose) {
            logRx(line);
        }
        
        // Parse and dispatch
        ParsedEvent event;
        if (parseLine(line, event)) {
            event.timestamp = millis();
            
            if (_listener) {
                _listener->onEvent(event);
            }
        }
        
        _lineReader.consumeLine();
    }
}

uint32_t ProtocolClient::nextId() {
    return _nextCommandId++;
}

uint32_t ProtocolClient::peekNextId() const {
    return _nextCommandId;
}

uint32_t ProtocolClient::sendCommand(const char* action, uint8_t pos) {
    if (!_cmdController || pos >= NUM_POSITIONS) return 0;
    
    uint32_t id = nextId();
    
    char cmd[64];
    snprintf(cmd, sizeof(cmd), "%s %c #%lu", action, posToChar(pos), (unsigned long)id);
    
    // Inject command directly into CommandController
    _cmdController->injectCommand(cmd);
    
    if (_verbose) {
        logTx(cmd);
    }
    
    addPending(id, action, pos);
    
    return id;
}

uint32_t ProtocolClient::sendCommandNoPos(const char* action) {
    if (!_cmdController) return 0;
    
    uint32_t id = nextId();
    
    char cmd[64];
    snprintf(cmd, sizeof(cmd), "%s #%lu", action, (unsigned long)id);
    
    // Inject command directly into CommandController
    _cmdController->injectCommand(cmd);
    
    if (_verbose) {
        logTx(cmd);
    }
    
    addPending(id, action, INVALID_POS);
    
    return id;
}

uint32_t ProtocolClient::sendShow(uint8_t pos) {
    return sendCommand("SHOW", pos);
}

uint32_t ProtocolClient::sendHide(uint8_t pos) {
    return sendCommand("HIDE", pos);
}

uint32_t ProtocolClient::sendSuccess(uint8_t pos) {
    return sendCommand("SUCCESS", pos);
}

uint32_t ProtocolClient::sendBlink(uint8_t pos) {
    return sendCommand("BLINK", pos);
}

uint32_t ProtocolClient::sendStopBlink(uint8_t pos) {
    return sendCommand("STOP_BLINK", pos);
}

uint32_t ProtocolClient::sendMistake(uint8_t pos) {
    return sendCommand("MISTAKE", pos);
}

uint32_t ProtocolClient::sendExpect(uint8_t pos) {
    return sendCommand("EXPECT", pos);
}

uint32_t ProtocolClient::sendExpectRelease(uint8_t pos) {
    return sendCommand("EXPECT_RELEASE", pos);
}

uint32_t ProtocolClient::sendSequenceCompleted() {
    return sendCommandNoPos("SEQUENCE_COMPLETED");
}

uint32_t ProtocolClient::sendPing() {
    return sendCommandNoPos("PING");
}

uint32_t ProtocolClient::sendInfo() {
    return sendCommandNoPos("INFO");
}

void ProtocolClient::logTx(const char* cmd) {
    if (_serial) {
        _serial->print("MOCKPI> TX: ");
        _serial->println(cmd);
    }
}

void ProtocolClient::logRx(const char* line) {
    if (_serial) {
        _serial->print("MOCKPI> RX: ");
        _serial->println(line);
    }
}

bool ProtocolClient::parseLine(const char* line, ParsedEvent& event) {
    event.clear();
    
    if (!line) return false;
    
    // Check for ARDUINO> prefix
    if (strncmp(line, ARDUINO_PREFIX, ARDUINO_PREFIX_LEN) != 0) {
        return false;
    }
    
    // Skip prefix
    const char* content = line + ARDUINO_PREFIX_LEN;
    
    // Skip leading whitespace
    while (*content == ' ') content++;
    
    if (*content == '\0') return false;
    
    // Parse first word (event type/action)
    char firstWord[20];
    uint8_t i = 0;
    while (*content && *content != ' ' && i < sizeof(firstWord) - 1) {
        firstWord[i++] = *content++;
    }
    firstWord[i] = '\0';
    
    // Skip whitespace
    while (*content == ' ') content++;
    
    // Determine event type
    if (strcmp(firstWord, "ACK") == 0) {
        event.type = EventType::ACK;
        // Next word is action
        i = 0;
        while (*content && *content != ' ' && i < sizeof(event.action) - 1) {
            event.action[i++] = *content++;
        }
        event.action[i] = '\0';
        while (*content == ' ') content++;
    }
    else if (strcmp(firstWord, "DONE") == 0) {
        event.type = EventType::DONE;
        // Next word is action
        i = 0;
        while (*content && *content != ' ' && i < sizeof(event.action) - 1) {
            event.action[i++] = *content++;
        }
        event.action[i] = '\0';
        while (*content == ' ') content++;
    }
    else if (strcmp(firstWord, "ERR") == 0) {
        event.type = EventType::ERR;
        // Next word is reason
        i = 0;
        while (*content && *content != ' ' && i < sizeof(event.reason) - 1) {
            event.reason[i++] = *content++;
        }
        event.reason[i] = '\0';
        while (*content == ' ') content++;
    }
    else if (strcmp(firstWord, "TOUCHED") == 0) {
        event.type = EventType::TOUCHED;
    }
    else if (strcmp(firstWord, "TOUCH_RELEASED") == 0) {
        event.type = EventType::TOUCH_RELEASED;
    }
    else if (strcmp(firstWord, "TOUCH_DOWN") == 0) {
        event.type = EventType::TOUCH_DOWN;
    }
    else if (strcmp(firstWord, "TOUCH_UP") == 0) {
        event.type = EventType::TOUCH_UP;
    }
    else if (strcmp(firstWord, "INFO") == 0) {
        event.type = EventType::INFO;
        strncpy(event.action, "INFO", sizeof(event.action) - 1);
    }
    else {
        event.type = EventType::UNKNOWN;
        strncpy(event.action, firstWord, sizeof(event.action) - 1);
    }
    
    // Parse position (single letter A-Y)
    if (*content && isValidPosChar(*content)) {
        event.position = charToPos(*content);
        content++;
        while (*content == ' ') content++;
    }
    
    // Parse command ID (#xxx)
    if (*content == '#') {
        content++;
        event.commandId = 0;
        while (*content >= '0' && *content <= '9') {
            event.commandId = event.commandId * 10 + (*content - '0');
            content++;
        }
    }
    
    return true;
}

PendingCommand* ProtocolClient::findPending(uint32_t id) {
    for (uint8_t i = 0; i < MAX_PENDING_COMMANDS; i++) {
        if (_pending[i].active && _pending[i].commandId == id) {
            return &_pending[i];
        }
    }
    return nullptr;
}

void ProtocolClient::addPending(uint32_t id, const char* action, uint8_t pos) {
    // Find empty slot
    for (uint8_t i = 0; i < MAX_PENDING_COMMANDS; i++) {
        if (!_pending[i].active) {
            _pending[i].commandId = id;
            strncpy(_pending[i].action, action, sizeof(_pending[i].action) - 1);
            _pending[i].action[sizeof(_pending[i].action) - 1] = '\0';
            _pending[i].position 
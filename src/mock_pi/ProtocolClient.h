/**
 * @file ProtocolClient.h
 * @brief Protocol client for sending commands and parsing events
 * 
 * Handles:
 * - Command ID generation and correlation
 * - Sending formatted commands (via CommandController injection)
 * - Parsing Arduino response lines from EventQueue
 * - Retry logic for ERR busy
 * 
 * NOTE: This runs on the SAME device as the Arduino firmware.
 * Commands are injected directly into CommandController, not sent over Serial.
 * Events are captured from the EventQueue output.
 */

#ifndef MOCK_PI_PROTOCOL_CLIENT_H
#define MOCK_PI_PROTOCOL_CLIENT_H

#include <Arduino.h>
#include "Types.h"
#include "LineReader.h"

// Forward declaration for CommandController
class CommandController;

namespace MockPI {

// Forward declaration
class ProtocolClient;

/**
 * @brief Callback interface for event dispatch
 */
class EventListener {
public:
    virtual ~EventListener() {}
    virtual void onEvent(const ParsedEvent& event) = 0;
};

/**
 * @brief Pending command entry for correlation
 */
struct PendingCommand {
    uint32_t commandId;
    char action[16];
    uint8_t position;
    uint32_t sentTime;
    uint8_t retryCount;
    bool active;
    
    PendingCommand() : commandId(0), position(INVALID_POS), sentTime(0), retryCount(0), active(false) {
        action[0] = '\0';
    }
};

/**
 * @brief Protocol client for Arduino communication
 * 
 * This client injects commands directly into the CommandController
 * since both run on the same device.
 */
class ProtocolClient {
public:
    ProtocolClient();
    
    /**
     * @brief Initialize with serial stream for logging and event reading
     * @param serial Serial stream for logging output
     * @param cmdController CommandController to inject commands into
     */
    void begin(Stream* serial, CommandController* cmdController);
    
    /**
     * @brief Set event listener
     */
    void setListener(EventListener* listener);
    
    /**
     * @brief Poll for incoming data and dispatch events
     * Call frequently in loop()
     */
    void tick();
    
    // === Command sending methods ===
    
    /**
     * @brief Send SHOW command
     * @return Command ID
     */
    uint32_t sendShow(uint8_t pos);
    
    /**
     * @brief Send HIDE command
     * @return Command ID
     */
    uint32_t sendHide(uint8_t pos);
    
    /**
     * @brief Send SUCCESS command
     * @return Command ID
     */
    uint32_t sendSuccess(uint8_t pos);
    
    /**
     * @brief Send BLINK command
     * @return Command ID
     */
    uint32_t sendBlink(uint8_t pos);
    
    /**
     * @brief Send STOP_BLINK command
     * @return Command ID
     */
    uint32_t sendStopBlink(uint8_t pos);
    
    /**
     * @brief Send MISTAKE command
     * @return Command ID
     */
    uint32_t sendMistake(uint8_t pos);
    
    /**
     * @brief Send EXPECT command
     * @return Command ID
     */
    uint32_t sendExpect(uint8_t pos);
    
    /**
     * @brief Send EXPECT_RELEASE command
     * @return Command ID
     */
    uint32_t sendExpectRelease(uint8_t pos);
    
    /**
     * @brief Send SEQUENCE_COMPLETED command
     * @return Command ID
     */
    uint32_t sendSequenceCompleted();
    
    /**
     * @brief Send PING command
     * @return Command ID
     */
    uint32_t sendPing();
    
    /**
     * @brief Send INFO command
     * @return Command ID
     */
    uint32_t sendInfo();
    
    /**
     * @brief Get next command ID (peek only, doesn't increment)
     */
    uint32_t peekNextId() const;
    
    /**
     * @brief Enable/disable verbose logging
     */
    void setVerbose(bool verbose);
    
private:
    Stream* _serial;
    CommandController* _cmdController;
    LineReader _lineReader;
    EventListener* _listener;
    uint32_t _nextCommandId;
    bool _verbose;
    
    PendingCommand _pending[MAX_PENDING_COMMANDS];
    
    /**
     * @brief Generate next command ID
     */
    uint32_t nextId();
    
    /**
     * @brief Send a command with position (injects into CommandController)
     */
    uint32_t sendCommand(const char* action, uint8_t pos);
    
    /**
     * @brief Send a command without position (injects into CommandController)
     */
    uint32_t sendCommandNoPos(const char* action);
    
    /**
     * @brief Parse a line and fill ParsedEvent
     * @return true if line was successfully parsed as an Arduino event
     */
    bool parseLine(const char* line, ParsedEvent& event);
    
    /**
     * @brief Log transmitted command
     */
    void logTx(const char* cmd);
    
    /**
     * @brief Log received line
     */
    void logRx(const char* line);
    
    /**
     * @brief Find pending command by ID
     */
    PendingCommand* findPending(uint32_t id);
    
    /**
     * @brief Add pending command
     */
    void addPending(uint32_t id, const char* action, uint8_t pos);
    
    /**
     * @brief Remove pending command
     */
    void removePending(uint32_t id);
};

} // namespace MockPI

#endif // MOCK_PI_PROTOCOL_CLIENT_H

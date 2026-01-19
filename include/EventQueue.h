/**
 * @file EventQueue.h
 * @brief Outgoing event queue for serial communication
 * 
 * Provides a non-blocking queue for outgoing serial messages.
 * Events are flushed gradually to avoid blocking the main loop.
 */

#ifndef EVENT_QUEUE_H
#define EVENT_QUEUE_H

#include <Arduino.h>
#include "Config.h"

// ============================================================================
// Event Types
// ============================================================================

enum class EventType : uint8_t {
    ACK,            // Command acknowledged
    DONE,           // Long-running command completed
    ERR,            // Command error
    TOUCH_DOWN,     // Touch sensor pressed (spontaneous)
    TOUCH_UP,       // Touch sensor released (spontaneous)
    TOUCHED_DOWN,   // Expected touch detected (EXPECT_DOWN fulfilled)
    TOUCHED_UP,     // Expected release detected (EXPECT_UP fulfilled)
    SCANNED,        // I2C scan completed with list of active sensors
    RECALIBRATED,   // Sensor(s) recalibrated
    SCAN_RESULT,    // I2C device found during scan (legacy)
    SCAN_DONE,      // I2C scan completed (legacy)
    INFO            // Firmware info response
};

// ============================================================================
// Event Structure
// ============================================================================

struct Event {
    EventType type;
    char action[12];      // Action name (e.g., "SHOW", "SUCCESS")
    char position;        // Position letter ('A'-'Y') or 0 if none
    uint32_t commandId;   // Command ID or NO_COMMAND_ID
    char extra[52];       // Extra data - larger for SCANNED[A,B,C,...] (max ~50 chars for 25 sensors)
    bool valid;           // Whether this slot contains valid data
};

// ============================================================================
// EventQueue Class
// ============================================================================

class EventQueue {
public:
    EventQueue();

    /**
     * @brief Initialize the event queue
     */
    void begin();

    /**
     * @brief Flush pending events to serial (non-blocking)
     * Sends up to maxEvents per call to avoid blocking
     * @param maxEvents Maximum events to send (default: 3)
     */
    void flush(uint8_t maxEvents = 3);

    /**
     * @brief Check if the queue is full
     * @return true if queue is full
     */
    bool isFull() const;

    /**
     * @brief Check if the queue is empty
     * @return true if queue is empty
     */
    bool isEmpty() const;

    /**
     * @brief Get number of events in queue
     * @return Number of pending events
     */
    uint8_t count() const;

    // === Event emission methods ===

    /**
     * @brief Queue an ACK event
     * @param action Action name
     * @param position Position letter (0 if none)
     * @param commandId Command ID (NO_COMMAND_ID if none)
     * @return true if queued successfully
     */
    bool queueAck(const char* action, char position = 0, uint32_t commandId = NO_COMMAND_ID);

    /**
     * @brief Queue a DONE event
     * @param action Action name
     * @param position Position letter (0 if none)
     * @param commandId Command ID (NO_COMMAND_ID if none)
     * @return true if queued successfully
     */
    bool queueDone(const char* action, char position = 0, uint32_t commandId = NO_COMMAND_ID);

    /**
     * @brief Queue an ERR event
     * @param reason Error reason string
     * @param commandId Command ID (NO_COMMAND_ID if none)
     * @return true if queued successfully
     */
    bool queueError(const char* reason, uint32_t commandId = NO_COMMAND_ID);

    /**
     * @brief Queue a TOUCH_DOWN event
     * @param position Position letter
     * @return true if queued successfully
     */
    bool queueTouchDown(char position);

    /**
     * @brief Queue a TOUCH_UP event
     * @param position Position letter
     * @return true if queued successfully
     */
    bool queueTouchUp(char position);

    /**
     * @brief Queue a SCAN_RESULT event
     * @param address I2C address found
     * @return true if queued successfully
     */
    bool queueScanResult(uint8_t address);

    /**
     * @brief Queue SCAN_DONE events (both SCAN_DONE and DONE SCAN)
     * @param commandId Command ID (NO_COMMAND_ID if none)
     * @return true if queued successfully
     */
    bool queueScanDone(uint32_t commandId = NO_COMMAND_ID);

    /**
     * @brief Queue a SCANNED event (new format: SCANNED[A,B,C,...])
     * @param sensorList String containing comma-separated active sensor letters
     * @param commandId Command ID (NO_COMMAND_ID if none)
     * @return true if queued successfully
     */
    bool queueScanned(const char* sensorList, uint32_t commandId = NO_COMMAND_ID);

    /**
     * @brief Queue a TOUCHED_DOWN event
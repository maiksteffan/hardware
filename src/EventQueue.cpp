/**
 * @file EventQueue.cpp
 * @brief Implementation of outgoing event queue
 */

#include "EventQueue.h"

// ============================================================================
// Constructor
// ============================================================================

EventQueue::EventQueue()
    : m_head(0)
    , m_tail(0)
    , m_count(0)
{
}

// ============================================================================
// Public Methods
// ============================================================================

void EventQueue::begin() {
    m_head = 0;
    m_tail = 0;
    m_count = 0;
    
    // Clear all queue slots
    for (uint8_t i = 0; i < EVENT_QUEUE_SIZE; i++) {
        m_queue[i].valid = false;
    }
}

void EventQueue::flush(uint8_t maxEvents) {
    uint8_t sent = 0;
    
    while (!isEmpty() && sent < maxEvents) {
        Event& event = m_queue[m_tail];
        
        if (event.valid) {
            sendEvent(event);
            event.valid = false;
        }
        
        m_tail = (m_tail + 1) % EVENT_QUEUE_SIZE;
        m_count--;
        sent++;
    }
}

bool EventQueue::isFull() const {
    return m_count >= EVENT_QUEUE_SIZE;
}

bool EventQueue::isEmpty() const {
    return m_count == 0;
}

uint8_t EventQueue::count() const {
    return m_count;
}

bool EventQueue::queueAck(const char* action, char position, uint32_t commandId) {
    Event event;
    event.type = EventType::ACK;
    strncpy(event.action, action, sizeof(event.action) - 1);
    event.action[sizeof(event.action) - 1] = '\0';
    event.position = position;
    event.commandId = commandId;
    event.extra[0] = '\0';
    event.valid = true;
    
    return enqueue(event);
}

bool EventQueue::queueDone(const char* action, char position, uint32_t commandId) {
    Event event;
    event.type = EventType::DONE;
    strncpy(event.action, action, sizeof(event.action) - 1);
    event.action[sizeof(event.action) - 1] = '\0';
    event.position = position;
    event.commandId = commandId;
    event.extra[0] = '\0';
    event.valid = true;
    
    return enqueue(event);
}

bool EventQueue::queueError(const char* reason, uint32_t commandId) {
    Event event;
    event.type = EventType::ERR;
    event.action[0] = '\0';
    event.position = 0;
    event.commandId = commandId;
    strncpy(event.extra, reason, sizeof(event.extra) - 1);
    event.extra[sizeof(event.extra) - 1] = '\0';
    event.valid = true;
    
    return enqueue(event);
}

bool EventQueue::queueTouchDown(char position) {
    Event event;
    event.type = EventType::TOUCH_DOWN;
    event.action[0] = '\0';
    event.position = position;
    event.commandId = NO_COMMAND_ID;
    event.extra[0] = '\0';
    event.valid = true;
    
    return enqueue(event);
}

bool EventQueue::queueTouchUp(char position) {
    Event event;
    event.type = EventType::TOUCH_UP;
    event.action[0] = '\0';
    event.position = position;
    event.commandId = NO_COMMAND_ID;
    event.extra[0] = '\0';
    event.valid = true;
    
    return enqueue(event);
}

bool EventQueue::queueScanResult(uint8_t address) {
    Event event;
    event.type = EventType::SCAN_RESULT;
    event.action[0] = '\0';
    event.position = 0;
    event.commandId = NO_COMMAND_ID;
    
    // Format address as hex
    snprintf(event.extra, sizeof(event.extra), "0x%02X", address);
    event.valid = true;
    
    return enqueue(event);
}

bool EventQueue::queueScanDone(uint32_t commandId) {
    // First queue SCAN_DONE (without ID, it's a status line)
    Event event1;
    event1.type = EventType::SCAN_DONE;
    event1.action[0] = '\0';
    event1.position = 0;
    event1.commandId = NO_COMMAND_ID;
    event1.extra[0] = '\0';
    event1.valid = true;
    
    if (!enqueue(event1)) {
        return false;
    }
    
    // Then queue DONE SCAN with ID
    return queueDone("SCAN", 0, commandId);
}

bool EventQueue::queueScanned(const char* sensorList, uint32_t commandId) {
    Event event;
    event.type = EventType::SCANNED;
    event.action[0] = '\0';
    event.position = 0;
    event.commandId = commandId;
    strncpy(event.extra, sensorList, sizeof(event.extra) - 1);
    event.extra[sizeof(event.extra) - 1] = '\0';
    event.valid = true;
    
    return enqueue(event);
}

bool EventQueue::queueTouched(char position, uint32_t commandId) {
    Event event;
    event.type = EventType::TOUCHED;
    event.action[0] = '\0';
    event.position = position;
    event.commandId = commandId;
    event.extra[0] = '\0';
    event.valid = true;
    
    return enqueue(event);
}

bool EventQueue::queueTouchReleased(char position, uint32_t commandId) {
    Event event;
    event.type = EventType::TOUCH_RELEASED;
    event.action[0] = '\0';
    event.position = position;
    event.commandId = commandId;
    event.extra[0] = '\0';
    event.valid = true;
    
    return enqueue(event);
}

bool EventQueue::queueRecalibrated(char position, uint32_t commandId) {
    Event event;
    event.type = EventType::RECALIBRATED;
    event.action[0] = '\0';
    event.position = position;
    event.commandId = commandId;
    event.extra[0] = '\0';
    event.valid = true;
    
    return enqueue(event);
}

bool EventQueue::queueInfo(uint32_t commandId) {
    Event event;
    event.type = EventType::INFO;
    event.action[0] = '\0';
    event.position = 0;
    event.commandId = commandId;
    event.extra[0] = '\0';
    event.valid = true;
    
    return enqueue(event);
}

// ============================================================================
// Private Methods
// ============================================================================

bool EventQueue::enqueue(const Event& event) {
    if (isFull()) {
        return false;
    }
    
    m_queue[m_head] = event;
    m_head = (m_head + 1) % EVENT_QUEUE_SIZE;
    m_count++;
    
    return true;
}

void EventQueue::sendEvent(const Event& event) {
    // All Arduino responses are prefixed with ARDUINO>
    Serial.print("ARDUINO> ");
    
    switch (event.type) {
        case EventType::ACK:
            Serial.print("ACK ");
            Serial.print(event.action);
            if (event.position != 0) {
                Serial.print(" ");
                Serial.print(event.position);
            }
            if (event.commandId != NO_COMMAND_ID) {
                Serial.print(" #");
                Serial.print(event.commandId);
            }
            Serial.println();
            break;
            
        case EventType::DONE:
            Serial.print("DONE ");
            Serial.print(event.action);
            if (event.position != 0) {
                Serial.print(" ");
                Serial.print(event.position);
            }
            if (event.commandId != NO_COMMAND_ID) {
                Serial.print(" #");
                Serial.print(event.commandId);
            }
            Serial.println();
            break;
            
        case EventType::ERR:
            Serial.print("ERR ");
            Serial.print(event.extra);
            if (event.commandId != NO_COMMAND_ID) {
                Serial.print(" #");
                Serial.print(event.commandId);
            }
            Serial.println();
            break;
            
        case EventType::TOUCH_DOWN:
            Serial.print("TOUCH_DOWN ");
            Serial.println(event.position);
            break;
            
        case EventType::TOUCH_UP:
            Serial.print("TOUCH_UP ");
            Serial.println(event.position);
            break;
            
        case EventType::TOUCHED:
            Serial.print("TOUCHED ");
            Serial.print(event.position);
            if (event.commandId != NO_COMMAND_ID) {
                Serial.print(" #");
                Serial.print(event.commandId);
            }
            Serial.println();
            break;
            
        case EventType::TOUCH_RELEASED:
            Serial.print("TOUCH_RELEASED ");
            Serial.print(event.position);
            if (event.commandId != NO_COMMAND_ID) {
                Serial.print(" #");
                Serial.print(event.commandId);
            }
            Serial.println();
            break;
            
        case EventType::SCANNED:
            // Output format: SCANNED[A,B,C] #id
            Serial.print("SCANNED[");
            Serial.print(event.extra);
            Serial.print("]");
            if (event.commandId != NO_COMMAND_ID) {
                Serial.print(" #");
                Serial.print(event.commandId);
            }
            Serial.println();
            break;
            
        case EventType::RECALIBRATED:
            Serial.print("RECALIBRATED ");
            if (event.position == 0) {
                Serial.print("ALL");
            } else {
                Serial.print(event.position);
            }
            if (event.commandId != NO_COMMAND_ID) {
                Serial.print(" #");
                Serial.print(event.commandId);
            }
            Serial.println();
            break;
            
        case EventType::SCAN_RESULT:
            Serial.print("SCAN_RESULT addr=");
            Serial.println(event.extra);
            break;
            
        case EventType::SCAN_DONE:
            Serial.println("SCAN_DONE");
            break;
            
        case EventType::INFO:
            Serial.print("INFO version=");
            Serial.print(FIRMWARE_VERSION);
            Serial.print(" protocol=");
            Serial.print(PROTOCOL_VERSION);
            Serial.print(" positions=A-Y leds=");
            Serial.print(NUM_LEDS_STRIP1);
            Serial.print("+");
            Serial.print(NUM_LEDS_STRIP2);
            Serial.print(" sensors=");
            Serial.print(NUM_TOUCH_SENSORS);
            if (event.commandId != NO_COMMAND_ID) {
                Serial.print(" #");
                Serial.print(event.commandId);
            }
            Serial.println();
            break;
    }
}

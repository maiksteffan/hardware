/**
 * @file WaitGroups.h
 * @brief Wait groups for touch and release events
 * 
 * Implements state machines for:
 * - Single touch/release waits
 * - Pair touch/release waits with simultaneity tolerance
 */

#ifndef MOCK_PI_WAIT_GROUPS_H
#define MOCK_PI_WAIT_GROUPS_H

#include <Arduino.h>
#include "Types.h"

namespace MockPI {

// Forward declaration
class ProtocolClient;

/**
 * @brief Base wait group with common functionality
 */
class WaitGroupBase {
public:
    WaitGroupBase();
    
    /**
     * @brief Check if wait group is active
     */
    bool isActive() const;
    
    /**
     * @brief Check if wait group is complete (success or failure)
     */
    bool isComplete() const;
    
    /**
     * @brief Check if wait completed successfully
     */
    bool isSuccess() const;
    
    /**
     * @brief Check if wait failed (timeout or simultaneity failure)
     */
    bool isFailure() const;
    
    /**
     * @brief Get current state
     */
    WaitState getState() const;
    
    /**
     * @brief Reset to inactive state
     */
    virtual void reset();
    
protected:
    WaitState _state;
    uint32_t _startTime;
    uint32_t _firstEventTime;
    uint8_t _pos1;
    uint8_t _pos2;
    bool _got1;
    bool _got2;
};

/**
 * @brief Wait group for touch events (TOUCHED)
 * 
 * Handles both single and pair touch waits
 */
class TouchWaitGroup : public WaitGroupBase {
public:
    TouchWaitGroup();
    
    /**
     * @brief Start waiting for single touch
     * @param pos Position to wait for
     * @param client Protocol client to send commands
     */
    void startSingle(uint8_t pos, ProtocolClient* client);
    
    /**
     * @brief Start waiting for pair touch
     * @param pos1 First position
     * @param pos2 Second position
     * @param client Protocol client to send commands
     */
    void startPair(uint8_t pos1, uint8_t pos2, ProtocolClient* client);
    
    /**
     * @brief Handle incoming TOUCHED event
     * @param pos Position that was touched
     * @param timestamp Event timestamp
     * @return true if event was consumed by this group
     */
    bool onTouched(uint8_t pos, uint32_t timestamp);
    
    /**
     * @brief Check for timeouts and update state
     * @param client Protocol client for sending MISTAKE commands
     */
    void tick(ProtocolClient* client);
    
    /**
     * @brief Reset to inactive state
     */
    void reset() override;
    
    /**
     * @brief Get positions in this group
     */
    uint8_t getPos1() const { return _pos1; }
    uint8_t getPos2() const { return _pos2; }
    bool isSingle() const { return _pos2 == INVALID_POS; }
    bool isPair() const { return _pos2 != INVALID_POS; }
    
private:
    uint32_t _toleranceMs;
    uint32_t _timeoutMs;
    bool _mistakeSent;
};

/**
 * @brief Wait group for release events (TOUCH_RELEASED)
 * 
 * Handles both single and pair release waits
 */
class ReleaseWaitGroup : public WaitGroupBase {
public:
    ReleaseWaitGroup();
    
    /**
     * @brief Start waiting for single release
     * @param pos Position to wait for
     * @param client Protocol client to send commands (BLINK, EXPECT_RELEASE)
     */
    void startSingle(uint8_t pos, ProtocolClient* client);
    
    /**
     * @brief Start waiting for pair release
     * @param pos1 First position
     * @param pos2 Second position
     * @param client Protocol client to send commands
     */
    void startPair(uint8_t pos1, uint8_t pos2, ProtocolClient* client);
    
    /**
     * @brief Handle incoming TOUCH_RELEASED event
     * @param pos Position that was released
     * @param timestamp Event timestamp
     * @return true if event was consumed by this group
     */
    bool onReleased(uint8_t pos, uint32_t timestamp);
    
    /**
     * @brief Check for timeouts and update state
     * @param client Protocol client for sending commands
     */
    void tick(ProtocolClient* client);
    
    /**
     * @brief Finalize the release (STOP_BLINK + HIDE)
     * Called when release is complete (success or failure)
     * @param client Protocol client for sending commands
     */
    void finalize(ProtocolClient* client);
    
    /**
     * @brief Reset to inactive state
     */
    void reset() override;
    
    /**
     * @brief Get positions in this group
     */
    uint8_t getPos1() const { return _pos1; }
    uint8_t getPos2() const { return _pos2; }
    bool isSingle() const { return _pos2 == INVALID_POS; }
    bool isPair() const { return _pos2 != INVALID_POS; }
    
    /**
     * @brief Check if finalize has been called
     */
    bool isFinalized() const { return _finalized; }
    
private:
    uint32_t _toleranceMs;
    bool _mistakeSent;
    bool _finalized;
};

} // namespace MockPI

#endif // MOCK_PI_WAIT_GROUPS_H

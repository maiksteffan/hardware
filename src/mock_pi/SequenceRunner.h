/**
 * @file SequenceRunner.h
 * @brief Generic sequence runner with release queue logic
 * 
 * Release queue algorithm (based on pseudocode):
 * - For first 2 tokens (idx 0, 1): Just SHOW, EXPECT, SUCCESS - no release
 * - From idx 2 onwards: Start release WHILE processing new token
 *   - If current token is SINGLE: pop 1 position from queue, release it
 *   - If current token is PAIR: pop 2 positions from queue, release as pair
 * - When touch succeeds: push position(s) to release queue
 * - Release and touch happen concurrently
 */

#ifndef MOCK_PI_SEQUENCE_RUNNER_H
#define MOCK_PI_SEQUENCE_RUNNER_H

#include <Arduino.h>
#include "Types.h"
#include "SequenceParser.h"
#include "ProtocolClient.h"
#include "WaitGroups.h"

namespace MockPI {

/**
 * @brief Runner sub-state for detailed state machine
 */
enum class RunnerSubState : uint8_t {
    IDLE,
    
    // INIT phase
    INIT_PING_SENT,
    INIT_WAITING_PING,
    INIT_INFO_SENT,
    INIT_WAITING_INFO,
    INIT_DONE,
    
    // Running phase
    START_TOKEN,        // Begin processing current token
    WAITING_TOUCH,      // Wait for touch event(s)
    TOUCH_SUCCESS,      // Touch received successfully
    TOUCH_FAILURE,      // Touch failed (timeout/simultaneity)
    
    // Completing phase
    COMPLETING_SENT,
    COMPLETING_WAITING,
    
    // Terminal states
    HALTED,
    FINISHED
};

// Release queue size (max positions to track)
constexpr uint8_t RELEASE_QUEUE_SIZE = 8;

/**
 * @brief Generic sequence runner with concurrent wait groups
 */
class SequenceRunner : public EventListener {
public:
    SequenceRunner();
    
    /**
     * @brief Initialize runner
     * @param client Protocol client for sending commands
     */
    void begin(ProtocolClient* client);
    
    /**
     * @brief Start running a sequence
     * @param sequence Sequence string like "A,B,C,E+F"
     * @param doInit Whether to do PING/INFO at start
     * @return true if sequence was parsed successfully
     */
    bool start(const char* sequence, bool doInit = true);
    
    /**
     * @brief Update state machine (call frequently)
     */
    void tick();
    
    /**
     * @brief Get current runner state
     */
    RunnerState getState() const;
    
    /**
     * @brief Get current token index
     */
    uint8_t getCurrentIndex() const;
    
    /**
     * @brief Get total token count
     */
    uint8_t getTotalCount() const;
    
    /**
     * @brief Check if runner is finished (success or halt)
     */
    bool isFinished() const;
    
    /**
     * @brief Enable/disable verbose logging
     */
    void setVerbose(bool verbose);
    
    // EventListener interface
    void onEvent(const ParsedEvent& event) override;
    
private:
    ProtocolClient* _client;
    SequenceParser _parser;
    
    RunnerState _state;
    RunnerSubState _subState;
    uint8_t _currentIndex;
    
    // Wait groups
    TouchWaitGroup _touchGroup;      // Current token touch
    ReleaseWaitGroup _releaseGroup;  // Release (runs concurrently)
    
    // Release queue (FIFO of positions to release later)
    uint8_t _releaseQueue[RELEASE_QUEUE_SIZE];
    uint8_t _releaseQueueHead;  // Next position to read
    uint8_t _releaseQueueTail;  // Next position to write
    
    // Init tracking
    uint32_t _initPingId;
    uint32_t _initInfoId;
    
    // Completion tracking
    uint32_t _completingId;
    uint32_t _completingTime;
    
    // State timestamps
    uint32_t _stateStartTime;
    
    bool _verbose;
    
    // === Logging ===
    void log(const char* msg);
    void logStateChange(const char* from, const char* to);
    void logTokenChange(uint8_t fromIdx, uint8_t toIdx);
    
    // === State management ===
    void setState(RunnerState state, RunnerSubState subState);
    
    // === Phase handlers ===
    void tickInit();
    void tickRunning();
    void tickCompleting();
    
    // === Release queue operations ===
    void releaseQueueClear();
    void releaseQueuePush(uint8_t pos);
    uint8_t releaseQueuePop();  // Returns INVALID_POS if empty
    uint8_t releaseQueueCount() const;
    
    // === Token processing ===
    void startCurrentToken();           // SHOW + EXPECT + start release if needed
    void onTouchComplete();             // Handle touch success/failure
    void sendSuccessForCurrentToken();  // Send SUCCESS commands
    void pushToReleaseQueue();          // Add current positions to queue
    void startReleaseFromQueue();       // Pop from queue and start release
    void advanceToNextToken();          // Move to next token
    void completeSequence();            // Send SEQUENCE_COMPLETED
    
    // === Error handling ===
    void halt(const char* reason);
};

} // namespace MockPI

#endif // MOCK_PI_SEQUENCE_RUNNER_H

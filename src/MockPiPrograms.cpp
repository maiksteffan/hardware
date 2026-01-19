/**
 * @file MockPiPrograms.cpp
 * @brief Implementation of Mock Raspberry Pi programs for on-device testing
 * 
 * This class simulates the Raspberry Pi "logic side" for testing the
 * complete LED/Touch protocol without a real Pi. It runs entirely on
 * the Arduino and uses a non-blocking state machine.
 */

#include "MockPiPrograms.h"
#include "TouchController.h"
#include "CommandController.h"
#include <stdarg.h>

// ============================================================================
// Constructor
// ============================================================================

MockPiPrograms::MockPiPrograms()
    : m_touchController(nullptr)
    , m_commandController(nullptr)
    , m_program(MockPiProgram::NONE)
    , m_state(MockPiState::IDLE)
    , m_verbose(true)
    , m_stepCount(0)
    , m_currentStep(0)
    , m_stateStartTime(0)
    , m_lastActionTime(0)
    , m_commandId(1000)
    , m_previousTouched(0)
    , m_currentlyTouched(0)
    , m_stepTouchedMask(0)
    , m_firstTouchTime(0)
    , m_recordedCount(0)
    , m_lastTouchTime(0)
    , m_twoHandCount(0)
    , m_twoHandCurrent(0)
    , m_twoHandCleanupIndex(0)
    , m_waitingForAck(false)
    , m_pendingCommands(0)
{
    m_recordedSequence[0] = '\0';
    m_twoHandPositions[0] = '\0';
}

// ============================================================================
// Public Methods
// ============================================================================

void MockPiPrograms::begin() {
    m_program = MockPiProgram::NONE;
    m_state = MockPiState::IDLE;
    m_previousTouched = 0;
    m_currentlyTouched = 0;
    m_stepTouchedMask = 0;
    m_recordedCount = 0;
    m_commandId = 1000;
    log("MockPi: Initialized");
}

void MockPiPrograms::update() {
    if (m_program == MockPiProgram::NONE) {
        return;
    }
    
    // Poll touch state directly from TouchController (if available)
    pollTouchState();
    
    // Update state machine
    updateStateMachine();
}

void MockPiPrograms::pollTouchState() {
    if (!m_touchController) {
        return;
    }
    
    // Build current touch mask by polling all sensors
    uint32_t newTouched = 0;
    for (uint8_t i = 0; i < NUM_TOUCH_SENSORS; i++) {
        if (m_touchController->isTouched(i)) {
            newTouched |= (1UL << i);
        }
    }
    
    // Detect edges
    uint32_t justPressed = newTouched & ~m_previousTouched;
    uint32_t justReleased = m_previousTouched & ~newTouched;
    
    // Process touch down events
    for (uint8_t i = 0; i < NUM_TOUCH_SENSORS; i++) {
        uint32_t bit = 1UL << i;
        if (justPressed & bit) {
            onTouchDown(indexToLetter(i));
        }
        if (justReleased & bit) {
            onTouchUp(indexToLetter(i));
        }
    }
    
    m_previousTouched = newTouched;
    m_currentlyTouched = newTouched;
}

void MockPiPrograms::feedEventLine(const char* line) {
    if (!line || line[0] == '\0') {
        return;
    }
    
    // Skip whitespace
    while (*line == ' ' || *line == '\t') line++;
    
    // Check for and strip "ARDUINO> " prefix
    if (strncmp(line, "ARDUINO>", 8) == 0) {
        line += 8;
        while (*line == ' ' || *line == '\t') line++;
    }
    
    // Parse event type
    char eventType[20] = {0};
    char positionChar = 0;
    uint32_t cmdId = NO_COMMAND_ID;
    
    // Try to parse common event formats:
    // TOUCHED_DOWN A [#id]
    // TOUCHED_UP A [#id]
    // TOUCH_DOWN A
    // TOUCH_UP A
    // ACK SHOW A [#id]
    // DONE SUCCESS A [#id]
    // ERR reason [#id]
    
    int i = 0;
    while (line[i] && line[i] != ' ' && i < 19) {
        eventType[i] = line[i];
        i++;
    }
    eventType[i] = '\0';
    
    // Skip whitespace after event type
    while (line[i] == ' ') i++;
    
    // Check for ACK/DONE/ERR which have action name next
    if (strcmp(eventType, "ACK") == 0 || strcmp(eventType, "DONE") == 0) {
        // Skip action name
        while (line[i] && line[i] != ' ') i++;
        while (line[i] == ' ') i++;
    }
    
    // Check for position letter
    if (line[i] >= 'A' && line[i] <= 'Y') {
        positionChar = line[i];
        i++;
    } else if (line[i] >= 'a' && line[i] <= 'y') {
        positionChar = line[i] - 32; // To uppercase
        i++;
    }
    
    // Skip whitespace
    while (line[i] == ' ') i++;
    
    // Check for command ID (#number)
    if (line[i] == '#') {
        i++;
        cmdId = 0;
        while (line[i] >= '0' && line[i] <= '9') {
            cmdId = cmdId * 10 + (line[i] - '0');
            i++;
        }
    }
    
    // Process the event
    processEvent(eventType, positionChar, cmdId);
}

void MockPiPrograms::startSequenceSimple(const char* positions) {
    if (!positions || positions[0] == '\0') {
        log("MockPi: Error - empty sequence");
        return;
    }
    
    // Build simple steps from position string
    m_stepCount = 0;
    for (uint8_t i = 0; positions[i] != '\0' && m_stepCount < MAX_SEQUENCE_LENGTH; i++) {
        char c = positions[i];
        // Skip non-position characters
        if (c >= 'a' && c <= 'y') c -= 32;
        if (c < 'A' || c > 'Y') continue;
        
        m_steps[m_stepCount].type = StepType::SINGLE;
        m_steps[m_stepCount].positionCount = 1;
        m_steps[m_stepCount].positions[0] = c;
        m_steps[m_stepCount].touchedMask = 0;
        m_steps[m_stepCount].firstTouchTime = 0;
        m_stepCount++;
    }
    
    if (m_stepCount == 0) {
        log("MockPi: Error - no valid positions");
        return;
    }
    
    m_program = MockPiProgram::SEQUENCE_SIMPLE;
    m_currentStep = 0;
    m_stepTouchedMask = 0;
    m_previousTouched = 0;
    transitionTo(MockPiState::STEP_SHOW);
    
    logf("MockPi: Starting simple sequence with %d steps", m_stepCount);
}

void MockPiPrograms::startSequenceSimultaneous(const char* spec) {
    if (!spec || spec[0] == '\0') {
        log("MockPi: Error - empty spec");
        return;
    }
    
    if (!parseSimultaneousSpec(spec)) {
        log("MockPi: Error - failed to parse spec");
        return;
    }
    
    m_program = MockPiProgram::SEQUENCE_SIMULTANEOUS;
    m_currentStep = 0;
    m_stepTouchedMask = 0;
    m_previousTouched = 0;
    transitionTo(MockPiState::STEP_SHOW);
    
    logf("MockPi: Starting simultaneous sequence with %d steps", m_stepCount);
}

void MockPiPrograms::startRecordPlayback() {
    m_program = MockPiProgram::RECORD_PLAYBACK;
    m_recordedCount = 0;
    m_recordedSequence[0] = '\0';
    m_previousTouched = 0;
    m_currentlyTouched = 0;
    m_lastTouchTime = millis();
    transitionTo(MockPiState::RECORDING);
    
    log("MockPi: Recording mode - touch holds to record sequence");
}

void MockPiPrograms::startTwoHandSequence(const char* positions) {
    if (!positions || positions[0] == '\0') {
        log("MockPi: Error - empty positions");
        return;
    }
    
    // Parse position string
    m_twoHandCount = 0;
    for (uint8_t i = 0; positions[i] != '\0' && m_twoHandCount < MAX_SEQUENCE_LENGTH; i++) {
        char c = positions[i];
        // Skip non-position characters
        if (c >= 'a' && c <= 'y') c -= 32;
        if (c < 'A' || c > 'Y') continue;
        
        m_twoHandPositions[m_twoHandCount++] = c;
    }
    m_twoHandPositions[m_twoHandCount] = '\0';
    
    if (m_twoHandCount == 0) {
        log("MockPi: Error - no valid positions");
        return;
    }
    
    m_program = MockPiProgram::TWO_HAND_SEQUENCE;
    m_twoHandCurrent = 0;
    m_stepTouchedMask = 0;
    m_previousTouched = 0;
    transitionTo(MockPiState::TWO_HAND_SHOW);
    
    logf("MockPi: Starting two-hand sequence with %d positions", m_twoHandCount);
}

void MockPiPrograms::stop() {
    m_program = MockPiProgram::NONE;
    m_state = MockPiState::IDLE;
    m_stepTouchedMask = 0;
    m_currentlyTouched = 0;
    m_previousTouched = 0;
    log("MockPi: Stopped");
}

bool MockPiPrograms::isRunning() const {
    return m_program != MockPiProgram::NONE && m_state != MockPiState::IDLE;
}

// ============================================================================
// Private Methods - Touch Event Handlers
// ============================================================================

void MockPiPrograms::onTouchDown(char position) {
    uint32_t posBit = posToBit(position);
    m_lastTouchTime = millis();
    
    if (m_verbose) {
        logf("MockPi: Touch DOWN at %c", position);
    }
    
    // Recording mode - add to sequence
    if (m_state == MockPiState::RECORDING || m_state == MockPiState::RECORDING_IDLE_CHECK) {
        // Check if already recorded (avoid duplicates)
        bool found = false;
        for (uint8_t i = 0; i < m_recordedCount; i++) {
            if (m_recordedSequence[i] == position) {
                found = true;
                break;
            }
        }
        if (!found && m_recordedCount < MAX_SEQUENCE_LENGTH) {
            m_recordedSequence[m_recordedCount++] = position;
            m_recordedSequence[m_recordedCount] = '\0';
            logf("MockPi: Recorded position %c (total: %d)", position, m_recordedCount);
        }
        // Go back to recording state if we were in idle check
        if (m_state == MockPiState::RECORDING_IDLE_CHECK) {
            transitionTo(MockPiState::RECORDING);
        }
        return;
    }
    
    // Sequence mode - update touch mask
    if (m_state == MockPiState::STEP_WAIT_TOUCH) {
        // Check if this position is part of current step
        SequenceStep& step = currentStepData();
        for (uint8_t i = 0; i < step.positionCount; i++) {
            if (step.positions[i] == position) {
                m_stepTouchedMask |= posBit;
                
                // Track first touch time for simultaneous
                if (m_firstTouchTime == 0) {
                    m_firstTouchTime = millis();
                }
                break;
            }
        }
    }
    
    // Two-hand sequence mode - check if expected position was touched
    if (m_state == MockPiState::TWO_HAND_WAIT_TOUCH) {
        if (m_twoHandCurrent < m_twoHandCount && 
            position == m_twoHandPositions[m_twoHandCurrent]) {
            m_stepTouchedMask |= posBit;
        }
    }
}

void MockPiPrograms::onTouchUp(char position) {
    if (m_verbose) {
        logf("MockPi: Touch UP at %c", position);
    }
    
    // In recording mode, check if we should start idle detection
    if (m_state == MockPiState::RECORDING && m_currentlyTouched == 0) {
        transitionTo(MockPiState::RECORDING_IDLE_CHECK);
    }
}

// ============================================================================
// Private Methods - Command Sending
// ============================================================================

void MockPiPrograms::sendCommand(const char* cmd) {
    // Log the command for visibility in serial monitor
    Serial.print("PI> ");
    Serial.println(cmd);
    
    // If we have a CommandController reference, use injectCommand for direct execution
    if (m_commandController) {
        m_commandController->injectCommand(cmd);
    }
    // Otherwise the command was just printed - in a loopback scenario,
    // the serial could be read back, but that's not typical on Arduino
    
    m_lastActionTime = millis();
}

void MockPiPrograms::sendCommandWithPos(const char* action, char pos) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%s %c #%lu", action, pos, (unsigned long)m_commandId++);
    sendCommand(buf);
}

// ============================================================================
// Private Methods - Spec Parsing
// ============================================================================

bool MockPiPrograms::parseSimultaneousSpec(const char* spec) {
    // Format: "A,B,(C+D),(E+F)"
    // - Comma separates steps
    // - Parentheses with + indicate simultaneous touches
    // - Single letters are single touches
    
    m_stepCount = 0;
    const char* p = spec;
    
    while (*p && m_stepCount < MAX_SEQUENCE_LENGTH) {
        // Skip whitespace and commas
        while (*p == ' ' || *p == ',' || *p == '\t') p++;
        if (*p == '\0') break;
        
        SequenceStep& step = m_steps[m_stepCount];
        step.positionCount = 0;
        step.touchedMask = 0;
        step.firstTouchTime = 0;
        
        if (*p == '(') {
            // Simultaneous group
            step.type = StepType::SIMULTANEOUS;
            p++; // Skip '('
            
            while (*p && *p != ')' && step.positionCount < MAX_GROUP_SIZE) {
                // Skip + and whitespace
                while (*p == '+' || *p == ' ') p++;
                if (*p == ')' || *p == '\0') break;
                
                char c = *p;
                if (c >= 'a' && c <= 'y') c -= 32;
                if (c >= 'A' && c <= 'Y') {
                    step.positions[step.positionCount++] = c;
                }
                p++;
            }
            
            if (*p == ')') p++;
        } else {
            // Single position
            step.type = StepType::SINGLE;
            char c = *p;
            if (c >= 'a' && c <= 'y') c -= 32;
            if (c >= 'A' && c <= 'Y') {
                step.positions[0] = c;
                step.positionCount = 1;
            }
            p++;
        }
        
        if (step.positionCount > 0) {
            m_stepCount++;
        }
    }
    
    return m_stepCount > 0;
}

// ============================================================================
// Private Methods - State Machine
// ============================================================================

void MockPiPrograms::updateStateMachine() {
    uint32_t now = millis();
    uint32_t elapsed = now - m_stateStartTime;
    
    switch (m_state) {
        case MockPiState::IDLE:
            // Nothing to do
            break;
            
        case MockPiState::STEP_SHOW: {
            // Send SHOW for all positions in current step
            SequenceStep& step = currentStepData();
            for (uint8_t i = 0; i < step.positionCount; i++) {
                sendCommandWithPos("SHOW", step.positions[i]);
            }
            transitionTo(MockPiState::STEP_EXPECT_DOWN);
            break;
        }
        
        case MockPiState::STEP_EXPECT_DOWN: {
            // Wait a bit for SHOW ACKs, then send EXPECT_DOWN
            if (elapsed >= MOCK_PI_INTER_STEP_DELAY_MS) {
                SequenceStep& step = currentStepData();
                m_stepTouchedMask = 0;
                m_firstTouchTime = 0;
                for (uint8_t i = 0; i < step.positionCount; i++) {
                    sendCommandWithPos("EXPECT_DOWN", step.positions[i]);
                }
                transitionTo(MockPiState::STEP_WAIT_TOUCH);
            }
            break;
        }
        
        case MockPiState::STEP_WAIT_TOUCH: {
            // Check timeout
            if (elapsed >= MOCK_PI_STEP_TIMEOUT_MS) {
                log("MockPi: Timeout waiting for touch - retrying step");
                transitionTo(MockPiState::STEP_SHOW); // Retry step
                break;
            }
            
            // For simultaneous steps, check timing window
            SequenceStep& step = currentStepData();
            if (step.type == StepType::SIMULTANEOUS && m_firstTouchTime > 0) {
                if (now - m_firstTouchTime > MOCK_PI_SIMULTANEOUS_WINDOW_MS) {
                    if (!allStepPositionsTouched()) {
                        // Timing window expired without all touches
                        log("MockPi: Simultaneous window expired, retrying");
                        m_stepTouchedMask = 0;
                        m_firstTouchTime = 0;
                        // Send HIDE then restart
                        for (uint8_t i = 0; i < step.positionCount; i++) {
                            sendCommandWithPos("HIDE", step.positions[i]);
                        }
                        transitionTo(MockPiState::STEP_SHOW);
                        break;
                    }
                }
            }
            
            // Check if all touches received
            if (allStepPositionsTouched()) {
                transitionTo(MockPiState::STEP_SUCCESS);
            }
            break;
        }
        
        case MockPiState::STEP_SUCCESS: {
            // Wait a bit, then send SUCCESS for all positions
            if (elapsed >= MOCK_PI_INTER_STEP_DELAY_MS) {
                SequenceStep& step = currentStepData();
                for (uint8_t i = 0; i < step.positionCount; i++) {
                    sendCommandWithPos("SUCCESS", step.positions[i]);
                }
                transitionTo(MockPiState::STEP_EXPECT_UP);
            }
            break;
        }
        
        case MockPiState::STEP_EXPECT_UP: {
            // Wait a bit, then send EXPECT_UP
            if (elapsed >= MOCK_PI_INTER_STEP_DELAY_MS) {
                SequenceStep& step = currentStepData();
                for (uint8_t i = 0; i < step.positionCount; i++) {
                    sendCommandWithPos("EXPECT_UP", step.positions[i]);
                }
                transitionTo(MockPiState::STEP_WAIT_RELEASE);
            }
            break;
        }
        
        case MockPiState::STEP_WAIT_RELEASE: {
            // Check timeout (use shorter timeout for release)
            if (elapsed >= MOCK_PI_STEP_TIMEOUT_MS / 2) {
                // Timeout on release - proceed anyway
                log("MockPi: Release timeout, continuing");
                transitionTo(MockPiState::STEP_HIDE);
                break;
            }
            
            // Check if all releases received
            if (allStepPositionsReleased()) {
                transitionTo(MockPiState::STEP_HIDE);
            }
            break;
        }
        
        case MockPiState::STEP_HIDE: {
            // Wait a bit, then send HIDE
            if (elapsed >= MOCK_PI_INTER_STEP_DELAY_MS) {
                SequenceStep& step = currentStepData();
                for (uint8_t i = 0; i < step.positionCount; i++) {
                    sendCommandWithPos("HIDE", step.positions[i]);
                }
                transitionTo(MockPiState::STEP_NEXT);
            }
            break;
        }
        
        case MockPiState::STEP_NEXT: {
            // Move to next step
            if (elapsed >= MOCK_PI_INTER_STEP_DELAY_MS) {
                m_currentStep++;
                m_stepTouchedMask = 0;
                
                if (m_currentStep >= m_stepCount) {
                    transitionTo(MockPiState::SEQUENCE_COMPLETE);
                } else {
                    logf("MockPi: Step %d of %d", m_currentStep + 1, m_stepCount);
                    transitionTo(MockPiState::STEP_SHOW);
                }
            }
            break;
        }
        
        case MockPiState::SEQUENCE_COMPLETE: {
            // Send SEQUENCE_COMPLETED
            if (elapsed >= MOCK_PI_INTER_STEP_DELAY_MS) {
                char buf[32];
                snprintf(buf, sizeof(buf), "SEQUENCE_COMPLETED #%lu", (unsigned long)m_commandId++);
                sendCommand(buf);
                log("MockPi: Sequence completed!");
                
                // Check if we're in playback mode (record-playback)
                if (m_program == MockPiProgram::RECORD_PLAYBACK) {
                    // Return to recording mode
                    m_recordedCount = 0;
                    m_recordedSequence[0] = '\0';
                    transitionTo(MockPiState::RECORDING);
                    log("MockPi: Returning to recording mode");
                } else {
                    m_program = MockPiProgram::NONE;
                    transitionTo(MockPiState::IDLE);
                }
            }
            break;
        }
        
        // ================================================================
        // Two-Hand Sequence States (with BLINK)
        // Pattern for A,B,C,D:
        //   SHOW A -> touch A -> SUCCESS A
        //   SHOW B -> touch B -> SUCCESS B -> BLINK A
        //   wait release A -> STOP_BLINK A -> HIDE A -> SHOW C
        //   touch C -> SUCCESS C -> BLINK B
        //   wait release B -> STOP_BLINK B -> HIDE B -> SHOW D
        //   touch D -> SEQUENCE_COMPLETED
        // ================================================================
        
        case MockPiState::TWO_HAND_SHOW: {
            // Send SHOW for current position
            if (m_twoHandCurrent < m_twoHandCount) {
                char pos = m_twoHandPositions[m_twoHandCurrent];
                sendCommandWithPos("SHOW", pos);
                transitionTo(MockPiState::TWO_HAND_EXPECT_DOWN);
            } else {
                // No more positions, go to final cleanup
                m_twoHandCleanupIndex = (m_twoHandCount >= 1) ? (m_twoHandCount - 1) : 0;
                transitionTo(MockPiState::TWO_HAND_FINAL_CLEANUP);
            }
            break;
        }
        
        case MockPiState::TWO_HAND_EXPECT_DOWN: {
            // Wait a bit, then send EXPECT_DOWN
            if (elapsed >= MOCK_PI_INTER_STEP_DELAY_MS) {
                char pos = m_twoHandPositions[m_twoHandCurrent];
                m_stepTouchedMask = 0;
                sendCommandWithPos("EXPECT_DOWN", pos);
                transitionTo(MockPiState::TWO_HAND_WAIT_TOUCH);
            }
            break;
        }
        
        case MockPiState::TWO_HAND_WAIT_TOUCH: {
            // Check timeout
            if (elapsed >= MOCK_PI_STEP_TIMEOUT_MS) {
                log("MockPi: Timeout waiting for touch - retrying");
                transitionTo(MockPiState::TWO_HAND_SHOW);
                break;
            }
            
            // Check if current position was touched
            char pos = m_twoHandPositions[m_twoHandCurrent];
            if (m_stepTouchedMask & posToBit(pos)) {
                transitionTo(MockPiState::TWO_HAND_SUCCESS);
            }
            break;
        }
        
        case MockPiState::TWO_HAND_SUCCESS: {
            // Wait a bit, then send SUCCESS
            if (elapsed >= MOCK_PI_INTER_STEP_DELAY_MS) {
                char pos = m_twoHandPositions[m_twoHandCurrent];
                sendCommandWithPos("SUCCESS", pos);
                
                // Check if this is the last position
                if (m_twoHandCurrent >= m_twoHandCount - 1) {
                    // Last position - go to final cleanup
                    m_twoHandCleanupIndex = 0;
                    if (m_twoHandCount >= 2) {
                        m_twoHandCleanupIndex = m_twoHandCount - 2; // Second-to-last still held
                    }
                    transitionTo(MockPiState::TWO_HAND_FINAL_CLEANUP);
                }
                // After touching position N and SUCCESS, if N >= 1, we blink posit
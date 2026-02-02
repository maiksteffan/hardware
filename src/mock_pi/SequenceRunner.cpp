/**
 * @file SequenceRunner.cpp
 * @brief Implementation of sequence runner with release queue
 * 
 * Release queue algorithm:
 * - First 2 tokens (idx 0, 1): SHOW, EXPECT, SUCCESS only - no release
 * - From idx >= 2: pop from queue and release WHILE processing new token
 *   - Single token: pop 1, release as single
 *   - Pair token: pop 2, release as pair
 * - After SUCCESS: push position(s) to release queue
 */

#include "SequenceRunner.h"

namespace MockPI {

SequenceRunner::SequenceRunner()
    : _client(nullptr)
    , _state(RunnerState::IDLE)
    , _subState(RunnerSubState::IDLE)
    , _currentIndex(0)
    , _releaseQueueHead(0)
    , _releaseQueueTail(0)
    , _initPingId(0)
    , _initInfoId(0)
    , _completingId(0)
    , _completingTime(0)
    , _stateStartTime(0)
    , _verbose(true)
{
    releaseQueueClear();
}

void SequenceRunner::begin(ProtocolClient* client) {
    _client = client;
    if (_client) {
        _client->setListener(this);
    }
}

void SequenceRunner::setVerbose(bool verbose) {
    _verbose = verbose;
}

bool SequenceRunner::start(const char* sequence, bool doInit) {
    if (!_client) {
        log("ERROR: No protocol client");
        return false;
    }
    
    // Parse sequence
    if (!_parser.parse(sequence)) {
        log("ERROR: Failed to parse sequence");
        return false;
    }
    
    if (_verbose) {
        Serial.print("MOCKPI> ");
        _parser.print(Serial);
    }
    
    _currentIndex = 0;
    _stateStartTime = millis();
    releaseQueueClear();
    _touchGroup.reset();
    _releaseGroup.reset();
    
    if (doInit) {
        setState(RunnerState::INIT, RunnerSubState::INIT_PING_SENT);
        _initPingId = _client->sendPing();
    } else {
        setState(RunnerState::RUNNING, RunnerSubState::START_TOKEN);
    }
    
    return true;
}

void SequenceRunner::tick() {
    if (!_client) return;
    
    // Always tick the release group (runs in background)
    if (_releaseGroup.isActive()) {
        _releaseGroup.tick(_client);
        
        // Finalize if complete
        if (_releaseGroup.isComplete() && !_releaseGroup.isFinalized()) {
            _releaseGroup.finalize(_client);
        }
    }
    
    // State machine
    switch (_state) {
        case RunnerState::IDLE:
            break;
            
        case RunnerState::INIT:
            tickInit();
            break;
            
        case RunnerState::RUNNING:
            tickRunning();
            break;
            
        case RunnerState::COMPLETING:
            tickCompleting();
            break;
            
        case RunnerState::HALTED:
        case RunnerState::FINISHED:
            break;
    }
}

RunnerState SequenceRunner::getState() const {
    return _state;
}

uint8_t SequenceRunner::getCurrentIndex() const {
    return _currentIndex;
}

uint8_t SequenceRunner::getTotalCount() const {
    return _parser.count();
}

bool SequenceRunner::isFinished() const {
    return _state == RunnerState::FINISHED || _state == RunnerState::HALTED;
}

void SequenceRunner::setState(RunnerState state, RunnerSubState subState) {
    if (_verbose && (_state != state)) {
        logStateChange(runnerStateToStr(_state), runnerStateToStr(state));
    }
    _state = state;
    _subState = subState;
    _stateStartTime = millis();
}

void SequenceRunner::log(const char* msg) {
    if (_verbose) {
        Serial.print("MOCKPI> RUN: ");
        Serial.println(msg);
    }
}

void SequenceRunner::logStateChange(const char* from, const char* to) {
    Serial.print("MOCKPI> RUN: state ");
    Serial.print(from);
    Serial.print(" -> ");
    Serial.println(to);
}

void SequenceRunner::logTokenChange(uint8_t fromIdx, uint8_t toIdx) {
    const Token& fromToken = _parser.getToken(fromIdx);
    const Token& toToken = _parser.getToken(toIdx);
    
    Serial.print("MOCKPI> RUN: idx=");
    Serial.print(fromIdx);
    Serial.print(" token=");
    Serial.print(posToChar(fromToken.pos1));
    if (fromToken.isPair()) {
        Serial.print('+');
        Serial.print(posToChar(fromToken.pos2));
    }
    Serial.print(" -> idx=");
    Serial.print(toIdx);
    Serial.print(" token=");
    Serial.print(posToChar(toToken.pos1));
    if (toToken.isPair()) {
        Serial.print('+');
        Serial.print(posToChar(toToken.pos2));
    }
    Serial.println();
}

// ============================================================================
// Release Queue Operations
// ============================================================================

void SequenceRunner::releaseQueueClear() {
    _releaseQueueHead = 0;
    _releaseQueueTail = 0;
    for (uint8_t i = 0; i < RELEASE_QUEUE_SIZE; i++) {
        _releaseQueue[i] = INVALID_POS;
    }
}

void SequenceRunner::releaseQueuePush(uint8_t pos) {
    if (releaseQueueCount() >= RELEASE_QUEUE_SIZE) {
        log("WARNING: Release queue full!");
        return;
    }
    _releaseQueue[_releaseQueueTail] = pos;
    _releaseQueueTail = (_releaseQueueTail + 1) % RELEASE_QUEUE_SIZE;
    
    if (_verbose) {
        Serial.print("MOCKPI> RUN: Queue push ");
        Serial.print(posToChar(pos));
        Serial.print(" (count=");
        Serial.print(releaseQueueCount());
        Serial.println(")");
    }
}

uint8_t SequenceRunner::releaseQueuePop() {
    if (releaseQueueCount() == 0) {
        return INVALID_POS;
    }
    uint8_t pos = _releaseQueue[_releaseQueueHead];
    _releaseQueue[_releaseQueueHead] = INVALID_POS;
    _releaseQueueHead = (_releaseQueueHead + 1) % RELEASE_QUEUE_SIZE;
    
    if (_verbose) {
        Serial.print("MOCKPI> RUN: Queue pop ");
        Serial.print(posToChar(pos));
        Serial.print(" (count=");
        Serial.print(releaseQueueCount());
        Serial.println(")");
    }
    return pos;
}

uint8_t SequenceRunner::releaseQueueCount() const {
    if (_releaseQueueTail >= _releaseQueueHead) {
        return _releaseQueueTail - _releaseQueueHead;
    }
    return RELEASE_QUEUE_SIZE - _releaseQueueHead + _releaseQueueTail;
}

// ============================================================================
// Event Handler
// ============================================================================

void SequenceRunner::onEvent(const ParsedEvent& event) {
    switch (event.type) {
        case EventType::ACK:
            if (_subState == RunnerSubState::INIT_WAITING_PING && 
                event.commandId == _initPingId) {
                _subState = RunnerSubState::INIT_INFO_SENT;
                _initInfoId = _client->sendInfo();
            }
            break;
            
        case EventType::INFO:
            if (_subState == RunnerSubState::INIT_INFO_SENT ||
                _subState == RunnerSubState::INIT_WAITING_INFO) {
                _subState = RunnerSubState::INIT_DONE;
            }
            break;
            
        case EventType::DONE:
            if (_state == RunnerState::COMPLETING) {
                if (strcmp(event.action, "SEQUENCE_COMPLETED") == 0) {
                    setState(RunnerState::FINISHED, RunnerSubState::FINISHED);
                    log("Sequence completed successfully!");
                }
            }
            break;
            
        case EventType::TOUCHED:
            if (_touchGroup.isActive()) {
                _touchGroup.onTouched(event.position, event.timestamp);
            }
            break;
            
        case EventType::TOUCH_RELEASED:
            if (_releaseGroup.isActive()) {
                _releaseGroup.onReleased(event.position, event.timestamp);
            }
            break;
            
        case EventType::TOUCH_DOWN:
        case EventType::TOUCH_UP:
            if (_verbose) {
                Serial.print("MOCKPI> RUN: Spontaneous ");
                Serial.print(eventTypeToStr(event.type));
                Serial.print(" ");
                Serial.println(posToChar(event.position));
            }
            break;
            
        case EventType::ERR:
            {
                ErrorReason reason = parseErrorReason(event.reason);
                if (reason == ErrorReason::BUSY) {
                    if (_verbose) {
                        log("Received ERR busy - would retry");
                    }
                } else if (reason != ErrorReason::NONE) {
                    char msg[64];
                    snprintf(msg, sizeof(msg), "ERROR: %s", event.reason);
                    halt(msg);
                }
            }
            break;
            
        default:
            break;
    }
}

// ============================================================================
// Init Phase
// ============================================================================

void SequenceRunner::tickInit() {
    switch (_subState) {
        case RunnerSubState::INIT_PING_SENT:
            _subState = RunnerSubState::INIT_WAITING_PING;
            break;
            
        case RunnerSubState::INIT_WAITING_PING:
            if (millis() - _stateStartTime > 2000) {
                log("PING timeout, continuing anyway");
                _subState = RunnerSubState::INIT_INFO_SENT;
                _initInfoId = _client->sendInfo();
            }
            break;
            
        case RunnerSubState::INIT_INFO_SENT:
            _subState = RunnerSubState::INIT_WAITING_INFO;
            break;
            
        case RunnerSubState::INIT_WAITING_INFO:
            if (millis() - _stateStartTime > 2000) {
                log("INFO timeout, continuing anyway");
                _subState = RunnerSubState::INIT_DONE;
            }
            break;
            
        case RunnerSubState::INIT_DONE:
            log("Init complete, starting sequence");
            setState(RunnerState::RUNNING, RunnerSubState::START_TOKEN);
            break;
            
        default:
            break;
    }
}

// ============================================================================
// Running Phase
// ============================================================================

void SequenceRunner::tickRunning() {
    // Tick touch group
    if (_touchGroup.isActive()) {
        _touchGroup.tick(_client);
    }
    
    switch (_subState) {
        case RunnerSubState::START_TOKEN:
            if (_currentIndex >= _parser.count()) {
                completeSequence();
                return;
            }
            startCurrentToken();
            _subState = RunnerSubState::WAITING_TOUCH;
            break;
            
        case RunnerSubState::WAITING_TOUCH:
            if (_touchGroup.isComplete()) {
                onTouchComplete();
            }
            break;
            
        case RunnerSubState::TOUCH_SUCCESS:
            // Send SUCCESS, push to queue, advance
            sendSuccessForCurrentToken();
            pushToReleaseQueue();
            advanceToNextToken();
            break;
            
        case RunnerSubState::TOUCH_FAILURE:
            // On failure, still push to queue and advance
            pushToReleaseQueue();
            advanceToNextToken();
            break;
            
        default:
            break;
    }
}

void SequenceRunner::startCurrentToken() {
    const Token& token = _parser.getToken(_currentIndex);
    
    if (_verbose) {
        Serial.print("MOCKPI> RUN: Starting token ");
        Serial.print(_currentIndex);
        Serial.print(": ");
        Serial.print(posToChar(token.pos1));
        if (token.isPair()) {
            Serial.print('+');
            Serial.print(posToChar(token.pos2));
        }
        Serial.println();
    }
    
    // From idx >= 2: start release from queue (concurrent with touch)
    if (_currentIndex >= 2) {
        startReleaseFromQueue();
    }
    
    // Start touch wait: SHOW + EXPECT
    if (token.isSingle()) {
        _touchGroup.startSingle(token.pos1, _client);
    } else {
        _touchGroup.startPair(token.pos1, token.pos2, _client);
    }
}

void SequenceRunner::startReleaseFromQueue() {
    // Finalize any previous release first
    if (_releaseGroup.isActive() || (_releaseGroup.isComplete() && !_releaseGroup.isFinalized())) {
        _releaseGroup.finalize(_client);
    }
    _releaseGroup.reset();
    
    const Token& currentToken = _parser.getToken(_currentIndex);
    
    if (currentToken.isSingle()) {
        // Pop 1 position from queue
        uint8_t pos = releaseQueuePop();
        if (pos != INVALID_POS) {
            if (_verbose) {
                Serial.print("MOCKPI> RUN: Starting single release for ");
                Serial.println(posToChar(pos));
            }
            _releaseGroup.startSingle(pos, _client);
        }
    } else {
        // Pop 2 positions from queue for pair release
        uint8_t pos1 = releaseQueuePop();
        uint8_t pos2 = releaseQueuePop();
        if (pos1 != INVALID_POS && pos2 != INVALID_POS) {
            if (_verbose) {
                Serial.print("MOCKPI> RUN: Starting pair release for ");
                Serial.print(posToChar(pos1));
                Serial.print('+');
                Serial.println(posToChar(pos2));
            }
            _releaseGroup.startPair(pos1, pos2, _client);
        } else if (pos1 != INVALID_POS) {
            // Only got one - release as single
            if (_verbose) {
                Serial.print("MOCKPI> RUN: Starting single release (only one in queue) for ");
                Serial.println(posToChar(pos1));
            }
            _releaseGroup.startSingle(pos1, _client);
        }
    }
}

void SequenceRunner::onTouchComplete() {
    if (_touchGroup.isSuccess()) {
        _subState = RunnerSubState::TOUCH_SUCCESS;
    } else {
        if (_verbose) {
            log("Touch failed (timeout or simultaneity)");
        }
        _subState = RunnerSubState::TOUCH_FAILURE;
    }
}

void SequenceRunner::sendSuccessForCurrentToken() {
    const Token& token = _parser.getToken(_currentIndex);
    
    _client->sendSuccess(token.pos1);
    if (token.isPair()) {
        _client->sendSuccess(token.pos2);
    }
}

void SequenceRunner::pushToReleaseQueue() {
    const Token& token = _parser.getToken(_currentIndex);
    
    // Push position(s) to release queue
    releaseQueuePush(token.pos1);
    if (token.isPair()) {
        releaseQueuePush(token.pos2);
    }
}

void SequenceRunner::advanceToNextToken() {
    uint8_t prevIdx = _currentIndex;
    _currentIndex++;
    
    if (_currentIndex < _parser.count() && _verbose) {
        logTokenChange(prevIdx, _currentIndex);
    }
    
    // Reset touch group
    _touchGroup.reset();
    
    // Move to START_TOKEN to process next
    _subState = RunnerSubState::START_TOKEN;
}

// ============================================================================
// Completing Phase
// ===================================================
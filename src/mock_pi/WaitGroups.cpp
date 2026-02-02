/**
 * @file WaitGroups.cpp
 * @brief Implementation of wait groups
 */

#include "WaitGroups.h"
#include "ProtocolClient.h"

namespace MockPI {

// ============================================================================
// WaitGroupBase
// ============================================================================

WaitGroupBase::WaitGroupBase()
    : _state(WaitState::INACTIVE)
    , _startTime(0)
    , _firstEventTime(0)
    , _pos1(INVALID_POS)
    , _pos2(INVALID_POS)
    , _got1(false)
    , _got2(false)
{
}

bool WaitGroupBase::isActive() const {
    return _state != WaitState::INACTIVE && 
           _state != WaitState::SUCCESS && 
           _state != WaitState::FAILURE &&
           _state != WaitState::TIMEOUT;
}

bool WaitGroupBase::isComplete() const {
    return _state == WaitState::SUCCESS || 
           _state == WaitState::FAILURE ||
           _state == WaitState::TIMEOUT;
}

bool WaitGroupBase::isSuccess() const {
    return _state == WaitState::SUCCESS;
}

bool WaitGroupBase::isFailure() const {
    return _state == WaitState::FAILURE || _state == WaitState::TIMEOUT;
}

WaitState WaitGroupBase::getState() const {
    return _state;
}

void WaitGroupBase::reset() {
    _state = WaitState::INACTIVE;
    _startTime = 0;
    _firstEventTime = 0;
    _pos1 = INVALID_POS;
    _pos2 = INVALID_POS;
    _got1 = false;
    _got2 = false;
}

// ============================================================================
// TouchWaitGroup
// ============================================================================

TouchWaitGroup::TouchWaitGroup()
    : WaitGroupBase()
    , _toleranceMs(SIMUL_TOLERANCE_MS)
    , _timeoutMs(SINGLE_TIMEOUT_MS)
    , _mistakeSent(false)
{
}

void TouchWaitGroup::reset() {
    WaitGroupBase::reset();
    _mistakeSent = false;
}

void TouchWaitGroup::startSingle(uint8_t pos, ProtocolClient* client) {
    reset();
    _pos1 = pos;
    _pos2 = INVALID_POS;
    _startTime = millis();
    _state = WaitState::WAITING_FIRST;
    
    // Send SHOW and EXPECT
    if (client) {
        client->sendShow(pos);
        client->sendExpect(pos);
    }
}

void TouchWaitGroup::startPair(uint8_t pos1, uint8_t pos2, ProtocolClient* client) {
    reset();
    _pos1 = pos1;
    _pos2 = pos2;
    _startTime = millis();
    _state = WaitState::WAITING_FIRST;
    
    // Send SHOW and EXPECT for both
    if (client) {
        client->sendShow(pos1);
        client->sendShow(pos2);
        client->sendExpect(pos1);
        client->sendExpect(pos2);
    }
}

bool TouchWaitGroup::onTouched(uint8_t pos, uint32_t timestamp) {
    if (_state == WaitState::INACTIVE || isComplete()) {
        return false;
    }
    
    // Check if this position is one we're waiting for
    bool isPos1 = (pos == _pos1);
    bool isPos2 = (_pos2 != INVALID_POS && pos == _pos2);
    
    if (!isPos1 && !isPos2) {
        return false;  // Not for us
    }
    
    if (isSingle()) {
        // Single: any touch on our position is success
        if (isPos1) {
            _got1 = true;
            _state = WaitState::SUCCESS;
            return true;
        }
    } else {
        // Pair: need both within tolerance
        if (_state == WaitState::WAITING_FIRST) {
            // First touch arrived
            if (isPos1) _got1 = true;
            if (isPos2) _got2 = true;
            _firstEventTime = timestamp;
            _state = WaitState::WAITING_SECOND;
            return true;
        }
        else if (_state == WaitState::WAITING_SECOND) {
            // Second touch arrived
            if (isPos1 && !_got1) _got1 = true;
            if (isPos2 && !_got2) _got2 = true;
            
            if (_got1 && _got2) {
                // Both touched - check timing
                uint32_t delta = timestamp - _firstEventTime;
                if (delta <= _toleranceMs) {
                    _state = WaitState::SUCCESS;
                } else {
                    _state = WaitState::FAILURE;
                }
            }
            return true;
        }
    }
    
    return false;
}

void TouchWaitGroup::tick(ProtocolClient* client) {
    if (_state == WaitState::INACTIVE || isComplete()) {
        return;
    }
    
    uint32_t now = millis();
    
    if (isSingle()) {
        // Single timeout
        if (now - _startTime > _timeoutMs) {
            _state = WaitState::TIMEOUT;
        }
    } else {
        // Pair: check simultaneity window
        if (_state == WaitState::WAITING_SECOND) {
            uint32_t delta = now - _firstEventTime;
            if (delta > _toleranceMs) {
                // Window expired - failure
                _state = WaitState::FAILURE;
                
                // Send MISTAKE for both if not sent
                if (!_mistakeSent && client) {
                    client->sendMistake(_pos1);
                    client->sendMistake(_pos2);
                    _mistakeSent = true;
                }
            }
        }
        else if (_state == WaitState::WAITING_FIRST) {
            // Overall timeout for pair (both must arrive within timeout)
            if (now - _startTime > _timeoutMs) {
                _state = WaitState::TIMEOUT;
            }
        }
    }
}

// ============================================================================
// ReleaseWaitGroup
// ============================================================================

ReleaseWaitGroup::ReleaseWaitGroup()
    : WaitGroupBase()
    , _toleranceMs(SIMUL_TOLERANCE_MS)
    , _mistakeSent(false)
    , _finalized(false)
{
}

void ReleaseWaitGroup::reset() {
    WaitGroupBase::reset();
    _mistakeSent = false;
    _finalized = false;
}

void ReleaseWaitGroup::startSingle(uint8_t pos, ProtocolClient* client) {
    reset();
    _pos1 = pos;
    _pos2 = INVALID_POS;
    _startTime = millis();
    _state = WaitState::WAITING_FIRST;
    
    // Send BLINK and EXPECT_RELEASE
    if (client) {
        client->sendBlink(pos);
        client->sendExpectRelease(pos);
    }
}

void ReleaseWaitGroup::startPair(uint8_t pos1, uint8_t pos2, ProtocolClient* client) {
    reset();
    _pos1 = pos1;
    _pos2 = pos2;
    _startTime = millis();
    _state = WaitState::WAITING_FIRST;
    
    // Send BLINK and EXPECT_RELEASE for both
    if (client) {
        client->sendBlink(pos1);
        client->sendBlink(pos2);
        client->sendExpectRelease(pos1);
        client->sendExpectRelease(pos2);
    }
}

bool ReleaseWaitGroup::onReleased(uint8_t pos, uint32_t timestamp) {
    if (_state == WaitState::INACTIVE || isComplete()) {
        return false;
    }
    
    // Check if this position is one we're waiting for
    bool isPos1 = (pos == _pos1);
    bool isPos2 = (_pos2 != INVALID_POS && pos == _pos2);
    
    if (!isPos1 && !isPos2) {
        return false;  // Not for us
    }
    
    if (isSingle()) {
        // Single: any release on our position is success
        if (isPos1) {
            _got1 = true;
            _state = WaitState::SUCCESS;
            return true;
        }
    } else {
        // Pair: need both within tolerance
        if (_state == WaitState::WAITING_FIRST) {
            // First release arrived
            if (isPos1) _got1 = true;
            if (isPos2) _got2 = true;
            _firstEventTime = timestamp;
            _state = WaitState::WAITING_SECOND;
            return true;
        }
        else if (_state == WaitState::WAITING_SECOND) {
            // Second release arrived
            if (isPos1 && !_got1) _got1 = true;
            if (isPos2 && !_got2) _got2 = true;
            
            if (_got1 && _got2) {
                // Both released - check timing
                uint32_t delta = timestamp - _firstEventTime;
                if (delta <= _toleranceMs) {
                    _state = WaitState::SUCCESS;
                } else {
                    _state = WaitState::FAILURE;
                }
            }
            return true;
        }
    }
    
    return false;
}

void ReleaseWaitGroup::tick(ProtocolClient* client) {
    if (_state == WaitState::INACTIVE || isComplete()) {
        return;
    }
    
    uint32_t now = millis();
    
    if (isPair() && _state == WaitState::WAITING_SECOND) {
        uint32_t delta = now - _firstEventTime;
        if (delta > _toleranceMs) {
            // Window expired - failure
            _state = WaitState::FAILURE;
            
            // Send MISTAKE for both if not sent
            if (!_mistakeSent && client) {
                client->sendMistake(_pos1);
                client->sendMistake(_pos2);
                _mistakeSent = true;
            }
        }
    }
    // Note: We don't timeout releases (they run in background)
    // The caller should finalize after success/failure
}

void ReleaseWaitGroup::finalize(ProtocolClient* client) {
    if (_finalized) return;
    _finalized = true;
    
    if (client) {
        // STOP_BLINK and HIDE for all positions
        client->sendStopBlink(_pos1);
        client->sendHide(_pos1);
        
        if (_pos2 != INVALID_POS) {
            client->sendStopBlink(_pos2);
            client->sendHide(_pos2);
        }
    }
}

} // namespace MockPI

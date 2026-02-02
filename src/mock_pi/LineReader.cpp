/**
 * @file LineReader.cpp
 * @brief Implementation of non-blocking serial line reader
 */

#include "LineReader.h"

namespace MockPI {

LineReader::LineReader()
    : _serial(nullptr)
    , _bufferIndex(0)
    , _lineReady(false)
{
    _buffer[0] = '\0';
}

void LineReader::begin(Stream* serial) {
    _serial = serial;
    _bufferIndex = 0;
    _lineReady = false;
    _buffer[0] = '\0';
}

void LineReader::poll() {
    if (!_serial || _lineReady) return;
    
    while (_serial->available() > 0) {
        char c = _serial->read();
        
        // Handle line terminator
        if (c == '\n' || c == '\r') {
            if (_bufferIndex > 0) {
                _buffer[_bufferIndex] = '\0';
                _lineReady = true;
                return;
            }
            // Skip empty lines (or CR in CR+LF)
            continue;
        }
        
        // Add character to buffer if space available
        if (_bufferIndex < MAX_LINE_LENGTH - 1) {
            _buffer[_bufferIndex++] = c;
        }
        // If buffer overflow, truncate but continue until newline
    }
}

bool LineReader::hasLine() const {
    return _lineReady;
}

const char* LineReader::getLine() const {
    return _buffer;
}

void LineReader::consumeLine() {
    _bufferIndex = 0;
    _lineReady = false;
    _buffer[0] = '\0';
}

} // namespace MockPI

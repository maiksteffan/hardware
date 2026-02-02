/**
 * @file LineReader.h
 * @brief Non-blocking serial line reader
 * 
 * Accumulates incoming serial data into complete lines terminated by '\n'.
 */

#ifndef MOCK_PI_LINE_READER_H
#define MOCK_PI_LINE_READER_H

#include <Arduino.h>
#include "Types.h"

namespace MockPI {

class LineReader {
public:
    LineReader();
    
    /**
     * @brief Initialize with serial stream
     */
    void begin(Stream* serial);
    
    /**
     * @brief Poll for incoming data (non-blocking)
     * Call this frequently in loop()
     */
    void poll();
    
    /**
     * @brief Check if a complete line is available
     */
    bool hasLine() const;
    
    /**
     * @brief Get the complete line (without newline)
     * Only valid when hasLine() returns true
     */
    const char* getLine() const;
    
    /**
     * @brief Consume the current line (clear buffer for next)
     * Call after processing the line
     */
    void consumeLine();
    
private:
    Stream* _serial;
    char _buffer[MAX_LINE_LENGTH];
    uint8_t _bufferIndex;
    bool _lineReady;
};

} // namespace MockPI

#endif // MOCK_PI_LINE_READER_H

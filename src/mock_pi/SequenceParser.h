/**
 * @file SequenceParser.h
 * @brief Parser for sequence strings like "A,B,C,D,E+F,G+H,I,J,K"
 * 
 * Parses comma-separated tokens where each token is either:
 * - Single position: "A"
 * - Pair: "E+F"
 */

#ifndef MOCK_PI_SEQUENCE_PARSER_H
#define MOCK_PI_SEQUENCE_PARSER_H

#include <Arduino.h>
#include "Types.h"

namespace MockPI {

class SequenceParser {
public:
    SequenceParser();
    
    /**
     * @brief Parse a sequence string
     * @param sequence String like "A,B,C,E+F,G+H"
     * @return true if parsing succeeded
     */
    bool parse(const char* sequence);
    
    /**
     * @brief Get number of tokens parsed
     */
    uint8_t count() const;
    
    /**
     * @brief Get token at index
     * @param index Token index (0-based)
     * @return Token at index (invalid token if out of range)
     */
    const Token& getToken(uint8_t index) const;
    
    /**
     * @brief Clear parsed tokens
     */
    void clear();
    
    /**
     * @brief Print parsed sequence (for debugging)
     */
    void print(Stream& out) const;
    
private:
    Token _tokens[MAX_TOKENS];
    uint8_t _count;
    Token _invalidToken;
    
    /**
     * @brief Parse a single token string (e.g., "A" or "E+F")
     */
    bool parseToken(const char* str, uint8_t len, Token& out);
};

} // namespace MockPI

#endif // MOCK_PI_SEQUENCE_PARSER_H

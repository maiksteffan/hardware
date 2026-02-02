/**
 * @file SequenceParser.cpp
 * @brief Implementation of sequence parser
 */

#include "SequenceParser.h"

namespace MockPI {

SequenceParser::SequenceParser() : _count(0) {
    clear();
}

void SequenceParser::clear() {
    _count = 0;
    for (uint8_t i = 0; i < MAX_TOKENS; i++) {
        _tokens[i] = Token();
    }
}

bool SequenceParser::parse(const char* sequence) {
    clear();
    
    if (!sequence || *sequence == '\0') {
        return false;
    }
    
    const char* p = sequence;
    const char* tokenStart = p;
    
    while (*p != '\0' && _count < MAX_TOKENS) {
        if (*p == ',') {
            // End of token
            uint8_t len = p - tokenStart;
            if (len > 0) {
                Token token;
                if (!parseToken(tokenStart, len, token)) {
                    return false;
                }
                _tokens[_count++] = token;
            }
            tokenStart = p + 1;
        }
        p++;
    }
    
    // Handle last token (no trailing comma)
    if (tokenStart < p && _count < MAX_TOKENS) {
        uint8_t len = p - tokenStart;
        Token token;
        if (!parseToken(tokenStart, len, token)) {
            return false;
        }
        _tokens[_count++] = token;
    }
    
    return _count > 0;
}

bool SequenceParser::parseToken(const char* str, uint8_t len, Token& out) {
    if (len == 0) return false;
    
    // Check for pair format "X+Y"
    const char* plus = nullptr;
    for (uint8_t i = 0; i < len; i++) {
        if (str[i] == '+') {
            plus = str + i;
            break;
        }
    }
    
    if (plus) {
        // Pair token
        if (plus == str || plus == str + len - 1) {
            // + at start or end is invalid
            return false;
        }
        
        // Parse first position
        char c1 = str[0];
        if (!isValidPosChar(c1)) return false;
        
        // Parse second position
        char c2 = *(plus + 1);
        if (!isValidPosChar(c2)) return false;
        
        out = Token(charToPos(c1), charToPos(c2));
    } else {
        // Single token
        if (len != 1) return false;  // Single must be exactly one char
        
        char c = str[0];
        if (!isValidPosChar(c)) return false;
        
        out = Token(charToPos(c));
    }
    
    return out.isValid();
}

uint8_t SequenceParser::count() const {
    return _count;
}

const Token& SequenceParser::getToken(uint8_t index) const {
    if (index >= _count) {
        return _invalidToken;
    }
    return _tokens[index];
}

void SequenceParser::print(Stream& out) const {
    out.print("Sequence[");
    out.print(_count);
    out.print("]: ");
    
    for (uint8_t i = 0; i < _count; i++) {
        if (i > 0) out.print(", ");
        
        const Token& t = _tokens[i];
        out.print(posToChar(t.pos1));
        if (t.isPair()) {
            out.print('+');
            out.print(posToChar(t.pos2));
        }
    }
    out.println();
}

} // namespace MockPI

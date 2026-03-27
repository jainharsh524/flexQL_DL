#pragma once

#include <string>

enum class TokenType {
    KEYWORD,
    IDENTIFIER,
    NUMBER,
    STRING,
    SYMBOL,
    END
};

struct Token {
    TokenType type;
    std::string value;
};
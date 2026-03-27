#pragma once

#include "token.h"
#include <vector>
#include <string>

class Tokenizer {
private:
    std::string input;
    size_t pos;

public:
    Tokenizer(const std::string& input);

    std::vector<Token> tokenize();

private:
    char peek();
    char advance();
    void skip_whitespace();

    Token parse_identifier();
    Token parse_number();
    Token parse_string();
};
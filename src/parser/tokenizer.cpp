#include "parser/tokenizer.h"
#include <cctype>

Tokenizer::Tokenizer(const std::string& input)
    : input(input), pos(0) {}

char Tokenizer::peek() {
    if (pos >= input.size()) return '\0';
    return input[pos];
}

char Tokenizer::advance() {
    return input[pos++];
}

void Tokenizer::skip_whitespace() {
    while (isspace(peek())) advance();
}

Token Tokenizer::parse_identifier() {
    std::string val;
    while (isalnum(peek()) || peek() == '_') {
        val += advance();
    }
    return {TokenType::IDENTIFIER, val};
}

Token Tokenizer::parse_number() {
    std::string val;
    while (isdigit(peek())) {
        val += advance();
    }
    return {TokenType::NUMBER, val};
}

Token Tokenizer::parse_string() {
    advance(); // skip '
    std::string val;

    while (peek() != '\'' && peek() != '\0') {
        val += advance();
    }

    advance(); // closing '

    return {TokenType::STRING, val};
}

std::vector<Token> Tokenizer::tokenize() {
    std::vector<Token> tokens;

    while (pos < input.size()) {
        skip_whitespace();

        char c = peek();

        if (isalpha(c)) {
            tokens.push_back(parse_identifier());
        }
        else if (isdigit(c)) {
            tokens.push_back(parse_number());
        }
        else if (c == '\'') {
            tokens.push_back(parse_string());
        }
        else if (c == ',' || c == '(' || c == ')' || c == '=' || c == '*') {
            tokens.push_back({TokenType::SYMBOL, std::string(1, advance())});
        }
        else {
            advance(); // skip unknown
        }
    }

    tokens.push_back({TokenType::END, ""});
    return tokens;
}
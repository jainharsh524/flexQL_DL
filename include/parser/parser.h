#pragma once
#include "ast.h"
#include <string>

class Parser {
public:
    Statement parse(const std::string& sql);
};
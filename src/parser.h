#pragma once
#include <vector>
#include "lexer.h"

class Parser {
public:
    explicit Parser(const std::vector<Token>& tokens);

    bool parse();

private:
    const std::vector<Token>& m_tokens;
    int m_pos;
    Token peek();
    Token next();
    bool match(TokenType type);

};
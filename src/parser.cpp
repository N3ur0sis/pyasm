#include "parser.h"

Parser::Parser(const std::vector<Token>& tokens) : m_tokens(tokens), m_pos(0) {}

bool Parser::parse() {
    m_pos = 0;
    return file() && match(TokenType::ENDOFFILE);
}

Token Parser::peek() {
    if (m_pos < m_tokens.size()) return m_tokens[m_pos];
    return {TokenType::ENDOFFILE, ""};
}

Token Parser::next() {
    if (m_pos < m_tokens.size()) return m_tokens[m_pos++];
    return {TokenType::ENDOFFILE, ""};
}

bool Parser::match(TokenType type) {
    if (peek().type == type) {
        next();
        return true;
    }
    std::cout << "Failed to match: " << Lexer::tokenTypeToString(type) << " at position " << m_pos << std::endl;
    return false;
}


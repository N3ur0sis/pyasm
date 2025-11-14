#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include <stack>
#include <iostream>
#include "errorManager.h"


/* Enumeration for token types */
enum class TokenType {
    IDF, INTEGER, STRING, NEWLINE, ENDOFFILE,
    // Keywords
    KW_AND, KW_DEF, KW_ELSE, KW_FOR, KW_IF, KW_TRUE, KW_FALSE,
    KW_IN, KW_NOT, KW_OR, KW_PRINT, KW_RETURN, KW_NONE, KW_WHILE,
    // Operators
    OP_PLUS, OP_MINUS, OP_MUL, OP_MOD, OP_EQ,OP_EQ_EQ, OP_NEQ,
    OP_LE, OP_GE, OP_DIV,OP_LE_EQ,OP_GE_EQ,
    // Brackets and symbols
    CAR_LPAREN, CAR_RPAREN, CAR_LBRACKET, CAR_RBRACKET,
    CAR_COMMA, CAR_COLON,
    // Scope
    BEGIN, END
};

/* Structure to represent a token */
struct Token {
    TokenType type;
    std::string value;
    int line = 0;
};

/* Lexer class declaration */
class Lexer {
public:
    explicit Lexer(std::string src, ErrorManager& errorManager);

    std::vector<Token> tokenize();
    void displayTokens(const std::vector<Token>& tokens);
    static std::string tokenTypeToString(TokenType type);


private:
    std::string m_src;
    ErrorManager& m_errorManager;
    int m_pos = -1;
    int m_line = 1;
    std::stack<int> m_scope;

    std::unordered_map<std::string, TokenType> m_keywords;
    std::unordered_map<std::string, TokenType> m_ope_simple;
    std::unordered_map<std::string, TokenType> m_ope_double;
    std::unordered_map<std::string, TokenType> m_brackets;

    // Helper functions
    char lookahead(int ahead = 1) const;
    char progress();
    void reportError(const std::string& message, int line) const;

    // Token-specific handling functions
    void handleIdentifierOrKeyword(std::vector<Token>& tokens, std::string& buffer);
    void handleInteger(std::vector<Token>& tokens, std::string& buffer);
    void handleSimpleOperator(std::vector<Token>& tokens, std::string& buffer);
    void handleDoubleOperator(std::vector<Token>& tokens, std::string& buffer);
    void handleNotEqual(std::vector<Token>& tokens, std::string& buffer);
    void handleDivision(std::vector<Token>& tokens, std::string& buffer);
    void handleBracket(std::vector<Token>& tokens, std::string& buffer);
    void handleNewline(std::vector<Token>& tokens);
    void handleString(std::vector<Token>& tokens, std::string& buffer);
    void skipComment();
    void endOfFile(std::vector<Token>& tokens);
    void manageIndentation(std::vector<Token>& tokens, int n);
    void handleEscapeCharacter(std::string& buffer);

    // Utility functions
    bool isDoubleOperatorStart(char ch) const;
    
};
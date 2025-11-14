//
// Created by Aymmeric Robert on 22/10/2024.
//

#include "lexer.h"
#include <cctype>
#include <cstdlib>

/* Constructor for Lexer with keyword, operator, and bracket initialization */
Lexer::Lexer(std::string src, ErrorManager& errorManager) 
    : m_src(std::move(src)), m_errorManager(errorManager) {
    m_keywords = {
        {"and", TokenType::KW_AND}, {"def", TokenType::KW_DEF},
        {"else", TokenType::KW_ELSE}, {"for", TokenType::KW_FOR},
        {"if", TokenType::KW_IF}, {"True", TokenType::KW_TRUE},
        {"False", TokenType::KW_FALSE}, {"in", TokenType::KW_IN},
        {"not", TokenType::KW_NOT}, {"or", TokenType::KW_OR},
        {"print", TokenType::KW_PRINT}, {"return", TokenType::KW_RETURN},
        {"None", TokenType::KW_NONE}, {"while", TokenType::KW_WHILE}
    };

    m_ope_simple = {
        {"+", TokenType::OP_PLUS}, {"*", TokenType::OP_MUL},
        {"%", TokenType::OP_MOD}, {"-", TokenType::OP_MINUS},
        {"<", TokenType::OP_LE}, {">", TokenType::OP_GE},
        {"=", TokenType::OP_EQ}

    };

    m_ope_double = {
        {"==", TokenType::OP_EQ_EQ}, {"!=", TokenType::OP_NEQ},
        {"<=", TokenType::OP_LE_EQ}, {">=", TokenType::OP_GE_EQ},
        {"//", TokenType::OP_DIV}
    };

    m_brackets = {
        {"(", TokenType::CAR_LPAREN}, {")", TokenType::CAR_RPAREN},
        {"[", TokenType::CAR_LBRACKET}, {"]", TokenType::CAR_RBRACKET},
        {",", TokenType::CAR_COMMA}, {":", TokenType::CAR_COLON}
    };

    m_scope.push(0);
}

/* Main tokenization function */
std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;
    std::string buffer;

    while (lookahead()) {
        if (std::isalpha(lookahead()) || lookahead() == '_') {
            handleIdentifierOrKeyword(tokens, buffer);
        }
        else if (std::isdigit(lookahead())) {
            handleInteger(tokens, buffer);
        }
        else if (isDoubleOperatorStart(lookahead())) {
            handleDoubleOperator(tokens, buffer);
        }
        else if (m_ope_simple.contains(std::string(1, lookahead()))) {
            handleSimpleOperator(tokens, buffer);
        }
        
        else if (lookahead() == '!') {
            handleNotEqual(tokens, buffer);
        }
        else if (lookahead() == '/') {
            handleDivision(tokens, buffer);
        }
        else if (m_brackets.contains(std::string(1, lookahead()))) {
            handleBracket(tokens, buffer);
        }
        else if (lookahead() == '\n') {
            handleNewline(tokens);
        }
        else if (lookahead() == ' ' || lookahead() == '\t' || lookahead() == '\r') {
            progress();
        }
        else if (lookahead() == '"') {
            handleString(tokens, buffer);
        }
        else if (lookahead() == '#') {
            skipComment();
        }
        else {

            char unexpectedChar = lookahead();
            if (unexpectedChar == '\0') {
                m_errorManager.addError(Error{"Unexpected character: ", "End of input", "Lexical", m_line});
            } else {
                std::string character(1, unexpectedChar); // Crée une chaîne contenant un seul caractère
                m_errorManager.addError(Error{"Unexpected character: ", character, "Lexical", m_line});
            }
            // m_errorManager.displayErrors();
            // exit(EXIT_FAILURE);
            progress();
        }
    }

    endOfFile(tokens);
    return tokens;
}

// Helper function definitions
void Lexer::handleIdentifierOrKeyword(std::vector<Token>& tokens, std::string& buffer) {
    buffer.push_back(progress());
    while (lookahead() && (std::isalnum(lookahead()) || lookahead() == '_')) {
        buffer.push_back(progress());
    }
    if (m_keywords.contains(buffer)) {
        tokens.push_back({.type = m_keywords[buffer], .value = buffer, .line = m_line});
    } else {
        tokens.push_back({.type = TokenType::IDF, .value = buffer, .line = m_line});
    }
    buffer.clear();
}

void Lexer::handleInteger(std::vector<Token>& tokens, std::string& buffer) {
    if (lookahead() == '0') {
        buffer.push_back(progress());
        if (std::isalnum(lookahead())) {
            reportError("Integers cannot start with zeros", m_line);
            while (std::isalnum(lookahead())) {
               progress();
            }
            return;
        }
    } else {
        while (lookahead() && std::isdigit(lookahead())) {
            buffer.push_back(progress());
        }
        if (std::isalpha(lookahead())) {
            reportError("Identifier cannot start with a digit", m_line);
        } else if (buffer.size() > 79) {
            reportError("Identifier name too long", m_line);
        }
    }
    tokens.push_back({.type = TokenType::INTEGER, .value = buffer, .line = m_line});
    buffer.clear();
}

void Lexer::handleSimpleOperator(std::vector<Token>& tokens, std::string& buffer) {
    buffer.push_back(progress());
    tokens.push_back({.type = m_ope_simple[buffer], .value = buffer, .line = m_line});
    buffer.clear();
}

void Lexer::handleDoubleOperator(std::vector<Token>& tokens, std::string& buffer) {
    buffer.push_back(progress());
    if(lookahead() == '='){
        buffer.push_back(progress());
        tokens.push_back({.type=m_ope_double[buffer], .value=buffer, .line = m_line });
        buffer.clear();
    }
    else if(isalnum(lookahead()) || isspace(lookahead())){
        tokens.push_back({.type=m_ope_simple[buffer], .value=buffer, .line = m_line });
        buffer.clear();
    }
}

void Lexer::handleNotEqual(std::vector<Token>& tokens, std::string& buffer) {
    buffer.push_back(progress());
    if (lookahead() == '=') {
        buffer.push_back(progress());
        tokens.push_back({.type = TokenType::OP_NEQ, .value = buffer, .line = m_line});
    } else {
        reportError("Expected '=' after '!'", m_line);
    }
    buffer.clear();
}

void Lexer::handleDivision(std::vector<Token>& tokens, std::string& buffer) {
    buffer.push_back(progress());
    if (lookahead() == '/') {
        buffer.push_back(progress());
        tokens.push_back({.type = TokenType::OP_DIV, .value = buffer, .line = m_line});
    } else {
        reportError("Expected '/' after '/'", m_line);
    }
    buffer.clear();
}

void Lexer::handleBracket(std::vector<Token>& tokens, std::string& buffer) {
    buffer.push_back(progress());
    tokens.push_back({.type = m_brackets[buffer], .value = buffer, .line = m_line});
    buffer.clear();
}

void Lexer::handleNewline(std::vector<Token>& tokens) {
    tokens.push_back({.type = TokenType::NEWLINE, .value = "", .line = m_line});
    m_line++;
    progress();

    int indentation = 0;
    while (lookahead() == ' ' || lookahead() == '\t') {
        progress();
        indentation++;
    }
    manageIndentation(tokens, indentation);
}

void Lexer::handleString(std::vector<Token>& tokens, std::string& buffer) {
    progress();  // Skip initial quote
    while (true) {
        if (!lookahead()) {
            reportError("Reached end of file without closing string", m_line);
            break;
        } else if (lookahead() == '"') {
            progress();  // Skip closing quote
            break;
        } else if (lookahead() == '\\') {
            progress();
            handleEscapeCharacter(buffer);
        } else if (lookahead() == '\n') {
            m_line++;
            progress();
        } else {
            buffer.push_back(progress());
        }
    }
    tokens.push_back({.type = TokenType::STRING, .value = buffer, .line = m_line});
    buffer.clear();
}

void Lexer::skipComment() {
    while (lookahead() && lookahead() != '\n') {
        progress();
    }
}

void Lexer::endOfFile(std::vector<Token>& tokens) {
    while (m_scope.top() != 0) {
        m_scope.pop();
        tokens.push_back({.type = TokenType::END, .value = "", .line = m_line});
    }
    tokens.push_back({.type = TokenType::ENDOFFILE, .value = "", .line = m_line});
}

void Lexer::manageIndentation(std::vector<Token>& tokens, int n) {
    if (n > m_scope.top()) {
        m_scope.push(n);
        tokens.push_back({.type = TokenType::BEGIN, .value = "", .line = m_line});
    } else if (n < m_scope.top()) {
        while (n < m_scope.top()) {
            m_scope.pop();
            tokens.push_back({.type = TokenType::END, .value = "", .line = m_line});
        }
        if (n != m_scope.top()) {
            reportError("Indentation error", m_line);
        }
    }
}

void Lexer::handleEscapeCharacter(std::string& buffer) {
    if (lookahead() == '"' || lookahead() == '\\') {
        buffer.push_back(progress());
    } else if (lookahead() == 'n') {
        progress();
        buffer.push_back('\n');
    } else {
        buffer.push_back('\\');
    }
}

/*  Error handling for the Lexer */
void Lexer::reportError(const std::string& message, int line) const {
    m_errorManager.addError(Error{message, "", "Lexical", line});
}


char Lexer::lookahead(int ahead) const {
    if (m_pos + ahead >= static_cast<int>(m_src.length())) {
        return '\0';
    }
    return m_src[m_pos + ahead];
}

/* Advance and return next character */
char Lexer::progress() {
    return m_src[++m_pos];
}

/* Check if character starts a double operator */
bool Lexer::isDoubleOperatorStart(char ch) const {
    return ch == '=' || ch == '<' || ch == '>';
}

/* Utility function to convert TokenType to a string (for display) */
std::string Lexer::tokenTypeToString(TokenType type) {
    switch (type) {
        case TokenType::IDF: return "Identifier";
        case TokenType::INTEGER: return "Integer";
        case TokenType::STRING: return "String";
        case TokenType::NEWLINE: return "Newline";
        case TokenType::ENDOFFILE: return "End of File";
        case TokenType::KW_AND: return "Keyword: and";
        case TokenType::KW_DEF: return "Keyword: def";
        case TokenType::KW_ELSE: return "Keyword: else";
        case TokenType::KW_FOR: return "Keyword: for";
        case TokenType::KW_IF: return "Keyword: if";
        case TokenType::KW_TRUE: return "Keyword: true";
        case TokenType::KW_FALSE: return "Keyword: false";
        case TokenType::KW_IN: return "Keyword: in";
        case TokenType::KW_NOT: return "Keyword: not";
        case TokenType::KW_OR: return "Keyword: or";
        case TokenType::KW_PRINT: return "Keyword: print";
        case TokenType::KW_RETURN: return "Keyword: return";
        case TokenType::KW_NONE: return "Keyword: none";
        case TokenType::KW_WHILE: return "Keyword: while";
        case TokenType::OP_PLUS: return "Operator: +";
        case TokenType::OP_MINUS: return "Operator: -";
        case TokenType::OP_MUL: return "Operator: *";
        case TokenType::OP_MOD: return "Operator: %";
        case TokenType::OP_EQ: return "Operator: =";
        case TokenType::OP_EQ_EQ: return "Operator: ==";
        case TokenType::OP_GE: return "Operator: >";
        case TokenType::OP_LE: return "Operator: <";
        case TokenType::OP_NEQ: return "Operator: !=";
        case TokenType::OP_LE_EQ: return "Operator: <=";
        case TokenType::OP_GE_EQ: return "Operator: >=";
        case TokenType::OP_DIV: return "Operator: /";
        case TokenType::CAR_LPAREN: return "Symbol: (";
        case TokenType::CAR_RPAREN: return "Symbol: )";
        case TokenType::CAR_LBRACKET: return "Symbol: [";
        case TokenType::CAR_RBRACKET: return "Symbol: ]";
        case TokenType::CAR_COMMA: return "Symbol: ,";
        case TokenType::CAR_COLON: return "Symbol: :";
        case TokenType::BEGIN: return "Scope: Begin";
        case TokenType::END: return "Scope: End";
        default: return "Unknown";
    }
}

/* Display function to print out the tokens */
void Lexer::displayTokens(const std::vector<Token>& tokens) {
    for (const auto& token : tokens) {
        std::cout << "Token: " << tokenTypeToString(token.type)
                  << ", Value: " << token.value
                  << ", Line: " << token.line << '\n';
    }
}

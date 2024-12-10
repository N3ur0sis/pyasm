#include "parser.h"
#include <iostream>

Parser::Parser(const std::vector<Token>& tokens) : tokens(tokens), pos(0) {}

std::shared_ptr<ASTNode> Parser::parse() {
    return parseRoot();
}

void Parser::print(const std::shared_ptr<ASTNode>& node, int depth) {
    if (!node) return;
    std::cout << std::string(depth * 2, ' ') << node->type;
    if (!node->value.empty()) std::cout << ": " << node->value;
    std::cout << std::endl;
    for (const auto& child : node->children) print(child, depth + 1);
}

Token Parser::peek() {
    if (pos < tokens.size()) return tokens[pos];
    return {TokenType::ENDOFFILE, ""};
}

Token Parser::next() {
    return pos < tokens.size() ? tokens[pos++] : Token{TokenType::ENDOFFILE, ""};
}

bool Parser::expect(TokenType type) {
    if (peek().type == type) {
        next();
        return true;
    }
    return false;
}

void Parser::skipNewlines() {
    while (peek().type == TokenType::NEWLINE) next();
}

std::shared_ptr<ASTNode> Parser::parseRoot() {
    auto root = std::make_shared<ASTNode>("Program");
    while (peek().type != TokenType::ENDOFFILE) {
        skipNewlines();
        auto expr = parseExpr();
        if (expr) root->children.push_back(expr);
        skipNewlines();
    }
    return root;
}

std::shared_ptr<ASTNode> Parser::parseExpr() {
    return parseOrExpr();
}

std::shared_ptr<ASTNode> Parser::parsePrimary() {
    Token tok = peek();
    if (expect(TokenType::INTEGER)) return std::make_shared<ASTNode>("Integer", tok.value);
    if (expect(TokenType::IDF)) return std::make_shared<ASTNode>("Identifier", tok.value);
    if (expect(TokenType::CAR_LPAREN)) {
        auto expr = parseExpr();
        expect(TokenType::CAR_RPAREN); // Ensure closing parenthesis
        return expr;
    }
    if (expect(TokenType::CAR_LBRACKET)) {
        auto expr = parseE();
        expect(TokenType::CAR_RBRACKET); // Ensure closing parenthesis
        return expr;
    }
    std::cerr << "Unexpected token: " << tok.value << std::endl;
    return nullptr;
}
std::shared_ptr<ASTNode> Parser::parseE() {
    auto exprNode = parseExpr(); 
    auto ePrimeNode = parseEPrime(); 
    
    auto listNode = std::make_shared<ASTNode>("List");
    listNode->children.push_back(exprNode);
    if (ePrimeNode) {
        auto currentNode = ePrimeNode;
        while (currentNode) {
            listNode->children.push_back(currentNode->children.front());  
            if (currentNode->children.size() > 1) {
                currentNode = currentNode->children[1];
            } else {
                currentNode = nullptr;
            } 
        }
    }
    
    return listNode;
}

std::shared_ptr<ASTNode> Parser::parseEPrime() {
    if (expect(TokenType::CAR_COMMA)) {
        auto exprNode = parseExpr(); 
        auto ePrimeNode = parseEPrime();  
        auto commaNode = std::make_shared<ASTNode>("EPrime");

        commaNode->children.push_back(exprNode);
        if (ePrimeNode) {
            commaNode->children.push_back(ePrimeNode);
        }
        
        return commaNode;
    }
    return nullptr;
}
std::shared_ptr<ASTNode> Parser::parseOrExpr() {
    auto left = parseAndExpr();
    while (expect(TokenType::KW_OR)) {
        auto opNode = std::make_shared<ASTNode>("Or");
        opNode->children.push_back(left);
        opNode->children.push_back(parseAndExpr());
        left = opNode;
    }
    return left;
}

std::shared_ptr<ASTNode> Parser::parseAndExpr() {
    auto left = parseCompExpr();
    while (expect(TokenType::KW_AND)) {
        auto opNode = std::make_shared<ASTNode>("And");
        opNode->children.push_back(left);
        opNode->children.push_back(parseCompExpr());
        left = opNode;
    }
    return left;
}

std::shared_ptr<ASTNode> Parser::parseCompExpr() {
    auto left = parseArithExpr();
    if (peek().type == TokenType::OP_EQ || peek().type == TokenType::OP_NEQ ||
        peek().type == TokenType::OP_LE || peek().type == TokenType::OP_GE) {
        auto compOp = next();
        auto opNode = std::make_shared<ASTNode>("Compare", compOp.value);
        opNode->children.push_back(left);
        opNode->children.push_back(parseArithExpr());
        return opNode;
    }
    return left;
}

std::shared_ptr<ASTNode> Parser::parseArithExpr() {
    auto left = parseTerm();
    while (peek().type == TokenType::OP_PLUS || peek().type == TokenType::OP_MINUS) {
        auto arithOp = next();
        auto opNode = std::make_shared<ASTNode>("ArithOp", arithOp.value);
        opNode->children.push_back(left);
        opNode->children.push_back(parseTerm());
        left = opNode;
    }
    return left;
}

std::shared_ptr<ASTNode> Parser::parseTerm() {
    auto left = parseFactor();
    while (peek().type == TokenType::OP_MUL || peek().type == TokenType::OP_DIV || peek().type == TokenType::OP_MOD) {
        auto termOp = next();
        auto opNode = std::make_shared<ASTNode>("TermOp", termOp.value);
        opNode->children.push_back(left);
        opNode->children.push_back(parseFactor());
        left = opNode;
    }
    return left;
}

std::shared_ptr<ASTNode> Parser::parseFactor() {
    if (expect(TokenType::OP_MINUS)) {
        auto opNode = std::make_shared<ASTNode>("UnaryOp", "-");
        opNode->children.push_back(parsePrimary());
        return opNode;
    }
    return parsePrimary();
}


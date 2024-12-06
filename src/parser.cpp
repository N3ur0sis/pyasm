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
    return parsePrimary();
}

std::shared_ptr<ASTNode> Parser::parsePrimary() {
    Token tok = peek();
    if (expect(TokenType::INTEGER)) return std::make_shared<ASTNode>("Integer", tok.value);
    if (expect(TokenType::IDF)) return std::make_shared<ASTNode>("Identifier", tok.value);
    std::cerr << "Unexpected token: " << tok.value << std::endl;
    return nullptr;
}
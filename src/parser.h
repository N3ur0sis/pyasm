#pragma once
#include <memory>
#include <vector>
#include "lexer.h"

struct ASTNode {
    std::string type;
    std::string value;
    std::vector<std::shared_ptr<ASTNode>> children;

    ASTNode(std::string t, std::string v = "") : type(std::move(t)), value(std::move(v)) {}
};

class Parser {
public:
    explicit Parser(const std::vector<Token>& tokens);
    std::shared_ptr<ASTNode> parse(); // Entry point of the parser
    void print(const std::shared_ptr<ASTNode>& node, int depth = 0); // Print AST

private:
    const std::vector<Token>& tokens;
    int pos;

    Token peek();       // Look the current token
    Token next();       // Consume the current token
    bool expect(TokenType type); // Match a token
    bool expectR(TokenType type); // Match a token
    void skipNewlines(); // Skip newline tokens
    std::shared_ptr<ASTNode> parseRoot(); // Parse root program
    std::shared_ptr<ASTNode> parseExpr(); // Parse an expression
    std::shared_ptr<ASTNode> parsePrimary(); // Parse a single value
    std::shared_ptr<ASTNode> parseE();
    std::shared_ptr<ASTNode> parseEPrime();
    std::shared_ptr<ASTNode> parseOrExpr();
    std::shared_ptr<ASTNode> parseAndExpr();
    std::shared_ptr<ASTNode> parseCompExpr();
    std::shared_ptr<ASTNode> parseArithExpr();
    std::shared_ptr<ASTNode> parseTerm();
    std::shared_ptr<ASTNode> parseFactor();
    std::shared_ptr<ASTNode> parseOperator(TokenType type, const std::string& opName);
};
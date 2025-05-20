#pragma once
#include <memory>
#include <vector>
#include "lexer.h"
#include "errorManager.h"

struct ASTNode {
    std::string type;
    std::string value;
    std::vector<std::shared_ptr<ASTNode>> children;
    std::string line = "0"; // Line number for error reporting

    ASTNode(std::string t, std::string v = "") : type(std::move(t)), value(std::move(v)) {}
};

class Parser {
public:
    explicit Parser(const std::vector<Token>& tokens, ErrorManager& errorManager);
    std::shared_ptr<ASTNode> parse(); // Entry point of the parser
    void print(const std::shared_ptr<ASTNode>& node, int depth = 0); // Print AST
    void exportToDot(const std::shared_ptr<ASTNode>& node, std::ostream& out); // Export AST to DOT format
    void generateDotFile(const std::shared_ptr<ASTNode>& root, const std::string& filename); // Generate DOT file
	std::shared_ptr<ASTNode> parsePrint();
private:
    const std::vector<Token>& tokens;
    ErrorManager& m_errorManager;
    long pos;
    bool EOF_bool = false;

    Token peek();       // Look the current token
    Token next();       // Consume the current token
    bool expect(TokenType type); // Match a token
    bool expectR(TokenType type); // Match a token
    void skipNewlines(); // Skip newline tokens
    void continueParsing();
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
    std::shared_ptr<ASTNode> parseTest(const std::shared_ptr<ASTNode>& idNode);
    std::shared_ptr<ASTNode> parseExprPrime();
    std::shared_ptr<ASTNode> parseSimpleStmt();
    std::shared_ptr<ASTNode> parseStmtSeconde();
    std::shared_ptr<ASTNode> parseStmt();
    std::shared_ptr<ASTNode> parseDefinition();
    std::shared_ptr<ASTNode> parseSuite();
    void handleInvalidNewlines(TokenType closingToken);
};

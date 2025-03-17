#pragma once

#include <memory>
#include "parser.h"
#include "errorManager.h"
#include "symbolTable.h"

/**
 * Minimal semantic analyzer that checks:
 *  - usage of undeclared variables
 *  - assignment compatibility
 *  - function call parameter count
 * 
 * And now properly switches to a function's child scope.
 */
class SemanticAnalyzer {
public:
    explicit SemanticAnalyzer(ErrorManager& errMgr)
        : m_errorManager(errMgr) {}

    // Entry point
    void checkSemantics(const std::shared_ptr<ASTNode>& root, SymbolTable* globalTable);

private:
    ErrorManager& m_errorManager;

    // Recursive visitation
    void visit(const std::shared_ptr<ASTNode>& node, SymbolTable* currentScope);

    // Specific checks
    void checkVariableUsage(const std::shared_ptr<ASTNode>& node, SymbolTable* currentScope);
    void checkAssignment(const std::shared_ptr<ASTNode>& node, SymbolTable* currentScope);
    void checkFunctionCall(const std::shared_ptr<ASTNode>& node, SymbolTable* currentScope);

    // Utility: find symbol in the current scope chain
    Symbol* findSymbol(const std::string& name, SymbolTable* table);
};
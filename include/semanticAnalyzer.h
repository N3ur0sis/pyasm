#pragma once

#include <memory>
#include "parser.h"
#include "errorManager.h"
#include "symbolTable.h"

class SemanticAnalyzer {
public:
    explicit SemanticAnalyzer(ErrorManager& errMgr)
        : m_errorManager(errMgr) {}

    // Point d’entrée de l’analyse
    void checkSemantics(const std::shared_ptr<ASTNode>& root, SymbolTable* globalTable);

private:
    ErrorManager& m_errorManager;

    // Visite récursive des nœuds de l'AST
    void visit(const std::shared_ptr<ASTNode>& node, SymbolTable* currentScope);

    // Vérifications spécifiques
    void checkFunctionRedefinition(const std::shared_ptr<ASTNode>& node, SymbolTable* currentScope);
    void checkFunctionCall(const std::shared_ptr<ASTNode>& node, SymbolTable* currentScope);

    // Recherche d'un symbole dans la portée courante ou ses ancêtres
    Symbol* findSymbol(const std::string& name, SymbolTable* table);
};

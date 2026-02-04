#pragma once

#include <memory>
#include <unordered_set>
#include "parser.h"
#include "errorManager.h"
#include "symbolTable.h"

class SemanticAnalyzer {
public:
    explicit SemanticAnalyzer(ErrorManager& errMgr)
        : m_errorManager(errMgr) {}

    // Points d’entrée de l’analyse
    const std::shared_ptr<ASTNode>& firstPass(const std::shared_ptr<ASTNode>& root);
    void checkSemantics(const std::shared_ptr<ASTNode>& root, SymbolTable* globalTable);

private:
    const std::unordered_set<std::string> kForbiddenNames = {"range", "len", "list", "print"};

    ErrorManager& m_errorManager;

    // Visite récursive des nœuds de l'AST
    void visit(const std::shared_ptr<ASTNode>& node, SymbolTable* currentScope);

    // Vérifications spécifiques
    void checkFunctionRedefinition(const std::shared_ptr<ASTNode>& node);
    void checkFunctionCall(const std::shared_ptr<ASTNode>& node, SymbolTable* currentScope);
    void checkReturnPlacement(SymbolTable* scope, std::string line);

    // Recherche d'un symbole dans la portée courante ou ses ancêtres
    Symbol* findSymbol(const std::string& name, SymbolTable* table);
    // Vérifie que la portée courante est une fonction
    bool insideFunction(SymbolTable* tbl);
    void checkIdentifierInitialization(const std::shared_ptr<ASTNode>& node, SymbolTable* scope);

};

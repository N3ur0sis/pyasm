#include "semanticAnalyzer.h"
#include <iostream>

void SemanticAnalyzer::checkSemantics(const std::shared_ptr<ASTNode>& root, SymbolTable* globalTable) {
    // Start the recursive visitation from the global scope
    visit(root, globalTable);
}

void SemanticAnalyzer::visit(const std::shared_ptr<ASTNode>& node, SymbolTable* currentScope) {
    if (!node) return;

    // ---- SCOPE SWITCH FOR FUNCTION DEFINITION ----
    // If this node is a FunctionDefinition, we find its child scope
    // that was created in SymbolTableGenerator (named "function <funcName>").
    if (node->type == "FunctionDefinition") {
        // Look for the child table "function <node->value>"
        SymbolTable* functionTable = nullptr;
        for (auto& childTbl : currentScope->children) {
            // The symbol-table generator gave the function scope a name like "function add"
            if (childTbl->scopeName == "function " + node->value) {
                functionTable = childTbl.get();
                break;
            }
        }

        // If no child scope found, we could raise an error or skip
        if (!functionTable) {
            // You can add an error or a warning here if desired.
            return;
        }

        // By convention, node->children[0] = parameter list, node->children[1] = function body
        // We'll only descend into the body with the child scope
        if (node->children.size() >= 2) {
            auto functionBody = node->children[1];
            visit(functionBody, functionTable);
        }
        // Do NOT continue the usual "for (child in node->children) ..." recursion
        // because we've already visited the relevant child with a new scope.
        return;
    }

    // ---- USUAL CHECKS ----
    if (node->type == "Identifier") {
        checkVariableUsage(node, currentScope);
    }
    else if (node->type == "Affect") {
        checkAssignment(node, currentScope);
    }
    else if (node->type == "FunctionCall") {
        checkFunctionCall(node, currentScope);
    }

    // ---- RECURSE INTO CHILDREN ----
    for (const auto& child : node->children) {
        visit(child, currentScope);
    }
}

/**
 * 1) Ensure a variable is declared
 */
void SemanticAnalyzer::checkVariableUsage(const std::shared_ptr<ASTNode>& node, SymbolTable* currentScope) {
    if (!currentScope->lookup(node->value)) {
        m_errorManager.addError(Error{
            "Variable non déclarée: ",
            node->value,
            "Semantic",
            0  // no line info here
        });
    }
}

/**
 * 2) Check assignment type compatibility, presence of variable
 */
void SemanticAnalyzer::checkAssignment(const std::shared_ptr<ASTNode>& node, SymbolTable* currentScope) {
    if (node->children.size() < 2) return;

    auto varNode = node->children[0];
    auto exprNode = node->children[1];

    Symbol* sym = findSymbol(varNode->value, currentScope);
    if (!sym) {
        m_errorManager.addError(Error{
            "Affectation à une variable non déclarée: ",
            varNode->value,
            "Semantic",
            0
        });
        return;
    }

    // Minimal type check (everything is "int" except node->type == "String")
    std::string varType = "int";
    if (sym->symCat == "variable") {
        if (auto vs = dynamic_cast<VariableSymbol*>(sym)) {
            varType = vs->type;
        }
    }

    std::string exprType = "int";
    if (exprNode->type == "String") {
        exprType = "string";
    }

    if (varType != exprType) {
        std::string errMsg = "Type incompatible pour l'affectation (" + varType + " <- " + exprType + ")";
        m_errorManager.addError(Error{
            errMsg,
            varNode->value,
            "Semantic",
            0
        });
    }
}

/**
 * 3) Check function calls: existence, parameter count
 */
void SemanticAnalyzer::checkFunctionCall(const std::shared_ptr<ASTNode>& node, SymbolTable* currentScope) {
    if (node->children.size() < 2) return;

    auto funcIdNode = node->children[0];
    auto paramListNode = node->children[1];

    Symbol* sym = findSymbol(funcIdNode->value, currentScope);
    if (!sym) {
        m_errorManager.addError(Error{
            "Appel d'une fonction non déclarée: ",
            funcIdNode->value,
            "Semantic",
            0
        });
        return;
    }

    if (sym->symCat != "function") {
        m_errorManager.addError(Error{
            "Tentative d'appeler un symbole qui n'est pas une fonction: ",
            funcIdNode->value,
            "Semantic",
            0
        });
        return;
    }

    auto funcSym = dynamic_cast<FunctionSymbol*>(sym);
    if (!funcSym) return;  // fallback

    int expectedParams = funcSym->numParams;
    int actualParams = (int) paramListNode->children.size();
    if (expectedParams != actualParams) {
        std::string msg = "Nombre de paramètres incorrect pour la fonction " 
                          + funcIdNode->value + " (attendu " 
                          + std::to_string(expectedParams) + ", reçu "
                          + std::to_string(actualParams) + ")";
        m_errorManager.addError(Error{
            msg,
            funcIdNode->value,
            "Semantic",
            0
        });
    }
}

/**
 * Utility: search for a symbol in current scope or ancestors
 */
Symbol* SemanticAnalyzer::findSymbol(const std::string& name, SymbolTable* table) {
    for (auto& sPtr : table->symbols) {
        if (sPtr->name == name) {
            return sPtr.get();
        }
    }
    if (table->parent) {
        return findSymbol(name, table->parent);
    }
    return nullptr;
}
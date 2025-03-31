#include "semanticAnalyzer.h"
#include <iostream>

// TODO : un bloc ne peut pas cacher une variable de même nom dans un bloc parent donc si un bloc reaffecte une variable, il faut vérifier qu'elle existe avant
// et que la variable est de même type que ce qu'on veut réaffecter.


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

        // If no child scope found, we raise a warning : it should be a bug with the symbol-table generator
        if (!functionTable) {
            m_errorManager.addError(Error{
                "Warning - No function found in the symbol-table: ",
                node->type,
                "Semantic",
                0
            }); 
            return;
        }

        // By convention, node->children[0] = parameter list, node->children[1] = function body
        // We'll only descend into the body with the child scope
        if (node->children.size() >= 2) {
            auto functionBody = node->children[1];
            visit(functionBody, functionTable);
        }
        return;
    }

    // ---- USUAL CHECKS ----
    if (node->type == "Identifier") {
        checkVariableUsage(node, currentScope);
    }
    else if (node->type == "Affect") {
        std::string rightType = returnRightType(node, currentScope);              // Types possibles à droite : integer, string, list, bool, None
        // TODO : vérifier si c'est pertinent
        if (rightType == "None") {
            m_errorManager.addError(Error{
                "Affectation à une variable de type None: ",
                node->value,
                "Semantic",
                0
            });
        }
        setLeftType(node, currentScope, rightType); // Types possibles à gauche : integer, string, list, bool, None
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

/**
 * Utility: return the type of the given expression
 * (e.g. for "5", return "int")
 * (e.g. for "[5, 6, 7]", return "int[]")
 * (e.g. for "["5", 'hello']", return "string[]")
 * (e.g. for variable1 * variable2, computes recursively)
 * 
 * @param node The AST node representing the expression.
 * @param currentScope The current symbol table scope.
 * @return The type of the expression as a string.
 */
std::string SemanticAnalyzer::returnExprType(const std::shared_ptr<ASTNode>& node, SymbolTable* currentScope) {
    if (node->children.size() < 2) return "None";

    auto exprNode = node->children[1];

    // TODO : gérer les autres types
    if (exprNode->type == "Integer") {
        return "int";
    }
    else if (exprNode->type == "String") {
        return "string";
    }
    else if (exprNode->type == "List") {
        for (const auto& child : exprNode->children) {
            if (child->type == "Integer") {
                return "int[]";
            }
            else if (child->type == "String") {
                if (verifyListConsistency(exprNode)) {
                    return "string[]";
                }
                else {
                    m_errorManager.addError(Error{
                        "Les listes doivent-être homogènes: ",
                        exprNode->type,
                        "Semantic",
                        0
                    });
                }
                return "string[]";
            }
            // TODO : gérer les autres types et autres cas
        }
    }
    else if (exprNode->type == "Boolean") {
        return "bool";
    }
    else if (exprNode->type == "None") {
        return "None";
    }

    return "None";  // default case
}

/**
 * Utility: set the type of the left side of an assignment
 * (e.g. for "x = 5", set the type of "x" to "int")
 */


 /*
 * Utility: verify the consistency of a list
 *
 * This function checks if all elements in the list are of the same type.
 * 
 * @param node The AST node representing the list with its children representing the elements.
 * @return true if all elements are of the same type, false otherwise.
 */

bool SemanticAnalyzer::verifyListConsistency(const std::shared_ptr<ASTNode>& node) {
    if (node->children.empty()) return true;
    
    std::string expectedType = "";
    std::vector<std::string> variableNames;
    
    // First pass: determine expected type and collect variable names
    for (const auto& child : node->children) {
        if (child->type == "Integer") {
            if (expectedType.empty()) {
                expectedType = "int";
            } else if (expectedType != "int") {
                return false;
            }
        } else if (child->type == "String") {
            if (expectedType.empty()) {
                expectedType = "string";
            } else if (expectedType != "string") {
                return false;
            }
        } else if (child->value == "True" || child->value == "False") {
            if (expectedType.empty()) {
                expectedType = "bool";
            } else if (expectedType != "bool") {
                return false;
            }
        } else if (child->type == "Identifier") {
            // TODO : find a symbol in the current scope or parent scopes
            Symbol* sym = findSymbol(child->value, currentScope);
            if (sym && sym->symCat == "variable") {
                if (auto vs = dynamic_cast<VariableSymbol*>(sym)) {
                    if (vs->type != "auto" && !expectedType.empty() && vs->type != expectedType) {
                        return false;
                    } else if (vs->type != "auto" && expectedType.empty()) {
                        expectedType = vs->type;
                    } else if (vs->type == "auto") {
                        variableNames.push_back(child->value);
                    }
                }
            }
        } else if (child->type == "List") {
            // TODO : handle nested lists
            if (!verifyListConsistency(child)) {
                return false;
            }
        } else if (child->type == "None") {
            if (expectedType.empty()) {
                expectedType = "None";
            } else if (expectedType != "None") {
                return false;
            }
        } else if (child->type == "TermOp" || child->type == "ArithOp") {
            // TODO : handle operations
            std::string leftType = returnExprType(child->children[0], currentScope);
            std::string rightType = returnExprType(child->children[1], currentScope);
            if (leftType != rightType) {
                return false;
            }
        }
    }
    
    // If we found a type and have variables with assumed type
    if (!expectedType.empty() && !variableNames.empty()) {
        for (const auto& varName : variableNames) {
            Symbol* sym = findSymbol(varName, currentScope);
            if (sym && sym->symCat == "variable") {
                if (auto vs = dynamic_cast<VariableSymbol*>(sym)) {
                    if (vs->type == "auto") {
                        vs->assumedType = expectedType; // Set the assumed type
                    }
                }
            }
        }
    }
    
    return true;
}
#include "semanticAnalyzer.h"
#include <iostream>

void SemanticAnalyzer::checkSemantics(const std::shared_ptr<ASTNode>& root, SymbolTable* globalTable) {
    // Démarre l’analyse depuis la racine de l’arbre syntaxique et la table des symboles globale
    visit(root, globalTable);
}


// Visite récursive des nœuds de l'AST
// @param node : le nœud de l'AST à visiter
// @param currentScope : la portée courante dans laquelle se trouve le nœud
// @return : void
void SemanticAnalyzer::visit(const std::shared_ptr<ASTNode>& node, SymbolTable* currentScope) {
    if (!node) return;

    // ---- DÉFINITION DE FONCTION ----
    if (node->type == "FunctionDefinition") {
        checkFunctionRedefinition(node, currentScope);

        // Recherche de la table de symboles locale de la fonction
        SymbolTable* functionTable = nullptr;
        for (auto& child : currentScope->children) {
            if (child->scopeName == "function " + node->value) {
                functionTable = child.get();
                break;
            }
        }

        if (!functionTable) {
            return; // Fonction probablement mal construite, on ignore
        }

        // Visite du corps de la fonction uniquement
        if (node->children.size() >= 2) {
            auto functionBody = node->children[1];
            visit(functionBody, functionTable);
        }

        return;
    }

    // ---- APPEL DE FONCTION ----
    if (node->type == "FunctionCall") {
        checkFunctionCall(node, currentScope);
    }

    // ---- UTILISATION D’IDENTIFICATEUR ----
    if (node->type == "Identifier") {
        // Ce test ne peut pas vraiment détecter les cas utiles (cf. logique dynamique avec "auto")
        // TODO : éventuellement faire une fonction de vérification statique de l'utilisation d'une variable avant déclaration
    }

    // ---- DESCENTE RÉCURSIVE ----
    for (const auto& child : node->children) {
        visit(child, currentScope);
    }
}

// Vérifie qu'une fonction n'est pas redéfinie avec le même nom
// @return : true si la fonction est redéfinie, false sinon
// @param node : le nœud de l'AST représentant la fonction
// @param currentScope : la portée courante dans laquelle la fonction est définie
void SemanticAnalyzer::checkFunctionRedefinition(const std::shared_ptr<ASTNode>& node, SymbolTable* currentScope) {
    int occurrences = 0;
    for (const auto& sym : currentScope->symbols) {
        if (sym->name == node->value && sym->symCat == "function") {
            occurrences++;
        }
    }

    if (occurrences > 1) {
        m_errorManager.addError(Error{
            "Redéfinition interdite de la fonction : ",
            node->value,
            "Semantic",
            0
        });
    }
}

// Vérifie que l’appel de fonction correspond à une fonction connue
// @param node : le nœud de l'AST représentant l'appel de fonction
// @param currentScope : la portée courante dans laquelle la fonction est appelée
// @return : void
void SemanticAnalyzer::checkFunctionCall(const std::shared_ptr<ASTNode>& node, SymbolTable* currentScope) {
    if (node->children.empty()) return;
    if (currentScope == nullptr) return;

    // TODO : vérifier le nombre de paramètres lors d'un appel de fonction
    /*
    auto funcSym = dynamic_cast<FunctionSymbol*>(sym);
    int expectedParams = funcSym->numParams;
    int actualParams = (int) node->children[1]->children.size();
    if (expectedParams != actualParams) {
        m_errorManager.addError(Error{
            "Nombre de paramètres incorrect pour la fonction " + funcIdNode->value,
            funcIdNode->value,
            "Semantic",
            0
        });
    }
    */
   return;
}

// Recherche récursive dans la portée courante et ses ancêtres
// @param name : le nom du symbole à rechercher
// @param table : la table des symboles dans laquelle effectuer la recherche
// @return : le symbole trouvé ou nullptr si non trouvé
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

#include "semanticAnalyzer.h"
#include <iostream>
#include <algorithm>

// Liste des noms de variables de boucle for ouvertes
std::vector<std::string> loopVariables;

// Lance l’analyse sémantique à partir de la racine de l’AST
void SemanticAnalyzer::checkSemantics(const std::shared_ptr<ASTNode>& root, SymbolTable* globalTable) {
    // Démarre l’analyse depuis la racine de l’arbre syntaxique et la table des symboles globale
    visit(root, globalTable);
}

// Parcourt récursivement l’AST et déclenche les vérifications sémantiques appropriées
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

    // ---- RETURN PLACEMENT ----
    if (node->type == "Return") {
        checkReturnPlacement(currentScope);
    }

    // ---- FOR LOOP INTERN SHADOWING ----
    if (node->type == "For") {
        std::string loopVar = node->children[0]->value;
        std::cout << "PUSH: " << loopVar << std::endl;
        loopVariables.push_back(loopVar);
    }
    // ---- AFFECT IN FOR LOOP ----
    if (node->type == "Affect") {
        std::string affectIdent = node->children[0]->value;
        if (std::find(loopVariables.begin(), loopVariables.end(), affectIdent) != loopVariables.end()) {
            m_errorManager.addError({
                "You can't affect a variable with this name, shadowing a loop variable is forbidden: ",
                affectIdent,
                "Semantic",
                0
            });
        }
    }

    // ---- DESCENTE RÉCURSIVE ----
    for (const auto& child : node->children) {
        visit(child, currentScope);
    }

    // ---- SORTIE DU FOR ----
    if (node->type == "For") {
        std::cout << "POP: " << loopVariables.back() << std::endl;
        loopVariables.pop_back();
    }
}

// ────────────────────────────────────────────────────────────────
// Vérifie qu’une fonction n’est pas redéfinie ou nommée avec 
// un mot réservé
// ────────────────────────────────────────────────────────────────
void SemanticAnalyzer::checkFunctionRedefinition(const std::shared_ptr<ASTNode>& node, SymbolTable* currentScope) {
    int occurrences = 0;
    if (kForbiddenNames.count(node->value)) {
        m_errorManager.addError({
            "Définir une fonction nommée '" + node->value + "' est interdit (nom réservé).",
            node->value,
            "Semantic",
            0
        });
        return;
    }

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
    return;
}

// ────────────────────────────────────────────────────────────────
// Vérifie que l’appel de fonction correspond à une fonction 
// existante (non encore paramétré pour l’arité)
// ────────────────────────────────────────────────────────────────
void SemanticAnalyzer::checkFunctionCall(const std::shared_ptr<ASTNode>& node, SymbolTable* scope){
    if (node->children.empty()) return;
    
    auto functionCalled = node->children[0];
    auto paramList = (node->children.size() > 1) ? node->children[1] : nullptr;

    if (kForbiddenNames.count(functionCalled->value)) {
        if (functionCalled->value == "print") {
          if (!paramList || paramList->children.size() == 0) {
            m_errorManager.addError({
                "La fonction print prend au moins un paramètre.",
                functionCalled->value,
                "Semantic",
                0
            });
          }
          return;
        } 
        // Verifie que les fonction builtin sont appellées avec le bon nombre de paramètres (1)
        if (paramList && paramList->children.size() != 1) {
            m_errorManager.addError({
                "La fonction " + functionCalled->value + " prend un et un seul paramètre.",
                functionCalled->value,
                "Semantic",
                0
            });
        }
        return;
    }

    // vérifie que la fonction appellée a été définie
    Symbol* sym = findSymbol(functionCalled->value, scope);
    if (!sym || sym->symCat != "function") {
        m_errorManager.addError({
            "La fonction n’est pas définie : ",
            functionCalled->value, 
            "Semantic", 
            0
        });
        return;
    }


    // Vérifie que la fonction est appelée avec le bon nombre de paramètres
    if (auto fnSym = dynamic_cast<FunctionSymbol*>(sym)) {
        int expected = fnSym->numParams;
        int actual = paramList ? (int)paramList->children.size() : 0;
        if (expected != actual) {
            m_errorManager.addError({
                "Nombre de paramètres incorrect pour la fonction : ",
                functionCalled->value, 
                "Semantic", 
                0
            });
        }
        return;
    }
}

// ────────────────────────────────────────────────────────────────
// Détecte une instruction return en dehors d’un corps de fonction
// ────────────────────────────────────────────────────────────────
void SemanticAnalyzer::checkReturnPlacement(SymbolTable* scope){
    if (!insideFunction(scope)) {
        m_errorManager.addError({
            "Return statement outside of a function.",
            "", 
            "Syntax", 
            0
        });
    }
}


// ---- Fonctions utilitaires ----


// ────────────────────────────────────────────────────────────────
// Recherche récursive d’un symbole dans la portée actuelle 
// et ses parents
// ────────────────────────────────────────────────────────────────
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

// ────────────────────────────────────────────────────────────────
// Vérifie si une portée donnée correspond à une fonction
// ────────────────────────────────────────────────────────────────
bool SemanticAnalyzer::insideFunction(SymbolTable* tbl){
    if (!tbl) return false;
    if (tbl->scopeName.rfind("function ", 0) == 0) return true;
    return false;
}

// ────────────────────────────────────────────────────────────────
// Vérifie qu’un identifiant utilisé est bien défini dans la portée
// ────────────────────────────────────────────────────────────────
void SemanticAnalyzer::checkIdentifierInitialization(const std::shared_ptr<ASTNode>& node, SymbolTable* scope) {
    if (!node || node->type != "Identifier") return;

    Symbol* sym = findSymbol(node->value, scope);
    if (!sym) {
        m_errorManager.addError({
            "Utilisation d’une variable non initialisée : ",
            node->value,
            "Semantic",
            0
        });
    }
}


#include "semanticAnalyzer.h"
#include <iostream>
#include <algorithm>

// Liste des noms de variables de boucle for ouvertes
std::vector<std::string> loopVariables;
// Liste des noms de fonctions déjà définies
std::vector<std::string> definedFunctionsNames;


std::vector<std::string> ACCESSIBLE_GLOB;
std::shared_ptr<ASTNode> NEW_DEF;

void SemanticAnalyzer::smallhelpR(const std::shared_ptr<ASTNode>& node, const std::shared_ptr<ASTNode>& defnode, const std::shared_ptr<ASTNode>& parentnode, const int n, std::string suffixe) {
    if (node->type == "FunctionCall") {
        if  (defnode != nullptr) {
            // si défini dans le self même : += suffixe + name + "_"
            for (auto& d : defnode->children[1]->children) {
                if (node->children[0]->value == d->value) {
                    node->children[0]->value = suffixe + defnode->value + "_" + node->children[0]->value;
                    goto recurNOW;
                }
            }
            // si == à self name : += suffixe + "_"
            if (node->children[0]->value == defnode->value) {
                node->children[0]->value = suffixe + "_" + node->children[0]->value;
                goto recurNOW;
            }
        }
        if (parentnode != nullptr) {
            // si défini dans parent : += suffixe + "_"
            int i = 0;
            for (auto& d : parentnode->children[1]->children) {
                if (i >= n) break;
                if (suffixe + "_" + node->children[0]->value == d->value) {
                    node->children[0]->value = suffixe + "_" + node->children[0]->value;
                    goto recurNOW;
                }
                ++i;
            }
        }
        // si dans ACCESSIBLE_GLOB : += "_"
        if (std::find(ACCESSIBLE_GLOB.begin(), ACCESSIBLE_GLOB.end(), node->children[0]->value) != ACCESSIBLE_GLOB.end()) {
            node->children[0]->value = std::string("_") + node->children[0]->value;
            goto recurNOW;
        }
        // sinon erreur sémantique (et pas de modification)
        m_errorManager.addError({
            "Call to nonexistent/inaccessible function.",
            "",
            "Semantic",
            std::stoi(node->line)
        });   
    }

    //std::cout << "HERE FDP" << std::endl;

    // parcours récursif
    recurNOW:
    for (auto& child : node->children) {
        if (child) smallhelpR(child, defnode, parentnode, n, suffixe);
    }
}

void SemanticAnalyzer::bighelpR(const std::shared_ptr<ASTNode>& defnode, const std::shared_ptr<ASTNode>& parentnode, const int n, std::string suffixe) {
    // parcours des functioncalls
    smallhelpR(defnode->children[2], defnode, parentnode, n, suffixe);
    // parcours enfants
    int i = 0;
    for (auto& child : defnode->children[1]->children) {
        bighelpR(child, defnode, i, suffixe + defnode->value);
        ++i;
    }
    // renommage
    defnode->value = suffixe + std::string("_") + defnode->value;
    // suppression du noeud definition
    defnode->children.erase(defnode->children.begin() + 1);
    // ajout dans NEW_DEF
    NEW_DEF->children.push_back(defnode);
}

const std::shared_ptr<ASTNode>& SemanticAnalyzer::firstPass(const std::shared_ptr<ASTNode>& root) {
    NEW_DEF = std::make_shared<ASTNode>("Definitions");

    //std::cout << "HERE 1" << std::endl;

    for (auto& defnode : root->children[0]->children) {
        // parcours des functioncalls
        smallhelpR(defnode->children[2], defnode, nullptr, 0, "");
        // parcours enfants
        //std::cout << "HERE 2" << std::endl;
        int i = 0;
        for (auto& child : defnode->children[1]->children) {
            bighelpR(child, defnode, i, defnode->value);
            ++i;
        }
        // ajout dans ACCESSIBLE_GLOB
        ACCESSIBLE_GLOB.push_back(defnode->value);
        // renommage
        defnode->value = "_" + defnode->value;
        // suppression du noeud definition
        defnode->children.erase(defnode->children.begin() + 1);
        // ajout dans NEW_DEF
        NEW_DEF->children.push_back(defnode);
    }

    //std::cout << "HERE 3" << std::endl;

    smallhelpR(root->children[1], nullptr, nullptr, 0, "");
    root->children[0] = NEW_DEF;

    return root;
}

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
        checkFunctionRedefinition(node);

        // Recherche de la table de symboles locale de la fonction
        SymbolTable* functionTable = nullptr;
        for (auto& child : currentScope->children) {
            if (child->scopeName == "function " + node->value) {
                functionTable = child.get();
                break;
            }
        }

        if (!functionTable) {
            return; // Fonction probablement mal construite ou non utilisée, on ignore
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

    // ---- PRINT PARAMETER CHECK ----
    if (node->type == "Print") {
        if (node->children.empty()) { // Simplified check: direct children are arguments
            m_errorManager.addError({
                "Print function should be called with at least one parameter.",
                "",
                "Semantic",
                std::stoi(node->line)
            });
        }
    }

    // ---- UTILISATION D’IDENTIFICATEUR ----
    if (node->type == "Identifier") {
        // Une inférence de types statique n'est pas très efficace ici, mais peut quand même détecter quelques erreurs plus tôt
        // On va préférer la logique dynamique avec "auto" et vérifications directement dans la génération de code
        // TODO : éventuellement faire une fonction de vérification statique de l'utilisation d'une variable avant déclaration
    }

    // ---- RETURN PLACEMENT ----
    if (node->type == "Return") {
        checkReturnPlacement(currentScope, node->line);
    }

    // ---- FOR LOOP INTERN SHADOWING ----
    if (node->type == "For") {
        std::string loopVar = node->children[0]->value;
        if (std::find(loopVariables.begin(), loopVariables.end(), loopVar) != loopVariables.end()) {
            m_errorManager.addError(Error{
                "Loop variable name already exists in scope: " + currentScope->scopeName + ". Variable shadowing is not allowed: ",
                loopVar,
                "semantic",
                std::stoi(node->line)
            });
        } 
        // Add loop variable to the list of loop variables
        loopVariables.push_back(loopVar);
    }
    // ---- AFFECT IN FOR LOOP ----
    if (node->type == "Affect") {
        std::string affectIdent = node->children[0]->value;
        if (std::find(loopVariables.begin(), loopVariables.end(), affectIdent) != loopVariables.end()) {
            m_errorManager.addError(Error{
                "You can't affect a variable with this name, shadowing a loop variable is forbidden: ",
                affectIdent,
                "Semantic",
                std::stoi(node->line)
            });
        }
    }

    // ---- DESCENTE RÉCURSIVE ----
    for (const auto& child : node->children) {
        visit(child, currentScope);
    }

    // ---- SORTIE DU FOR ----
    if (node->type == "For") {
        loopVariables.pop_back();
    }
}

// ────────────────────────────────────────────────────────────────
// Vérifie qu’une fonction n’est pas redéfinie ou nommée avec 
// un mot réservé
// ────────────────────────────────────────────────────────────────
void SemanticAnalyzer::checkFunctionRedefinition(const std::shared_ptr<ASTNode>& node) {
    // Noms réservés (range, list, len, print) déjà vérifiés dans le parser

    // Vérifie si la fonction est déjà définie
    if (std::find(definedFunctionsNames.begin(), definedFunctionsNames.end(), node->value) != definedFunctionsNames.end()) {
        m_errorManager.addError(Error{
            "Function already defined: ",
            "A function already exists with the name " + node->value + ".",
            "Semantic",
            std::stoi(node->line)
        });
    } else {
        definedFunctionsNames.push_back(node->value);
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
        // Print n'est pas dans un FunctionCall, mais une instruction
        // Verifie que les fonction builtin sont appellées avec le bon nombre de paramètres (1)
        if (paramList && paramList->children.size() != 1) {
            m_errorManager.addError(Error{
                "Function " + functionCalled->value + " expects exactly one parameter.",
                "",
                "Semantic",
                std::stoi(node->line)
            });
        }
        return;
    }

    // vérifie que la fonction appellée a été définie
    Symbol* sym = findSymbol(functionCalled->value, scope);
    if (!sym || sym->symCat != "function") {
        m_errorManager.addError(Error{
            "Function Call Error: ",
            "Function " + functionCalled->value + " is not defined.",
            "Semantic", 
            std::stoi(node->line)
        });
        return;
    }


    // Vérifie que la fonction est appelée avec le bon nombre de paramètres
    if (auto fnSym = dynamic_cast<FunctionSymbol*>(sym)) {
        int expected = fnSym->numParams;
        int actual = paramList ? (int)paramList->children.size() : 0;
        if (expected != actual) {
            m_errorManager.addError(Error{
                        "Function Call Error: ",
                        "Function " + fnSym->name + " expects " + std::to_string(expected) + 
                        " arguments, but " + std::to_string(actual) + " were provided.",
                        "Semantic",
                        std::stoi(node->line)
                    });
        }
        return;
    }
}

// ────────────────────────────────────────────────────────────────
// Détecte une instruction return en dehors d’un corps de fonction
// ────────────────────────────────────────────────────────────────
void SemanticAnalyzer::checkReturnPlacement(SymbolTable* scope, std::string line){
    if (!insideFunction(scope)) {
        m_errorManager.addError(Error{
            "Return statement outside of a function.",
            "", 
            "Syntax", 
            std::stoi(line)
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
// Inutilisé
void SemanticAnalyzer::checkIdentifierInitialization(const std::shared_ptr<ASTNode>& node, SymbolTable* scope) {
    if (!node || node->type != "Identifier") return;

    Symbol* sym = findSymbol(node->value, scope);
    if (!sym) {
        m_errorManager.addError(Error{
            "Uninitialized identifier: ",
            node->value,
            "Semantic",
            std::stoi(node->line)
        });
    }
}


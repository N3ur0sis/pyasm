#include "symbolTable.h"
#include "parser.h"
#include <sstream>

int AUTO_OFFSET = 0;

int SymbolTable::calculateTypeSize(const std::string& type) {
    // Default to 0 for auto types for this part of the code
    // Real offset calculation will be done in semantic analysis
    return AUTO_OFFSET;
}

void SymbolTable::addSymbol(const Symbol& symbol) {
    // Vérifie la redéfinition dans la portée courante
    for (const auto& symPtr : symbols) {
        if (symPtr->name == symbol.name) {
            return;
        }
    }

    std::unique_ptr<Symbol> symClone;
    if (symbol.symCat == "variable") {
        if (auto varSym = dynamic_cast<const VariableSymbol*>(&symbol))
            symClone = std::make_unique<VariableSymbol>(*varSym);
        else
            symClone = std::make_unique<Symbol>(symbol);
    }
    else if (symbol.symCat == "function") {
        if (auto funcSym = dynamic_cast<const FunctionSymbol*>(&symbol))
            symClone = std::make_unique<FunctionSymbol>(*funcSym);
        else
            symClone = std::make_unique<Symbol>(symbol);
    }
    else if (symbol.symCat == "array") {
        if (auto arraySym = dynamic_cast<const ArraySymbol*>(&symbol))
            symClone = std::make_unique<ArraySymbol>(*arraySym);
        else
            symClone = std::make_unique<Symbol>(symbol);
    }
    else {
        symClone = std::make_unique<Symbol>(symbol);
    }

    // Premier calcul de l'offset : on ne le connais pas encore
    // On le met à 0 pour l'instant, il sera mis à jour plus tard
    // FIXME : ça marche comme ça mais c'est moche (ça sert à rien)
    if (symClone->symCat == "variable" || symClone->symCat == "array") {
        int symbolSize = AUTO_OFFSET;
        if (auto vs = dynamic_cast<VariableSymbol*>(symClone.get())) {
            symbolSize = calculateTypeSize(vs->type);
        } else if (auto as = dynamic_cast<ArraySymbol*>(symClone.get())) {
            symbolSize = calculateTypeSize(as->elementType) * as->size;
        }
        symClone->offset = nextOffset;
        nextOffset += (symbolSize + 7) & ~7;
    }

    symbols.push_back(std::move(symClone));
}

bool SymbolTable::lookup(const std::string& name) const {
    // Check in current scope
    for (const auto& s : symbols) {
        if (s->name == name)
            return true;
    }
    
    // Recursively check parent scopes
    if (parent) {
        return parent->lookup(name);
    }
    
    return false;
}

void SymbolTable::print(std::ostream& out, int indent) const {
    std::string indentStr(indent, ' ');
    out << indentStr << "Scope: " << scopeName << "\n";

    for (const auto& sPtr : symbols) {
        Symbol* s = sPtr.get();
        if (s->symCat == "variable") {
            VariableSymbol* vs = dynamic_cast<VariableSymbol*>(s);
            out << indentStr << " " << s->category << " : " << s->name
                << " (type=" << vs->type << ", global=" << (vs->isGlobal ? "true" : "false") 
                << ", offset: " << s->offset << ")";
        } else if (s->symCat == "function") {
            FunctionSymbol* fs = dynamic_cast<FunctionSymbol*>(s);
            out << indentStr << " " << s->category << " : " << s->name
            << " (returnType=" << fs->returnType << ", numParams=" << fs->numParams
            << ", offset: " << s->offset << ")";
        } else if (s->symCat == "array") {
            if (ArraySymbol* as = dynamic_cast<ArraySymbol*>(s)) {
                out << indentStr << " " << s->category << " : " << s->name
                << " (elementType=" << as->elementType << ", size=" << as->size
                << ", offset: " << s->offset << ")";
            }
        } else {
            out << indentStr << " " << s->category << " : " << s->name
            << " (offset: " << s->offset << ")";
        }
        out << std::endl;
    }

    for (const auto& child : children) {
        child->print(out, indent + 2);
    }
}

std::unique_ptr<SymbolTable> SymbolTableGenerator::generate(const std::shared_ptr<ASTNode>& root) {
    auto globalTable = std::make_unique<SymbolTable>("global", nullptr);
    visit(root, globalTable.get());
    return globalTable;
}

void SymbolTableGenerator::visit(const std::shared_ptr<ASTNode>& node, SymbolTable* currentTable) {
    if (!node){
        return;
    }

    if (node->type == "FunctionDefinition") {
        // Récupérer le nom de la fonction
        std::string funcName = node->value; // ex: "add"
    
        // Créer un symbole de fonction
        FunctionSymbol funcSymbol {
            funcName,
            "autoFun", // type de retour par défaut dans un premier temps
            0,         // nombre de paramètres sera mis à jour
            0          // offset
        };
    
        // La liste de paramètres est généralement node->children[0]
        // On compte le nombre de paramètres
        if (!node->children.empty()) {
            auto paramList = node->children[0];
            funcSymbol.numParams = (int)paramList->children.size();
        }
    
        // Ajouter le symbole fonction dans la table actuelle
        currentTable->addSymbol(funcSymbol);
    
        // Créer une table enfant pour la portée de la fonction
        auto functionTable = std::make_unique<SymbolTable>("function " + funcName, currentTable);
        // On remet l'offset à 0, si on gère l'empilement
        functionTable->nextOffset = 0;
    
        // Ajout des paramètres à la table enfant
        if (!node->children.empty()) {
            auto paramList = node->children[0];
            for (const auto& param : paramList->children) {
                VariableSymbol paramSymbol {
                    param->value,
                    "auto",       // type par défaut dans un premier temps
                    "parameter",
                    0           // Offset will be calculated
                };
                functionTable->addSymbol(paramSymbol);
            }
        }
    
        // Ensuite, on visite le corps de la fonction (node->children[1], s’il existe)
        if (node->children.size() >= 2) {
            visit(node->children[1], functionTable.get());  
        }
    
        // Enfin, on rattache la table enfant
        currentTable->children.push_back(std::move(functionTable));
        return;
    }
    else if (node->type == "Affect") {
        if (!node->children.empty()) {
            auto varNode = node->children[0];
            if (varNode && varNode->type == "Identifier") {
                if (!currentTable->lookup(varNode->value)) {
                    VariableSymbol varSymbol(
                        varNode->value,
                        "auto",  // type par défaut dans un premier temps
                        "variable",
                        0  // Offset will be calculated
                    );
                    currentTable->addSymbol(varSymbol);
                }
            }
        }
        return;
    }
    else if (node->type == "For") {
        // Create a new scope for the for loop
        auto forTable = std::make_unique<SymbolTable>("for loop", currentTable);
        
        if (node->children.size() >= 3) {
            auto loopVar = node->children[0];
            if (loopVar && loopVar->type == "Identifier") {
                VariableSymbol loopVariable(
                    loopVar->value,
                    "auto",             // type par défaut dans un premier temps
                    "loop variable",
                    0                   // Offset will be calculated
                );
                forTable->addSymbol(loopVariable);
            }
            
            // Process loop condition
            visit(node->children[1], forTable.get());


            // Process loop body
            visit(node->children[2], forTable.get());
        }
        
        currentTable->children.push_back(std::move(forTable));
        return;
    }
    // TODO : Est-ce que par exemple y = [5,6,7] doit être écrit comme une variable de type tableau de int ou autrement ?
    else if (node->type == "List") {
        // Create a new entry for the list
        if (!node->children.empty()) {
            int nbElement = node->children.size();
            // Extract the list type from the first element (if exists)
            std::string elementType = "auto[]";                             // type par défaut dans un premier temps
            
            
            // TODO : déplacer ce morceau de code qui ne vérifie pas grand chose actuellement
            /*
            if (nbElement > 0 && node->children[0]->type != "") {
                elementType = node->children[0]->type;
            }
            // verify that all elements have the same type
            for (int i = 1; i < nbElement; i++) {
                if (node->children[i]->type != elementType) {
                    std::cerr << "Error: List elements must have the same type\n";              // Erreur sémantique
                    return;
                }
            }
            // TODO : fin du code à déplacer
            */
            // Create the array symbol using the proper constructor
            ArraySymbol arraySym(
                node->value,          // name
                elementType,          // element type
                nbElement,            // array size
                0                     // offset will be calculated in addSymbol
            );
            currentTable->addSymbol(arraySym);
        }
    }
    else if (node->type == "IfBody") {
         auto ifTable = std::make_unique<SymbolTable>("IfBody", currentTable);
         for (const auto& child : node->children) {
             visit(child, ifTable.get());
         }
         currentTable->children.push_back(std::move(ifTable));
        return;
    }
    else if (node->type == "ElseBody") {
        auto elseTable = std::make_unique<SymbolTable>("ElseBody", currentTable);
        for (const auto& child : node->children) {
            visit(child, elseTable.get());
        }
        currentTable->children.push_back(std::move(elseTable));
       return;
   }

    // Recursively visit child nodes
    for (const auto& child : node->children) {
        visit(child, currentTable);
    }
}

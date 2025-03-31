#include "symbolTable.h"
#include "parser.h"
#include <sstream>

int AUTO_OFFSET = 0;

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

    // L'offset est mis à AUTO_OFFSET = 0 par défaut
    if (symClone->symCat == "variable" || symClone->symCat == "array") {
        int symbolSize = AUTO_OFFSET;
        if (dynamic_cast<VariableSymbol*>(symClone.get())) {
            symbolSize = AUTO_OFFSET;                                           // The size of the variable is not known yet
        } else if (auto as = dynamic_cast<ArraySymbol*>(symClone.get())) {
            symbolSize = AUTO_OFFSET * as->size;
        }

        // TODO : ce morceau de code est bon pour permettre l'alignement dans la mémoire
        // Mais il ne sert plus à rien ici, à réutiliser dans la génération de code
            // symClone->offset = nextOffset;
            // nextOffset += (symbolSize + 7) & ~7;
    }

    symbols.push_back(std::move(symClone));
}

/*
 * Look up a symbol by name in the current scope and parent scopes.
 * @return true if found, false otherwise.
*/

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
                << "(size=" << as->size
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
    visit(root, root, globalTable.get());
    return globalTable;
}

/*
 * This function now uses root as a parameter to permit the creation of function scopes by searching in the declaration node
*/

void SymbolTableGenerator::visit(const std::shared_ptr<ASTNode>& root, SymbolTable* globalTable, const std::shared_ptr<ASTNode>& node, SymbolTable* currentTable) {
    if (!node) {
        return;
    }

    // Handle variable assignments and usage
    if (node->type == "Affect") {
        if (!node->children.empty()) {
            auto varNode = node->children[0];
            if (varNode && varNode->type == "Identifier") {
                // Always add variables to the global table unless in a function scope
                SymbolTable* targetTable = currentTable;
                while (targetTable->parent && !targetTable->scopeName.starts_with("function ")) {
                    targetTable = targetTable->parent;
                }

                if (!targetTable->lookup(varNode->value)) {
                    VariableSymbol varSymbol(
                        varNode->value,
                        "auto",  // Default type, will be refined later
                        "variable",
                        0        // Offset will be calculated
                    );
                    targetTable->addSymbol(varSymbol);
                }
            }
        }
    }
    // Handle For loops
    else if (node->type == "For") {
        // Create a scope for the for loop
        auto forTable = std::make_unique<SymbolTable>("for loop", currentTable);
        
        if (node->children.size() >= 3) {
            // Add loop variable only to the for scope
            auto loopVar = node->children[0];
            if (loopVar && loopVar->type == "Identifier") {
                VariableSymbol loopVariable(
                    loopVar->value,
                    "auto",
                    "loop variable",
                    0
                );
                forTable->addSymbol(loopVariable);
            }
            
            // Process loop condition and body
            // Any other variables in the loop body will be added to the global table
            visit(root, node->children[1], forTable.get());
            visit(root, node->children[2], forTable.get());
        }
        
        currentTable->children.push_back(std::move(forTable));
        return;
    }
    // Handle List declarations
    else if (node->type == "List") {
        if (!node->children.empty()) {
            int nbElement = node->children.size();
            
            // Find the appropriate table (global or function)
            SymbolTable* targetTable = currentTable;
            while (targetTable->parent && !targetTable->scopeName.starts_with("function ")) {
                targetTable = targetTable->parent;
            }
            
            ArraySymbol arraySym(
                node->value,
                nbElement,
                0
            );
            targetTable->addSymbol(arraySym);
        }
    }
    // Handle If bodies
    else if (node->type == "IfBody") {
        auto ifTable = std::make_unique<SymbolTable>("IfBody", currentTable);
        for (const auto& child : node->children) {
            visit(root, child, ifTable.get());
        }
        currentTable->children.push_back(std::move(ifTable));
        return;
    }
    // Handle Else bodies
    else if (node->type == "ElseBody") {
        auto elseTable = std::make_unique<SymbolTable>("ElseBody", currentTable);
        for (const auto& child : node->children) {
            visit(root, child, elseTable.get());
        }
        currentTable->children.push_back(std::move(elseTable));
        return;
    }
    // Handle Function Calls - parse function definition to create function scope
    else if (node->type == "FunctionCall") {
        std::string funcName = node->value;
        
        // Check if the function exists in the root AST
        auto findFunctionDef = [&root, &funcName](const std::shared_ptr<ASTNode>& currentNode) -> std::shared_ptr<ASTNode> {
            if (currentNode && currentNode->type == "FunctionDefinition" && currentNode->value == funcName) {
                return currentNode;
            }
            
            for (const auto& child : currentNode->children) {
                auto result = findFunctionDef(child);
                if (result) {
                    return result;
                }
            }
            
            return nullptr;
        };
        
        auto functionDefNode = findFunctionDef(root);
        if (functionDefNode) {
            // Create function symbol and add to global table
            FunctionSymbol funcSymbol {
                funcName,
                "auto", // Default return type
                0,      // Number of parameters
                0       // Offset
            };
            
            // Count parameters
            if (!functionDefNode->children.empty()) {
                auto paramList = functionDefNode->children[0];
                funcSymbol.numParams = (int)paramList->children.size();
            }
            
            // Add to global table
            SymbolTable* globalTable = currentTable;
            while (globalTable->parent) {
                globalTable = globalTable->parent;
            }
            
            if (!globalTable->lookup(funcName)) {
                globalTable->addSymbol(funcSymbol);
            }
            
            // Create function scope
            auto functionTable = std::make_unique<SymbolTable>("function " + funcName, globalTable);
            functionTable->nextOffset = 0;
            
            // Process parameters
            if (!functionDefNode->children.empty()) {
                auto paramList = functionDefNode->children[0];
                for (const auto& param : paramList->children) {
                    // Check shadowing rules - no local variable should have same name as global
                    if (globalTable->lookup(param->value)) {
                        // Here you would typically handle the error
                        std::cerr << "Error: Parameter '" << param->value 
                                  << "' shadows a global variable in function '" 
                                  << funcName << "'" << std::endl;
                    }
                    
                    VariableSymbol paramSymbol {
                        param->value,
                        "auto",
                        "parameter",
                        0
                    };
                    functionTable->addSymbol(paramSymbol);
                }
            }
            
            // Process function body
            if (functionDefNode->children.size() >= 2) {
                visit(root, functionDefNode->children[1], functionTable.get());
            }
            
            // Add function table to global table
            globalTable->children.push_back(std::move(functionTable));
        }
    }
    // Handle Function definitions
    else if (node->type == "FunctionDefinition") {
        // Function definitions are handled during function calls
        // We skip them here since we want to process them only when called
        return;
    }

    // Recursively visit all other nodes
    for (const auto& child : node->children) {
        visit(root, child, currentTable);
    }
}

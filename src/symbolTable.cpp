#include "symbolTable.h"
#include "parser.h"
#include <sstream>

// Constant sizes for different types
const int INT_SIZE = 4;     // 4 bytes for integer
const int FLOAT_SIZE = 4;   // 4 bytes for float
const int POINTER_SIZE = 8; // 8 bytes for pointers (64-bit system)
const int BOOL_SIZE = 1;    // 1 byte for boolean

int SymbolTable::calculateTypeSize(const std::string& type) {
    if (type == "int") return INT_SIZE;
    if (type == "float") return FLOAT_SIZE;
    if (type == "bool") return BOOL_SIZE;
    
    // Default to pointer size for complex types like lists, objects
    return POINTER_SIZE;
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

    // Calcule l'offset pour les variables et tableaux
    if (symClone->symCat == "variable" || symClone->symCat == "array") {
        int symbolSize = POINTER_SIZE;
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
    if (!node)
        return;

    if (node->type == "FunctionDefinition") {
        // Reset offset for function scope
        currentTable->nextOffset = 0;

        FunctionSymbol funcSymbol { 
            node->value, 
            "int",  // Default return type
            0, // Default Number of parameters, changed after processing parameters
            0 // Function offset typically 0
        };

        // Set the number of parameters in the function symbol 
        if (!node->children.empty()) {
            funcSymbol.numParams = (node->children[0])->children.size();
        }
        currentTable->addSymbol(funcSymbol);
        
        auto functionTable = std::make_unique<SymbolTable>("function " + node->value, currentTable);
        
        // Process function parameters
        if (!node->children.empty()) {
            auto paramList = node->children[0];           
            for (const auto& param : paramList->children) {
                VariableSymbol paramSymbol { 
                    param->value,
                    "int",  // Default type
                    "parameter", 
                    0  // Offset will be calculated
                };
                functionTable->addSymbol(paramSymbol);
            }
        }
        
        // Process function body
        if (node->children.size() >= 2) {
            visit(node->children[1], functionTable.get());
        }
        
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
                        "int",  // Default type
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
                    "int",  // Default type
                    "loop variable",
                    0  // Offset will be calculated
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
    else if (node->type == "List") {
        // Create a new entry for the list
        if (!node->children.empty()) {
            int nbElement = node->children.size();
            // Extract the list type from the first element (if exists)
            std::string elementType = "int"; // Default type
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

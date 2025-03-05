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
    // Check if symbol already exists in current scope
    for (const auto& s : symbols) {
        if (s.name == symbol.name) {
            return; // Already declared in this scope
        }
    }

    // Create a copy of the symbol
    Symbol sym = symbol;

    // Determine size based on type
    int symbolSize = calculateTypeSize(sym.type.empty() ? "pointer" : sym.type);
    
    // Calculate offset with 8-byte alignment
    sym.offset = nextOffset;
    nextOffset += (symbolSize + 7) & ~7;
    
    symbols.push_back(sym);
}

bool SymbolTable::lookup(const std::string& name) const {
    // Check in current scope
    for (const auto& s : symbols) {
        if (s.name == name)
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
    
    for (const auto& s : symbols) {
        out << indentStr << " " << s.category << " : " << s.name
            << " (offset: " << s.offset << ")\n";
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

        Symbol funcSymbol { 
            node->value, 
            "function", 
            0,  // Function offset typically 0
            "function"  // Explicit type
        };
        currentTable->addSymbol(funcSymbol);
        
        auto functionTable = std::make_unique<SymbolTable>("function " + node->value, currentTable);
        
        // Process function parameters
        if (!node->children.empty()) {
            auto paramList = node->children[0];
            for (const auto& param : paramList->children) {
                Symbol paramSymbol { 
                    param->value, 
                    "parameter", 
                    0,  // Offset will be calculated
                    "int"  // Default type, could be enhanced
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
                    Symbol varSymbol { 
                        varNode->value, 
                        "variable", 
                        0,  // Offset will be calculated
                        "int"  // Default type
                    };
                    currentTable->addSymbol(varSymbol);
                }
            }
        }
    }
    else if (node->type == "For") {
        // Create a new scope for the for loop
        auto forTable = std::make_unique<SymbolTable>("for loop", currentTable);
        
        if (node->children.size() >= 3) {
            auto loopVar = node->children[0];
            if (loopVar && loopVar->type == "Identifier") {
                Symbol loopSymbol { 
                    loopVar->value, 
                    "loop variable", 
                    0,  // Offset will be calculated
                    "int"  // Default type
                };
                forTable->addSymbol(loopSymbol);
            }
            
            // Process loop body
            visit(node->children[2], forTable.get());
        }
        
        currentTable->children.push_back(std::move(forTable));
        return;
    }

    // Recursively visit child nodes
    for (const auto& child : node->children) {
        visit(child, currentTable);
    }
}
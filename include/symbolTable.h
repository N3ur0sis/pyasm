#pragma once

#include <string>
#include <vector>
#include <memory>
#include <ostream>
#include "parser.h"

// Base Symbol class with common attributes
class Symbol {
public:
    std::string name;        // Name of the symbol
    std::string category;    // Category (variable, function, parameter, etc.)
    int offset;              // Memory offset
    std::string symCat;      // Symbol category (to differentiate derived classes)
    
    // Default constructor
    Symbol() : offset(0) {}
    
    // Parameterized constructor
    Symbol(const std::string& n, const std::string& cat, int off = 0, std::string symCat = "symbol")
        : name(n), category(cat), offset(off), symCat(symCat) {}
        
    // Virtual destructor for polymorphic behavior
    virtual ~Symbol() {}
};

// Derived class for variables
class VariableSymbol : public Symbol {
public:
    bool isGlobal;
    std::string type;
    
    // Parameterized constructor
    VariableSymbol(const std::string& n, const std::string& t, const std::string& cat, bool global = false, int off = 0)
        : Symbol(n, cat, off, "variable"), isGlobal(global), type(t) {}
};

// Derived class for functions
class FunctionSymbol : public Symbol {
public:
    int numParams;
    //std::vector<std::string> paramTypes;
    std::string returnType;
    
    // // Parameterized constructor
    // FunctionSymbol(const std::string& n, const std::string& retType, 
    //                const std::vector<std::string>& params = {}, int off = 0)
    //     : Symbol(n, "function", off), numParams(params.size()), 
    //       paramTypes(params), returnType(retType) {}

    // Parameterized constructor
    FunctionSymbol(const std::string& n, const std::string& retType, int numParams, int off = 0)
        : Symbol(n, "function", off, "function"), numParams(numParams), returnType(retType) {}
};

// Derived class for array/list types
class ArraySymbol : public Symbol {
public:
    int size;
    std::string elementType;
    
    // Parameterized constructor
    ArraySymbol(const std::string& n, const std::string& elemType, int arraySize = 0, int off = 0)
        : Symbol(n, "array", off, "array"), size(arraySize), elementType(elemType) {}
};

class SymbolTable {
public:
    SymbolTable(const std::string& name, SymbolTable* parentTable) 
        : scopeName(name), parent(parentTable), nextOffset(0) {}

    // Add a symbol to the current scope
    void addSymbol(const Symbol& symbol);

    // Look up a symbol by name in current and parent scopes
    bool lookup(const std::string& name) const;

    // Print the symbol table (for debugging)
    void print(std::ostream& out, int indent = 0) const;

    // Calculate size of a type
    static int calculateTypeSize(const std::string& type);

    std::string scopeName;
    SymbolTable* parent;
    std::vector<std::unique_ptr<Symbol>> symbols;
    std::vector<std::unique_ptr<SymbolTable>> children;
    int nextOffset;
};

class SymbolTableGenerator {
public:
    // Generate symbol table from AST root
    static std::unique_ptr<SymbolTable> generate(const std::shared_ptr<ASTNode>& root);

private:
    // Recursive visit method to traverse AST and build symbol table
    static void visit(const std::shared_ptr<ASTNode>& node, SymbolTable* currentTable);
};

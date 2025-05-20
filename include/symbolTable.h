#pragma once

#include <string>
#include <vector>
#include <memory>
#include <ostream>
#include <set> // Required for std::set
#include "parser.h" // Assuming ASTNode is defined here
#include "errorManager.h" // Assuming ErrorManager is defined here

// Forward declaration if ASTNode is not fully included via parser.h for some reason
// struct ASTNode; 

// Base Symbol class with common attributes
class Symbol {
public:
    std::string name;        // Name of the symbol
    std::string category;    // General category (e.g., "variable", "function", "parameter")
    int offset;              // Memory offset (stack-relative or data segment offset)
    std::string symCat;      // Specific C++ class type identifier (e.g., "variable", "function")
    
    // Default constructor
    Symbol() : name(""), category("unknown"), offset(0), symCat("symbol") {}
    
    // Parameterized constructor
    Symbol(const std::string& n, const std::string& cat, int off = 0, std::string sCat = "symbol")
        : name(n), category(cat), offset(off), symCat(sCat) {}
        
    // Virtual destructor for polymorphic behavior
    virtual ~Symbol() = default;
};

// Derived class for variables and parameters
class VariableSymbol : public Symbol {
public:
    bool isGlobal;    // True if in .data, false if on stack (local/parameter)
    std::string type; // e.g., "Integer", "String", "List", "Boolean", "auto"
    // std::string assumedType = "auto"; // Retained if used by semantic analysis

    // Parameterized constructor
    // 'cat' can be "variable" for locals, or "parameter" for function parameters
    VariableSymbol(const std::string& n, const std::string& t, const std::string& cat, bool global = false, int off = 0)
        : Symbol(n, cat, off, "variable"), isGlobal(global), type(t) {}
};

// Derived class for functions
class FunctionSymbol : public Symbol {
public:
    int numParams;
    std::string returnType;
    int tableID;        // ID of the SymbolTable for this function's scope
    int frameSize;      // Total size for local variables on stack, 16-byte aligned

    // Parameterized constructor
    FunctionSymbol(const std::string& n, const std::string& retType, int nParams, int tID, int off = 0, int fSize = 0)
        : Symbol(n, "function", off, "function"), numParams(nParams), returnType(retType), tableID(tID), frameSize(fSize) {}
};

// Derived class for array/list types
class ArraySymbol : public Symbol {
public:
    int size; // Number of elements, or 0 if dynamically sized / pointer
    bool isGlobal;
    std::string elementType; // Type of elements in the array/list

    // Parameterized constructor
    ArraySymbol(const std::string& n, std::string eType = "auto", int arraySize = 0, bool global = false, int off = 0)
        : Symbol(n, "array", off, "array"), size(arraySize), isGlobal(global), elementType(eType) {}
};

class SymbolTable {
public:
    SymbolTable(const std::string& sName, SymbolTable* parentTable, int tID) 
        : scopeName(sName), parent(parentTable), nextDataOffset(0), tableID(tID) {} // Initialize nextDataOffset before tableID

    // Add a symbol to the current scope
    void addSymbol(const Symbol& symbol);

    // Look up a symbol by name in current and parent scopes
    bool lookup(const std::string& name) const;
    Symbol* findSymbol(const std::string& name); // Added to retrieve symbol

    // Look up a symbol by name only in the current scope
    bool immediateLookup(const std::string& name) const;
    Symbol* findImmediateSymbol(const std::string& name); // Added to retrieve symbol

    // Check if a symbol is shadowing a parameter in the current scope
    bool isShadowingParameter(const std::string& name) const;

    // Print the symbol table (for debugging)
    void print(std::ostream& out, int indent = 0) const;

    // Calculate size of a type (retained if used, consider moving if more appropriate elsewhere)
    static int calculateTypeSize(const std::string& type);

    std::string scopeName;
    SymbolTable* parent;
    std::vector<std::unique_ptr<Symbol>> symbols;
    std::vector<std::unique_ptr<SymbolTable>> children;
    int nextDataOffset; // Used for laying out global variables in .data section
    int tableID;
};

class SymbolTableGenerator {
public:
    explicit SymbolTableGenerator(ErrorManager& errMgr); // Remove : m_errorManager(errMgr), nextTableIdCounter(0) {}
    
    // Generate symbol table from AST root
    std::unique_ptr<SymbolTable> generate(const std::shared_ptr<ASTNode>& root);

private:
    ErrorManager& m_errorManager;
    int nextTableIdCounter;
    std::set<std::string> processedFunctionNames; 

    // Main recursive function to build symbol tables and assign layout
    void buildScopesAndSymbols(const std::shared_ptr<ASTNode>& node, SymbolTable* globalTable, SymbolTable* currentScopeTable);
    
    // Helper to discover local variables within a function body and assign stack offsets
    void discoverLocalsAndAssignOffsets(const std::shared_ptr<ASTNode>& bodyNode, SymbolTable* functionScopeTable, int& currentLocalOffset);
    
    // Kept if still used by other parts of your system, e.g. semantic analysis for return type inference
    std::shared_ptr<ASTNode> findFunctionDefNode(const std::shared_ptr<ASTNode>& astRoot, const std::string& funcName);
};

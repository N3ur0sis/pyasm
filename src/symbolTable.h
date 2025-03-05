#ifndef SYMBOL_TABLE_H
#define SYMBOL_TABLE_H

#include <string>
#include <vector>
#include <memory>
#include <ostream>

// Forward declaration of ASTNode to avoid circular dependencies
class ASTNode;

// Symbol structure to represent variables, functions, etc.
struct Symbol {
    std::string name;        // Name of the symbol
    std::string category;    // Category (variable, function, parameter, etc.)
    int offset;              // Memory offset
    std::string type;        // Type of the symbol
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
    std::vector<Symbol> symbols;
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

#endif // SYMBOL_TABLE_H
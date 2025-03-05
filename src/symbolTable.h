#pragma once
#include <string>
#include <vector>
#include <memory>
#include <iostream>


struct Symbol {
    std::string name;
    std::string category;
    int offset;
};


class SymbolTable {
public:

    SymbolTable(std::string scopeName, SymbolTable* parent = nullptr)
        : scopeName(std::move(scopeName)), parent(parent), nextOffset(0) {}


    void addSymbol(const Symbol& symbol);


    bool lookup(const std::string& name) const;


    void print(std::ostream& out, int indent = 0) const;


    std::vector<std::unique_ptr<SymbolTable>> children;

private:
    std::string scopeName;
    SymbolTable* parent;
    std::vector<Symbol> symbols;
    int nextOffset;

    static const int symbolSize = 4;
};

// Générateur de tables des symboles à partir de l'AST
class SymbolTableGenerator {
public:
    std::unique_ptr<SymbolTable> generate(const std::shared_ptr<struct ASTNode>& root);
private:
    void visit(const std::shared_ptr<struct ASTNode>& node, SymbolTable* currentTable);
};

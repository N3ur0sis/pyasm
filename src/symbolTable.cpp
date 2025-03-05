#include "symbolTable.h"
#include "parser.h"  // Pour la définition de ASTNode
#include <sstream>


void SymbolTable::addSymbol(const Symbol& symbol) {

    for (const auto& s : symbols) {
        if (s.name == symbol.name) {
            return; // Déjà déclaré : on ne le rajoute pas
        }
    }

    Symbol sym = symbol;
    sym.offset = nextOffset;
    nextOffset += symbolSize;  // Incrémenter l'offset d'une taille fixe (ici 4)
    symbols.push_back(sym);
}


bool SymbolTable::lookup(const std::string& name) const {
    for (const auto& s : symbols) {
        if (s.name == name)
            return true;
    }
    if (parent) {
        return parent->lookup(name);
    }
    return false;
}


void SymbolTable::print(std::ostream& out, int indent) const {
    std::string indentStr(indent, ' ');
    out << indentStr << "Scope: " << scopeName << "\n";
    for (const auto& s : symbols) {
        out << indentStr << "  " << s.category << " : " << s.name
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

        Symbol funcSymbol { node->value, "function", 0 };
        currentTable->addSymbol(funcSymbol);


        auto functionTable = std::make_unique<SymbolTable>("function " + node->value, currentTable);


        if (!node->children.empty()) {
            auto paramList = node->children[0];
            for (const auto& param : paramList->children) {
                Symbol paramSymbol { param->value, "parameter", 0 };
                functionTable->addSymbol(paramSymbol);
            }
        }

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
                    Symbol varSymbol { varNode->value, "variable", 0 };
                    currentTable->addSymbol(varSymbol);
                }
            }
        }
    }

    else if (node->type == "For") {
        if (!node->children.empty()) {
            auto loopVar = node->children[0];
            if (loopVar && loopVar->type == "Identifier") {
                if (!currentTable->lookup(loopVar->value)) {
                    Symbol loopSymbol { loopVar->value, "variable", 0 };
                    currentTable->addSymbol(loopSymbol);
                }
            }
        }
    }


    for (const auto& child : node->children) {
        visit(child, currentTable);
    }
}

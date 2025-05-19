#include "symbolTable.h"
#include "parser.h"
#include <sstream>
#include <set>

// TODO : la récursivité fait planter la génération des TDS (core dumped)

int AUTO_OFFSET = 0;
int nextTableId = 0; // O is the id of the global table
std::set<std::string> definedFunctions;             // Set to keep track of defined functions
#define RED "\033[31m"
#define RESET "\033[0m"


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
        symClone->offset = nextOffset;
        nextOffset += (symbolSize + 7) & ~7;
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

/*
 * Look up a symbol by name only in the current scope.
 * @return true if found, false otherwise.
*/
bool SymbolTable::immediateLookup(const std::string& name) const {
    // Check only in the current scope
    for (const auto& s : symbols) {
        if (s->name == name)
            return true;
    }
    return false;
}

/*
 * Check if a symbol is shadowing a parameter in the current scope.
 * @return true if shadowing, false otherwise.
*/
bool SymbolTable::isShadowingParameter(const std::string& name) const {
    // Check if the symbol is a parameter in the current scope
    for (const auto& s : symbols) {
        if (s->name == name && s->category == "parameter")
            return true;
    }
    return false;
}

void SymbolTable::print(std::ostream& out, int indent) const {
    std::string indentStr(indent, ' ');
    out << indentStr << RED << "Scope: " << RESET << scopeName << ", " << RED <<"id = " << tableID << RESET << "\n";

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
            << ", offset: " << s->offset 
            << ", table ID: " << fs->tableID << ")";
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
    auto globalTable = std::make_unique<SymbolTable>("global", nullptr, nextTableId++);
    visit(root, globalTable.get(), root->children[1], globalTable.get());
    return globalTable;
}

void SymbolTableGenerator::visit(const std::shared_ptr<ASTNode>& root, SymbolTable* globalTable, const std::shared_ptr<ASTNode>& node, SymbolTable* currentTable) {
    if (!node) {
        return;
    }

    SymbolTable* usedTable;
    if (currentTable->tableID != 0) {
        usedTable = currentTable;
    } else {
        usedTable = globalTable;
    }

    // Handle variable assignments and usage

    if (node->type == "Affect") {
        if (!node->children.empty()) {
            auto varNode = node->children[0];
            auto type = node->children[1]->type;
            if (type != "String"){
                type = "Integer"; // Default type for other variables
            }
            if (varNode && varNode->type == "Identifier") {
                // Always add variables to the global table unless in a function scope
                // If the variable isn't already declared in the current scope
                if (!usedTable->lookup(varNode->value)) {
                    VariableSymbol varSymbol(
                        varNode->value,
                        "auto",  // Default type, will be refined later
                        "variable",
                        0        // Offset will be calculated
                    );
                    if (node->children.size() >= 2 && node->children[1]->type == "List"){
                        varSymbol.type = "auto[]"; // Default list type, will be refined later
                    }
                    usedTable->addSymbol(varSymbol);
                }
                // check if the variable is shadowing a parameter in the current scope
                if (usedTable->tableID > 0 && usedTable->isShadowingParameter(varNode->value)) {
                    m_errorManager.addError(Error{
                        "Variable name already exists in scope as a parameter. Parameter shadowing is not allowed: ",
                        varNode->value,
                        "semantic",
                        std::stoi(varNode->line)
                    });
                }
            }
            

            
        }
    }
    // Handle For loops
    else if (node->type == "For") {
        // Create a scope for the for loop
        auto forTable = std::make_unique<SymbolTable>("for loop", usedTable, nextTableId++);
        
        if (node->children.size() >= 3) {
            // Add loop variable only to the for scope
            auto loopVar = node->children[0];
            if (loopVar && loopVar->type == "Identifier") {
                // Check for shadowing rules - the loop variable should not have the same name as a variable in current scope
                if (usedTable->immediateLookup(loopVar->value)) {
                    m_errorManager.addError(Error{
                        "Loop variable name already exists in scope: " + usedTable->scopeName + ". Variable shadowing is not allowed: ",
                        loopVar->value,
                        "semantic",
                        std::stoi(loopVar->line)
                    });
                }
                // Add loop variable to the for scope
                VariableSymbol loopVariable(
                    loopVar->value,
                    "auto",
                    "loop variable",
                    0
                );
                forTable->addSymbol(loopVariable);
            }
            
            // Any other variables in the loop body will be added to the global table
            visit(root, globalTable, node->children[2], usedTable);
        }
        
        usedTable->children.push_back(std::move(forTable));
        return;
    }
    // Handle If bodies
    else if (node->type == "IfBody") {
        for (const auto& child : node->children) {
            visit(root, globalTable, child, usedTable);
        }
        return;
    }
    // Handle Else bodies
    else if (node->type == "ElseBody") {
        for (const auto& child : node->children) {
            visit(root, globalTable, child, usedTable);
        }
        return;
    }



    // Handle Function Calls - parse function definition to create function scope
    else if (node->type == "FunctionCall") {
        if (node->children.empty()) {
            std::cerr << "Error: Function call without name" << std::endl;              // Erreur possible lié au compilateur, pas au code compilé
            return;
        }
        std::string funcName = node->children[0]->value;

        // Check if the function is already defined in the global table
        if (definedFunctions.find(funcName) != definedFunctions.end()) {
            return; // Function already added to the global table, return without processing
        }
      
        // Now call the standalone function directly in the definitions node of the AST
        auto functionDefNode = findFunctionDef(root->children[0], funcName);
        
        if (functionDefNode) {
            definedFunctions.insert(funcName); // Add function name to the set of defined functions
            // Create function symbol and add to global table
            int functionTableId = nextTableId++;
            if (!globalTable->lookup(funcName)) {
                FunctionSymbol funcSymbol {
                    funcName,
                    "autoFun", // Default return type
                    0,      // Number of parameters
                    0,             // Default Table ID
                    AUTO_OFFSET                  // Default offset
                };
                
                // Count parameters
                if (!functionDefNode->children.empty()) {
                    auto paramList = functionDefNode->children[0];
                    funcSymbol.numParams = (int)paramList->children.size();
                }
                // Add symbol to global table   
                funcSymbol.tableID = functionTableId;
                globalTable->addSymbol(funcSymbol);
            }
            
            // Create function scope
            auto functionTable = std::make_unique<SymbolTable>("function " + funcName, globalTable, functionTableId);
            functionTable->nextOffset = 0;
            
            // Process parameters
            if (!functionDefNode->children.empty()) {
                auto paramList = functionDefNode->children[0];
                for (const auto& param : paramList->children) {
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
                visit(root, globalTable, functionDefNode->children[1], functionTable.get());
            }
            
            // Add function table to global table
            globalTable->children.push_back(std::move(functionTable));
        }
    }

    // Recursively visit all other nodes
    for (const auto& child : node->children) {
        visit(root, globalTable, child, usedTable);
    }
}

/*
 * This function looks for the function definition in the AST.
 * It first checks the "Definitions" node and then searches for the function name.
 * If not found, it adds an error to the error manager.
*/
std::shared_ptr<ASTNode> SymbolTableGenerator::findFunctionDef(const std::shared_ptr<ASTNode>& root, const std::string& funcName){
    auto& node = root;
    if (!node) {
        return nullptr;
    }
    for (const auto& def: node->children) {
        if (def->type == "FunctionDefinition") {
            if (def->value == funcName) {
                return def;
            }
        }
    }
    // Function not found, the error will be handled in the semantic analyzer
    
    return nullptr;
}
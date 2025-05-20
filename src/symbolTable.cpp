#include "symbolTable.h"
#include "parser.h" // Assuming ASTNode might be fully defined here
#include "errorManager.h" // For m_errorManager
#include <sstream>
#include <set>
#include <algorithm> // For std::find_if

// Global nextTableId is now a member of SymbolTableGenerator: nextTableIdCounter
// Global definedFunctions is now a member of SymbolTableGenerator: processedFunctionNames
// Global AUTO_OFFSET seems unused for stack frames, can be removed.

#define RED "\033[31m"
#define RESET "\033[0m"


void SymbolTable::addSymbol(const Symbol& symbol) {
    // Check for redefinition in the current scope only
    for (const auto& symPtr : symbols) {
        if (symPtr->name == symbol.name) {
            // Optionally, throw an error or update the existing symbol based on language rules
            // For now, let's prevent adding if already exists to avoid duplicate entries.
            return;
        }
    }

    std::unique_ptr<Symbol> symClone;
    if (auto vs = dynamic_cast<const VariableSymbol*>(&symbol)) {
        symClone = std::make_unique<VariableSymbol>(*vs);
    } else if (auto fs = dynamic_cast<const FunctionSymbol*>(&symbol)) {
        symClone = std::make_unique<FunctionSymbol>(*fs);
    } else if (auto as = dynamic_cast<const ArraySymbol*>(&symbol)) {
        symClone = std::make_unique<ArraySymbol>(*as);
    } else {
        // Fallback for base Symbol or other derived types if any
        symClone = std::make_unique<Symbol>(symbol);
    }
    
    // Offset assignment is now handled by SymbolTableGenerator during scope creation
    // and layout assignment. The old nextOffset logic here is removed.
    // SymbolTable::nextDataOffset is for globals in .data section.

    symbols.push_back(std::move(symClone));
}

bool SymbolTable::lookup(const std::string& name) const {
    for (const auto& s : symbols) {
        if (s->name == name) return true;
    }
    if (parent) return parent->lookup(name);
    return false;
}

Symbol* SymbolTable::findSymbol(const std::string& name) {
    for (auto& sPtr : symbols) {
        if (sPtr->name == name) return sPtr.get();
    }
    if (parent) return parent->findSymbol(name);
    return nullptr;
}

bool SymbolTable::immediateLookup(const std::string& name) const {
    for (const auto& s : symbols) {
        if (s->name == name) return true;
    }
    return false;
}

Symbol* SymbolTable::findImmediateSymbol(const std::string& name) {
    for (auto& sPtr : symbols) {
        if (sPtr->name == name) return sPtr.get();
    }
    return nullptr;
}


bool SymbolTable::isShadowingParameter(const std::string& name) const {
    for (const auto& s : symbols) {
        // Check if 's->symCat' (preferred) or 's->category' is "parameter"
        if (s->name == name && s->symCat == "parameter")
            return true;
    }
    return false;
}

void SymbolTable::print(std::ostream& out, int indent) const {
    std::string indentStr(indent, ' ');
    out << indentStr << RED << "Scope: " << RESET << scopeName << ", " << RED <<"id = " << tableID 
        << ", nextDataOffset = " << nextDataOffset << RESET << "\n";

    for (const auto& sPtr : symbols) {
        Symbol* s = sPtr.get();
        out << indentStr << "  " << s->symCat << " : " << s->name;
        if (auto vs = dynamic_cast<VariableSymbol*>(s)) {
            out << " (type=" << vs->type << ", global=" << (vs->isGlobal ? "true" : "false") 
                << ", offset: " << vs->offset << ")";
        } else if (auto fs = dynamic_cast<FunctionSymbol*>(s)) {
            out << " (returnType=" << fs->returnType << ", numParams=" << fs->numParams
                << ", frameSize: " << fs->frameSize << ", offset: " << fs->offset 
                << ", table ID: " << fs->tableID << ")";
        } else if (auto as = dynamic_cast<ArraySymbol*>(s)) {
            out << " (elementType=" << as->elementType << ", size=" << as->size 
                << ", global=" << (as->isGlobal ? "true" : "false") << ", offset: " << as->offset << ")";
        } else {
             out << " (offset: " << s->offset << ")";
        }
        out << std::endl;
    }

    for (const auto& child : children) {
        child->print(out, indent + 2);
    }
}

// --- SymbolTableGenerator Implementation ---

SymbolTableGenerator::SymbolTableGenerator(ErrorManager& em) : m_errorManager(em), nextTableIdCounter(0) {}

std::unique_ptr<SymbolTable> SymbolTableGenerator::generate(const std::shared_ptr<ASTNode>& root) {
    nextTableIdCounter = 0; // Reset for each generation run
    processedFunctionNames.clear();
    auto globalTable = std::make_unique<SymbolTable>("global", nullptr, nextTableIdCounter++);
    
    if (root) { // Ensure root is not null
        for (const auto& childNode : root->children) {
            buildScopesAndSymbols(childNode, globalTable.get(), globalTable.get());
        }
    }
    return globalTable;
}

void SymbolTableGenerator::buildScopesAndSymbols(const std::shared_ptr<ASTNode>& node, SymbolTable* globalTable, SymbolTable* currentScopeTable) {
    if (!node) return;

    if (node->type == "FunctionDefinition") {
        std::string funcName = node->value;

        if (processedFunctionNames.count(funcName)) {
            m_errorManager.addError({"Function already defined: ", funcName, "Semantic", std::stoi(node->line)});
            return; 
        }
        processedFunctionNames.insert(funcName);

        int numParams = 0;
        if (node->children.size() > 0 && node->children[0]->type == "FormalParameterList") {
            numParams = node->children[0]->children.size();
        }
        
        // Placeholder for frameSize, will be calculated after processing locals
        FunctionSymbol funcSym(funcName, "autoFun" /*TODO: infer ret type*/, numParams, 0, 0, 0); 
        globalTable->addSymbol(funcSym);
        FunctionSymbol* addedFuncSym = dynamic_cast<FunctionSymbol*>(globalTable->findImmediateSymbol(funcName));
        
        if (!addedFuncSym) { return; }

        auto functionScope = std::make_unique<SymbolTable>("function " + funcName, globalTable, nextTableIdCounter++);
        addedFuncSym->tableID = functionScope->tableID; 
        SymbolTable* funcScopePtr = functionScope.get();

        int paramOffset = 16; // First parameter at [RBP+16]
        if (node->children.size() > 0 && node->children[0]->type == "FormalParameterList") {
            for (const auto& paramNode : node->children[0]->children) {
                // Correct constructor: VariableSymbol(name, type, category, isGlobal, offset)
                VariableSymbol param(paramNode->value, "Integer", "parameter", false, paramOffset); 
                funcScopePtr->addSymbol(param);
                paramOffset += 8; 
            }
        }
        
        int localStartOffset = -8; // First local variable at [RBP-8]
        if (node->children.size() > 1 && node->children[1]->type == "FunctionBody") {
            discoverLocalsAndAssignOffsets(node->children[1], funcScopePtr, localStartOffset);
        }
        
        int localsTotalSize = (localStartOffset == -8) ? 0 : -(localStartOffset + 8);
        
        // Frame size calculation for 16-byte alignment:
        // (rsp after `sub rsp, X` and pushing callee-saved regs must be 16-byte aligned)
        // X = frameSizeForSub = (aligned_locals_size + padding_for_alignment_with_callee_saved)
        int callee_saved_bytes = 5 * 8; // rbx, r12, r13, r14, r15 (System V ABI)
        int total_stack_usage_by_locals_and_callee_saved = localsTotalSize + callee_saved_bytes;
        
        int padding_needed = (16 - (total_stack_usage_by_locals_and_callee_saved % 16)) % 16;
        
        addedFuncSym->frameSize = localsTotalSize + padding_needed;

        globalTable->children.push_back(std::move(functionScope)); 

    } else if (node->type == "Affect") {
        if (node->children.size() >= 2 && node->children[0]->type == "Identifier") {
            std::string varName = node->children[0]->value;
            if (currentScopeTable->scopeName == "global") { // True global variable
                if (!currentScopeTable->findImmediateSymbol(varName)) {
                    // TODO: Determine size based on type if possible, default to 8 bytes (qword)
                    int varSize = 8; 
                    VariableSymbol globalVar(varName, "auto" /*TODO: infer type*/, "variable", currentScopeTable->nextDataOffset, true);
                    currentScopeTable->addSymbol(globalVar);
                    currentScopeTable->nextDataOffset += varSize; // TODO: Align if necessary
                }
            } else {
                // Assignment to a local variable or parameter.
                // Declaration of locals is handled by discoverLocalsAndAssignOffsets.
                // This spot is more for type updates or semantic checks if 'Affect' is revisited after initial discovery.
                if (!currentScopeTable->findSymbol(varName)) {
                     m_errorManager.addError({"Undeclared identifier in assignment: ", varName, "Semantic", std::stoi(node->line)});
                }
            }
        }
    } else { // For other node types, recurse if they can contain definitions or scopes
        for (const auto& child : node->children) {
            // Pass currentScopeTable, or a new child scope if this node type creates one (e.g., a 'for' loop scope)
            buildScopesAndSymbols(child, globalTable, currentScopeTable);
        }
    }
}

void SymbolTableGenerator::discoverLocalsAndAssignOffsets(const std::shared_ptr<ASTNode>& bodyNode, SymbolTable* functionScopeTable, int& currentLocalOffset) {
    if (!bodyNode) return;

    if (bodyNode->type == "Affect") {
        if (bodyNode->children.size() >= 1 && bodyNode->children[0]->type == "Identifier") {
            std::string varName = bodyNode->children[0]->value;
            // Check if it's already a parameter or a previously declared local in this function
            if (!functionScopeTable->findImmediateSymbol(varName)) {
                VariableSymbol localVar(varName, "auto" /*TODO: infer type*/, "variable", currentLocalOffset, false);
                functionScopeTable->addSymbol(localVar);
                currentLocalOffset -= 8; // Next local goes further down the stack
            }
        }
    } else if (bodyNode->type == "For") { // Example: loop variable is also a local
        if (bodyNode->children.size() >= 1 && bodyNode->children[0]->type == "Identifier") {
            std::string loopVarName = bodyNode->children[0]->value;
            if (!functionScopeTable->findImmediateSymbol(loopVarName)) {
                 VariableSymbol loopVar(loopVarName, "auto", "loop variable", currentLocalOffset, false);
                 functionScopeTable->addSymbol(loopVar);
                 currentLocalOffset -= 8;
            }
        }
        // Loop body (e.g., bodyNode->children[2]) needs to be traversed for more locals
        if (bodyNode->children.size() > 2) { // Assuming body is child 2
             discoverLocalsAndAssignOffsets(bodyNode->children[2], functionScopeTable, currentLocalOffset);
        }
        // Also traverse iterable if it can contain expressions that declare things (unlikely for simple lang)
        if (bodyNode->children.size() > 1) {
             discoverLocalsAndAssignOffsets(bodyNode->children[1], functionScopeTable, currentLocalOffset);
        }
        return; // Handled children of 'For' specifically to ensure correct traversal order
    }
    // Add other statements that might declare local variables (e.g. explicit 'var x;' syntax)

    // Generic recursion for children of the current bodyNode
    for (const auto& child : bodyNode->children) {
        discoverLocalsAndAssignOffsets(child, functionScopeTable, currentLocalOffset);
    }
}


// findFunctionDefNode might be used by a semantic analyzer pass later.
// For symbol table generation, direct processing of FunctionDefinition nodes is preferred.
std::shared_ptr<ASTNode> SymbolTableGenerator::findFunctionDefNode(const std::shared_ptr<ASTNode>& astRoot, const std::string& funcName) {
    if (!astRoot) return nullptr;
    // This assumes all function definitions are direct children of astRoot or a specific "definitions" node.
    for (const auto& node : astRoot->children) {
        if (node->type == "FunctionDefinition" && node->value == funcName) {
            return node;
        }
    }
    return nullptr;
}
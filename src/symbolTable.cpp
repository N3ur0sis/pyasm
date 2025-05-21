#include "symbolTable.h"
#include "parser.h" // Assuming ASTNode might be fully defined here
#include "errorManager.h" // For m_errorManager
#include <sstream>
#include <set>
#include <algorithm> // For std::find_if
#include <functional> // For std::function

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
    
    // Créer d'abord le scope de la fonction pour pouvoir l'utiliser dans l'inférence de type
    auto functionScope = std::make_unique<SymbolTable>("function " + funcName, globalTable, nextTableIdCounter++);
    SymbolTable* funcScopePtr = functionScope.get();
    
    // Traiter les paramètres
    int paramOffset = 16; // First parameter at [RBP+16]
    if (node->children.size() > 0 && node->children[0]->type == "FormalParameterList") {
        for (const auto& paramNode : node->children[0]->children) {
            VariableSymbol param(paramNode->value, "auto", "parameter", false, paramOffset); 
            funcScopePtr->addSymbol(param);
            paramOffset += 8; 
        }
    }
    
    // Trouver les variables locales et leur attribuer des offsets
    int localStartOffset = -8;
    if (node->children.size() > 1 && node->children[1]->type == "FunctionBody") {
        discoverLocalsAndAssignOffsets(node->children[1], funcScopePtr, localStartOffset);
    }
    
    // Maintenant que le scope est construit, inférer le type de retour
    std::string returnType = inferFunctionReturnType(node, funcScopePtr);
    
    // Créer le symbole de fonction avec le type de retour inféré
    FunctionSymbol funcSym(funcName, returnType, numParams, 0, 0, 0);
    globalTable->addSymbol(funcSym);
    FunctionSymbol* addedFuncSym = dynamic_cast<FunctionSymbol*>(globalTable->findImmediateSymbol(funcName));
    
    if (!addedFuncSym) { return; }
    
    addedFuncSym->tableID = functionScope->tableID;
    
    // Calcul de la taille du frame
    int localsTotalSize = (localStartOffset == -8) ? 0 : -(localStartOffset + 8);
    int callee_saved_bytes = 5 * 8; // rbx, r12, r13, r14, r15
    int total_stack_usage = localsTotalSize + callee_saved_bytes;
    int padding_needed = (16 - (total_stack_usage % 16)) % 16;
    addedFuncSym->frameSize = localsTotalSize + padding_needed;
    
    globalTable->children.push_back(std::move(functionScope));
} else if (node->type == "Affect") {
        if (node->children.size() >= 2 && node->children[0]->type == "Identifier") {
            std::string varName = node->children[0]->value;
            std::shared_ptr<ASTNode> rhsNode = node->children[1];
            
            std::string rhsInferredType = "auto"; // Default
            if (rhsNode->type == "List") {
                rhsInferredType = "List";
            } else if (rhsNode->type == "String") { // Assuming AST node type "String" for string literals
                rhsInferredType = "String";
            } else if (rhsNode->type == "Integer") {
                rhsInferredType = "Integer";
            } else if (rhsNode->type == "True" || rhsNode->type == "False") {
                rhsInferredType = "Boolean";
            } else if (rhsNode->type == "Identifier") {
                Symbol* s = currentScopeTable->findSymbol(rhsNode->value); // findSymbol checks current and then parents
                if (s) {
                    if(auto vs = dynamic_cast<VariableSymbol*>(s)) rhsInferredType = vs->type;
                    else if(auto fs = dynamic_cast<FunctionSymbol*>(s)) rhsInferredType = fs->returnType; // type of a function name is its return type
                }
            } else if (rhsNode->type == "FunctionCall") {
                if (!rhsNode->children.empty() && rhsNode->children[0]->type == "Identifier") {
                    if (rhsNode->children[0]->value == "list") {
                        rhsInferredType = "List"; 
                    }
                    else if (rhsNode->children[0]->value == "len") {
                        rhsInferredType = "Integer"; 
                    }
                    else{
                        Symbol* s = globalTable->findSymbol(rhsNode->children[0]->value); 
                        if (s && dynamic_cast<FunctionSymbol*>(s)) {
                            rhsInferredType = dynamic_cast<FunctionSymbol*>(s)->returnType;
                        } else {
                            // If not in global, it might be a method or a nested function not yet fully supported for inference here
                            // For now, fallback if not found in global. A more complex lookup might be needed.
                            rhsInferredType = "auto"; // Or a more specific "unknown_function_return"
                        }
                    }
                }
            } else if (rhsNode->type == "ArithOp" || rhsNode->type == "TermOp" || rhsNode->type == "Compare" || rhsNode->type == "And" || rhsNode->type == "Or" || rhsNode->type == "Not" || rhsNode->type == "UnaryOp") {
                // For complex expressions, ideally, a full expression type inference would run.
                // For now, if it's an operation that results in boolean:
                if (rhsNode->type == "Compare" || rhsNode->type == "And" || rhsNode->type == "Or" || rhsNode->type == "Not") {
                    rhsInferredType = "Boolean";
                } else {
                    // For ArithOp, TermOp, UnaryOp, default to Integer or keep auto if operands are mixed/unknown
                    // This part can be expanded significantly. For the patch's purpose (l = [1,2,3,4]), this is less critical.
                    // A simple approach: if any operand is string for '+', it's string. If list for '+', it's list. Else integer.
                    // This is a placeholder for more robust expression type inference.
                    rhsInferredType = "Integer"; // Default for other expressions for now
                }
            }


            Symbol* symToUpdate = currentScopeTable->findSymbol(varName);
            if (symToUpdate) {
                if (auto vs = dynamic_cast<VariableSymbol*>(symToUpdate)) {
                    // Update type if current is 'auto' or if new type is more specific than 'auto'
                    if ((vs->type == "auto" && rhsInferredType != "auto") || (rhsInferredType != "auto")) {
                        vs->type = rhsInferredType;
                    }
                }
            } else {
                if (currentScopeTable->scopeName == "global") {
                    std::string newGlobalType = (rhsInferredType != "auto") ? rhsInferredType : "Integer";
                    VariableSymbol globalVar(varName, newGlobalType, "global", true, 0); 
                    currentScopeTable->addSymbol(globalVar);
                } else {
                     m_errorManager.addError({"Assignment to undeclared local variable (should be pre-declared by discoverLocals): ", varName, "Semantic", std::stoi(node->line)});
                }
            }
        }
    }else if (node->type == "For") {
    if (node->children.size() >= 1 && node->children[0]->type == "Identifier") {
        std::string loopVarName = node->children[0]->value;
        
        for (const auto& sym : currentScopeTable->symbols) {
            if (sym->name == loopVarName) {
                m_errorManager.addError({"Variable de boucle for masque une variable existante: ", 
                                         loopVarName, "Semantic", std::stoi(node->line)});
                break;
            }
        }

        bool isGlobal = (currentScopeTable->scopeName == "global");
        
        if (isGlobal) {
            int offset = currentScopeTable->nextDataOffset;
            VariableSymbol loopVar(loopVarName, "Integer", "variable", offset, true);
            currentScopeTable->addSymbol(loopVar);
            currentScopeTable->nextDataOffset += 8; 
        } else {
            int offset = -8; 
            for (const auto& sym : currentScopeTable->symbols) {
                if (auto vs = dynamic_cast<VariableSymbol*>(sym.get())) {
                    if (!vs->isGlobal && vs->offset <= offset) {
                        offset = vs->offset - 8;
                    }
                }
            }
            VariableSymbol loopVar(loopVarName, "Integer", "variable", offset, false);
            currentScopeTable->addSymbol(loopVar);
        }
        
        if (node->children.size() >= 2) {
            buildScopesAndSymbols(node->children[1], globalTable, currentScopeTable);
        }
        
        if (node->children.size() >= 3) {
            buildScopesAndSymbols(node->children[2], globalTable, currentScopeTable);
        }
    }else if (node->type == "ListCall") {
    const std::string& listName = node->children[0]->value;      // L in  L[i]
    if (Symbol* s = currentScopeTable->findSymbol(listName)) {
        if (auto *vs = dynamic_cast<VariableSymbol*>(s)) {
            vs->type = "List";                                    // promote to List
        }
    }
} else {
        m_errorManager.addError({"Boucle for mal formée, identificateur attendu: ", 
                                 "", "Semantic", std::stoi(node->line)});
    }
} 
    else { // For other node types, recurse if they can contain definitions or scopes
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
            std::shared_ptr<ASTNode> rhsNode = bodyNode->children[1];
            
            std::string rhsInferredType = "auto"; // Default
            if (rhsNode->type == "List") {
                rhsInferredType = "List";
            } else if (rhsNode->type == "String") { // Assuming AST node type "String" for string literals
                rhsInferredType = "String";
            } else if (rhsNode->type == "Integer") {
                rhsInferredType = "Integer";
            } else if (rhsNode->type == "True" || rhsNode->type == "False") {
                rhsInferredType = "Boolean";
            } else if (rhsNode->type == "Identifier") {
                Symbol* s = functionScopeTable->findSymbol(rhsNode->value); // findSymbol checks current and then parents
                if (s) {
                    if(auto vs = dynamic_cast<VariableSymbol*>(s)) rhsInferredType = vs->type;
                    else if(auto fs = dynamic_cast<FunctionSymbol*>(s)) rhsInferredType = fs->returnType; // type of a function name is its return type
                }
            } else if (rhsNode->type == "FunctionCall") {
                if (!rhsNode->children.empty() && rhsNode->children[0]->type == "Identifier") {
                    if (rhsNode->children[0]->value == "list") {
                        rhsInferredType = "List"; 
                    }
                    else if (rhsNode->children[0]->value == "len") {
                        rhsInferredType = "Integer"; 
                    }
                    else{
                        Symbol* s = functionScopeTable->parent->findSymbol(rhsNode->children[0]->value); 
                        if (s && dynamic_cast<FunctionSymbol*>(s)) {
                            rhsInferredType = dynamic_cast<FunctionSymbol*>(s)->returnType;
                        } else {
                            // If not in global, it might be a method or a nested function not yet fully supported for inference here
                            // For now, fallback if not found in global. A more complex lookup might be needed.
                            rhsInferredType = "auto"; // Or a more specific "unknown_function_return"
                        }
                    }
                }
            } else if (rhsNode->type == "ArithOp" || rhsNode->type == "TermOp" || rhsNode->type == "Compare" || rhsNode->type == "And" || rhsNode->type == "Or" || rhsNode->type == "Not" || rhsNode->type == "UnaryOp") {
                // For complex expressions, ideally, a full expression type inference would run.
                // For now, if it's an operation that results in boolean:
                if (rhsNode->type == "Compare" || rhsNode->type == "And" || rhsNode->type == "Or" || rhsNode->type == "Not") {
                    rhsInferredType = "Boolean";
                } else {
                    // For ArithOp, TermOp, UnaryOp, default to Integer or keep auto if operands are mixed/unknown
                    // This part can be expanded significantly. For the patch's purpose (l = [1,2,3,4]), this is less critical.
                    // A simple approach: if any operand is string for '+', it's string. If list for '+', it's list. Else integer.
                    // This is a placeholder for more robust expression type inference.
                    rhsInferredType = "Integer"; // Default for other expressions for now
                }
            }

            if (!functionScopeTable->findImmediateSymbol(varName)) {
                VariableSymbol localVar(varName, rhsInferredType /*TODO: infer type*/, "variable", false, currentLocalOffset); // Corrected: assign currentLocalOffset
                functionScopeTable->addSymbol(localVar);
                currentLocalOffset -= 8; // Next local goes further down the stack
            }
        }
    } else if (bodyNode->type == "For") { 
        if (bodyNode->children.size() >= 1 && bodyNode->children[0]->type == "Identifier") {
            std::string loopVarName = bodyNode->children[0]->value;
            if (!functionScopeTable->findImmediateSymbol(loopVarName)) {
                 // VariableSymbol loopVar(loopVarName, "auto", "variable", false, currentLocalOffset); // Changed "loop variable" to "variable"
                 // The patch suggests "Integer" type for loop variable
                 VariableSymbol lv(loopVarName, "Integer", "variable", false, currentLocalOffset);
                 functionScopeTable->addSymbol(lv);
                 currentLocalOffset -= 8;
            }
        }
        // discover locals in the loop body as usual
        if (bodyNode->children.size() > 2 && bodyNode->children[2]) { // Assuming body is child 2
             discoverLocalsAndAssignOffsets(bodyNode->children[2], functionScopeTable, currentLocalOffset);
        }
        // Also traverse iterable if it can contain expressions that declare things (unlikely for simple lang)
        // The patch implies only the body needs discovery after loop var.
        // if (bodyNode->children.size() > 1) {
        //      discoverLocalsAndAssignOffsets(bodyNode->children[1], functionScopeTable, currentLocalOffset);
        // }
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

std::string SymbolTableGenerator::inferFunctionReturnType(const std::shared_ptr<ASTNode>& funcDefNode, SymbolTable* functionScope) {
    std::string inferredType = "autoFun"; // Default return type if no explicit return is found
    
    // Function to recursively find return statements in the function body
    std::function<void(const std::shared_ptr<ASTNode>&)> findReturns = 
        [&](const std::shared_ptr<ASTNode>& node) {
            if (!node) return;
            
            if (node->type == "Return") {
                // If return has no expression, it's a void return
                if (node->children.empty()) {
                    inferredType = "void";
                    return;
                }
                
                // Get the expression type from the return statement
                std::shared_ptr<ASTNode> returnExpr = node->children[0];
                std::string exprType = "auto"; // Default
                
                // Infer the type based on the return expression
                if (returnExpr->type == "List") {
                    exprType = "List";
                } else if (returnExpr->type == "String") {
                    exprType = "String";
                } else if (returnExpr->type == "Integer") {
                    exprType = "Integer";
                } else if (returnExpr->type == "True" || returnExpr->type == "False") {
                    exprType = "Boolean";
                } else if (returnExpr->type == "Identifier") {
                    // Get type from symbol table
                    Symbol* s = functionScope->findSymbol(returnExpr->value);
                    if (s) {
                        if (auto vs = dynamic_cast<VariableSymbol*>(s))
                            exprType = vs->type;
                        else if (auto fs = dynamic_cast<FunctionSymbol*>(s))
                            exprType = fs->returnType;
                    }
                } else if (returnExpr->type == "FunctionCall") {
                    if (!returnExpr->children.empty() && returnExpr->children[0]->type == "Identifier") {
                        // Handle built-in functions first
                        std::string funcName = returnExpr->children[0]->value;
                        if (funcName == "list") {
                            exprType = "List";
                        } else if (funcName == "len") {
                            exprType = "Integer";
                        } else {
                            // For user-defined functions, look up in the global scope
                            Symbol* s = functionScope->parent->findSymbol(funcName);
                            if (s && dynamic_cast<FunctionSymbol*>(s)) {
                                exprType = dynamic_cast<FunctionSymbol*>(s)->returnType;
                            }
                        }
                    }
                } else if (returnExpr->type == "ArithOp" || returnExpr->type == "TermOp") {
                    exprType = "Integer"; // Arithmetic operations typically return Integer
                } else if (returnExpr->type == "Compare" || returnExpr->type == "And" || 
                           returnExpr->type == "Or" || returnExpr->type == "Not") {
                    exprType = "Boolean"; // Logical operations return Boolean
                }
                
                // Update the inferred type if we found something more specific
                if (exprType != "auto" && inferredType == "autoFun") {
                    inferredType = exprType;
                } else if (exprType != "auto" && inferredType != exprType && inferredType != "autoFun") {
                    // Found multiple different return types - handling strategy:
                    // For simplicity, we'll use a common type or stick with the first one
                    // A more advanced implementation could use type hierarchies or union types
                    // For now, we'll keep the first non-auto type found
                }
            }
            
            // Recursively check all children for return statements
            for (const auto& child : node->children) {
                findReturns(child);
            }
        };
    
    // Start the recursive search for return statements in the function body
    if (funcDefNode->children.size() > 1 && funcDefNode->children[1]->type == "FunctionBody") {
        findReturns(funcDefNode->children[1]);
    }
    
    return inferredType;
}
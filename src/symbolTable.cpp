#include "symbolTable.h"
#include "parser.h" 
#include "errorManager.h" 
#include <sstream>
#include <set>
#include <algorithm> 
#include <functional> 


#define RED "\033[31m"
#define RESET "\033[0m"


void SymbolTable::addSymbol(const Symbol& symbol) {
    for (const auto& symPtr : symbols) {
        if (symPtr->name == symbol.name) {
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
        symClone = std::make_unique<Symbol>(symbol);
    }
    


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
        out << indentStr << "  " << s->category << " : " << s->name;
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
    nextTableIdCounter = 0; 
    processedFunctionNames.clear();
    auto globalTable = std::make_unique<SymbolTable>("global", nullptr, nextTableIdCounter++);
    
    if (root) { 
        for (const auto& childNode : root->children) {
            buildScopesAndSymbols(childNode, globalTable.get(), globalTable.get());
        }
    }

    inferTypes(root, globalTable.get());

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
            } else if (rhsNode->type == "String") { 
                rhsInferredType = "String";
            } else if (rhsNode->type == "Integer") {
                rhsInferredType = "Integer";
            } else if (rhsNode->type == "True" || rhsNode->type == "False") {
                rhsInferredType = "Boolean";
            } else if (rhsNode->type == "Identifier") {
                Symbol* s = currentScopeTable->findSymbol(rhsNode->value);
                if (s) {
                    if(auto vs = dynamic_cast<VariableSymbol*>(s)) rhsInferredType = vs->type;
                    else if(auto fs = dynamic_cast<FunctionSymbol*>(s)) rhsInferredType = fs->returnType; 
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

                            rhsInferredType = "auto"; 
                        }
                    }
                }
            } else if (rhsNode->type == "ArithOp" || rhsNode->type == "TermOp" || rhsNode->type == "Compare" || rhsNode->type == "And" || rhsNode->type == "Or" || rhsNode->type == "Not" || rhsNode->type == "UnaryOp") {

                if (rhsNode->type == "Compare" || rhsNode->type == "And" || rhsNode->type == "Or" || rhsNode->type == "Not") {
                    rhsInferredType = "Boolean";
                } else {
                    // Pour ArithOp et TermOp, déterminer le type en fonction des opérandes
                    rhsInferredType = "Integer"; // Type par défaut
                    
                    // Fonction récursive pour déterminer le type d'une expression
                    std::function<std::string(const std::shared_ptr<ASTNode>&)> inferExprType = 
                        [&](const std::shared_ptr<ASTNode>& expr) -> std::string {
                            if (!expr) return "auto";
                            
                            if (expr->type == "Integer") return "Integer";
                            if (expr->type == "String") return "String";
                            if (expr->type == "List") return "List";
                            if (expr->type == "True" || expr->type == "False") return "Boolean";
                            
                            if (expr->type == "Identifier") {
                                Symbol* s = currentScopeTable->findSymbol(expr->value);
                                if (s && dynamic_cast<VariableSymbol*>(s)) {
                                    return dynamic_cast<VariableSymbol*>(s)->type;
                                }
                                return "auto";
                            }
                            
                            if (expr->type == "FunctionCall" && !expr->children.empty() && 
                                expr->children[0]->type == "Identifier") {
                                std::string funcName = expr->children[0]->value;
                                if (funcName == "list") return "List";
                                if (funcName == "len") return "Integer";
                                
                                Symbol* s = globalTable->findSymbol(funcName);
                                if (s && dynamic_cast<FunctionSymbol*>(s)) {
                                    return dynamic_cast<FunctionSymbol*>(s)->returnType;
                                }
                            }
                            
                            // Cas récursifs pour les expressions
                            if ((expr->type == "ArithOp" || expr->type == "TermOp") && !expr->children.empty()) {
                                std::string leftType = "auto", rightType = "auto";
                                if (expr->children.size() > 0) leftType = inferExprType(expr->children[0]);
                                if (expr->children.size() > 1) rightType = inferExprType(expr->children[1]);
                                
                                // Priorité des types: List > String > Integer > auto
                                if (leftType == "List" || rightType == "List") {
                                    if (expr->value == "+") return "List"; // Concaténation de listes
                                    return "Integer";  // Autres opérations sur liste -> entier (comme len)
                                }
                                if (leftType == "String" || rightType == "String") {
                                    if (expr->value == "+") return "String"; // Concaténation de chaînes
                                    return "Integer";  // Autres opérations sur chaînes -> entier
                                }
                                if (leftType == "Integer" && rightType == "Integer") return "Integer";
                                
                                return "Integer"; // Par défaut, on suppose que c'est un entier
                            }
                            
                            if (expr->type == "UnaryOp" && !expr->children.empty()) {
                                // Pour les opérations unaires, le type est généralement conservé
                                return inferExprType(expr->children[0]);
                            }
                            
                            return "auto";
                        };
                    
                    // Analyser l'expression pour déterminer son type
                    rhsInferredType = inferExprType(rhsNode);
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
            vs->type = "List";                                    
        }
    }
} else {
        m_errorManager.addError({"Boucle for mal formée, identificateur attendu: ", 
                                 "", "Semantic", std::stoi(node->line)});
    }
} 
    else { 
        for (const auto& child : node->children) {
            buildScopesAndSymbols(child, globalTable, currentScopeTable);
        }
    }
}

void SymbolTableGenerator::discoverLocalsAndAssignOffsets(const std::shared_ptr<ASTNode>& bodyNode, SymbolTable* functionScopeTable, int& currentLocalOffset) {
    if (!bodyNode) return;

    if (bodyNode->type == "Affect") {
        if (bodyNode->children.size() >= 1 && bodyNode->children[0]->type == "Identifier") {
            std::string varName = bodyNode->children[0]->value;
            std::shared_ptr<ASTNode> rhsNode = bodyNode->children[1];
            
            std::string rhsInferredType = "auto";
            if (rhsNode->type == "List") {
                rhsInferredType = "List";
            } else if (rhsNode->type == "String") { 
                rhsInferredType = "String";
            } else if (rhsNode->type == "Integer") {
                rhsInferredType = "Integer";
            } else if (rhsNode->type == "True" || rhsNode->type == "False") {
                rhsInferredType = "Boolean";
            } else if (rhsNode->type == "Identifier") {
                Symbol* s = functionScopeTable->findSymbol(rhsNode->value);
                if (s) {
                    if(auto vs = dynamic_cast<VariableSymbol*>(s)) rhsInferredType = vs->type;
                    else if(auto fs = dynamic_cast<FunctionSymbol*>(s)) rhsInferredType = fs->returnType; 
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
                            rhsInferredType = "auto"; 
                        }
                    }
                }
            } else if (rhsNode->type == "ArithOp" || rhsNode->type == "TermOp" || rhsNode->type == "Compare" || rhsNode->type == "And" || rhsNode->type == "Or" || rhsNode->type == "Not" || rhsNode->type == "UnaryOp") {

                if (rhsNode->type == "Compare" || rhsNode->type == "And" || rhsNode->type == "Or" || rhsNode->type == "Not") {
                    rhsInferredType = "Boolean";
                } else {
                    rhsInferredType = "Integer"; 
                }
            }

            if (!functionScopeTable->findImmediateSymbol(varName)) {
                VariableSymbol localVar(varName, rhsInferredType, "variable", false, currentLocalOffset); 
                functionScopeTable->addSymbol(localVar);
                currentLocalOffset -= 8; // Next local goes further down the stack
            }
        }
    } else if (bodyNode->type == "For") { 
        if (bodyNode->children.size() >= 1 && bodyNode->children[0]->type == "Identifier") {
            std::string loopVarName = bodyNode->children[0]->value;
            if (!functionScopeTable->findImmediateSymbol(loopVarName)) {

                 VariableSymbol lv(loopVarName, "Integer", "variable", false, currentLocalOffset);
                 functionScopeTable->addSymbol(lv);
                 currentLocalOffset -= 8;
            }
        }

        if (bodyNode->children.size() > 2 && bodyNode->children[2]) { // Assuming body is child 2
             discoverLocalsAndAssignOffsets(bodyNode->children[2], functionScopeTable, currentLocalOffset);
        }

        return; 
    }

    for (const auto& child : bodyNode->children) {
        discoverLocalsAndAssignOffsets(child, functionScopeTable, currentLocalOffset);
    }
}


std::shared_ptr<ASTNode> SymbolTableGenerator::findFunctionDefNode(const std::shared_ptr<ASTNode>& astRoot, const std::string& funcName) {
    if (!astRoot) return nullptr;
    for (const auto& node : astRoot->children) {
        if (node->type == "FunctionDefinition" && node->value == funcName) {
            return node;
        }
    }
    return nullptr;
}

std::string SymbolTableGenerator::inferFunctionReturnType(const std::shared_ptr<ASTNode>& funcDefNode, SymbolTable* functionScope) {
    std::string inferredType = "autoFun"; 
    

    std::function<void(const std::shared_ptr<ASTNode>&)> findReturns = 
        [&](const std::shared_ptr<ASTNode>& node) {
            if (!node) return;
            
            if (node->type == "Return") {

                if (node->children.empty()) {
                    inferredType = "void";
                    return;
                }
            

                std::shared_ptr<ASTNode> returnExpr = node->children[0];
                std::string exprType = "auto";

                if (returnExpr->type == "List") {
                    exprType = "List";
                } else if (returnExpr->type == "String") {
                    exprType = "String";
                } else if (returnExpr->type == "Integer") {
                    exprType = "Integer";
                } else if (returnExpr->type == "True" || returnExpr->type == "False") {
                    exprType = "Boolean";
                } else if (returnExpr->type == "Identifier") {

                    Symbol* s = functionScope->findSymbol(returnExpr->value);
                    if (s) {
                        if (auto vs = dynamic_cast<VariableSymbol*>(s))
                            exprType = vs->type;
                        else if (auto fs = dynamic_cast<FunctionSymbol*>(s))
                            exprType = fs->returnType;
                    }
                } else if (returnExpr->type == "FunctionCall") {
                    if (!returnExpr->children.empty() && returnExpr->children[0]->type == "Identifier") {

                        std::string funcName = returnExpr->children[0]->value;
                        if (funcName == "list") {
                            exprType = "List";
                        } else if (funcName == "len") {
                            exprType = "Integer";
                        } else {
                            Symbol* s = functionScope->parent->findSymbol(funcName);
                            if (s && dynamic_cast<FunctionSymbol*>(s)) {
                                exprType = dynamic_cast<FunctionSymbol*>(s)->returnType;
                            }
                        }
                    }
                } else if (returnExpr->type == "ArithOp" || returnExpr->type == "TermOp") {
                    exprType = "Integer"; 
                } else if (returnExpr->type == "Compare" || returnExpr->type == "And" || 
                           returnExpr->type == "Or" || returnExpr->type == "Not") {
                    exprType = "Boolean";
                }
                
                if (exprType != "auto" && inferredType == "autoFun") {
                    inferredType = exprType;
                } else if (exprType != "auto" && inferredType != exprType && inferredType != "autoFun") {

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


std::string SymbolTableGenerator::statementInference(const std::shared_ptr<ASTNode>& def, SymbolTable* globalTable, const std::shared_ptr<ASTNode>& node, SymbolTable* currentTable, std::string type) {
    std::cout << std::endl << node->type << " " << node->value << " " << type << std::endl;
    if (node->type == "Integer") return "Integer";
    else if (node->type == "String") return "String";
    else if (node->type == "List") {
        for (const auto& c : node->children) statementInference(def, globalTable, c, currentTable, "auto");
        return "List";
    }
    else if (node->type == "True" or node->type == "False") return "Boolean";
    else if (node->type == "Or" or node->type == "And") {
        statementInference(def, globalTable, node->children[0], currentTable, "auto");
        statementInference(def, globalTable, node->children[1], currentTable, "auto");
        return "Boolean";
    }
    else if (node->type == "Or" or node->type == "Not") {
        statementInference(def, globalTable, node->children[0], currentTable, "auto");
        return "Boolean";
    }
    else if (node->type == "None") return "auto";
    else if (node->type == "Identifier") {
        auto symb = dynamic_cast<VariableSymbol*>(currentTable->findSymbol(node->value));
        if (symb->type == "auto") symb->type = type;
        std::cout << node->value << " " << symb->type << std::endl;
        return symb->type;
    }
    else if (node->type == "TermOp") {
        statementInference(def, globalTable, node->children[0], currentTable, "Integer");
        statementInference(def, globalTable, node->children[1], currentTable, "Integer");
        return "Integer";
    }
    else if (node->type == "UnaryOp") {
        statementInference(def, globalTable, node->children[0], currentTable, "Integer");
        return "Integer";
    }
    else if (node->type == "ArithOp") {
        if (node->value == "+") {
            std::string ret1 = statementInference(def, globalTable, node->children[0], currentTable, type);
            std::string ret2 = statementInference(def, globalTable, node->children[1], currentTable, type);
            if (ret1 == "auto" and ret2 != "auto") {
                statementInference(def, globalTable, node->children[0], currentTable, ret2);
                return ret2;
            }
            else if (ret1 != "auto" and ret2 == "auto") {
                statementInference(def, globalTable, node->children[1], currentTable, ret1);
                return ret1;
            }
            else return ret1;
        }
        else {
            statementInference(def, globalTable, node->children[0], currentTable, "Integer");
            statementInference(def, globalTable, node->children[1], currentTable, "Integer");
            return "Integer";
        }
    }
    else if (node->type == "ListCall") {
        statementInference(def, globalTable, node->children[0], currentTable, "List");
        statementInference(def, globalTable, node->children[1], currentTable, "Integer");
        return "Integer";
    }
    else if (node->type == "Return") {
        if (!node->children.empty()) {
            std::string v = statementInference(def, globalTable, node->children[0], currentTable, "auto");
            if (v != "auto") {
                dynamic_cast<FunctionSymbol*>(globalTable->findSymbol(currentTable->scopeName.substr(9)))->returnType = v;
            }
        }
        return "auto";
    }
    else if (node->type == "Affect") {
        auto v = statementInference(def, globalTable, node->children[1], currentTable, "auto");
        statementInference(def, globalTable, node->children[0], currentTable, v);
        return v;
    }
    else if (node->type == "FunctionCall") {
        if (node->children[0]->value == "list") {
            for (const auto& param : node->children[1]->children) {
                statementInference(def, globalTable, param, currentTable, "auto");
            }
            return "List";
        }
        else if (node->children[0]->value == "range") {
            for (const auto& param : node->children[1]->children) {
                statementInference(def, globalTable, param, currentTable, "Integer");
            }
            return "List";
        }
        else if (node->children[0]->value == "len") {
            for (const auto& param : node->children[1]->children) {
                statementInference(def, globalTable, param, currentTable, "List");
            }
            return "Integer";
        }
        else {
            // obtention TDS de fonction
            SymbolTable* TDS;
            for (const auto& tds : globalTable->children) {
                if (tds->scopeName == "function " + node->children[0]->value) {
                    TDS = tds.get();
                    break;
                }
            }
            // obtention FunctionDefinition
            std::shared_ptr<ASTNode> FDEF;
            for (const auto& fdef : def->children) {
                if (fdef->value == node->children[0]->value) {
                    FDEF = fdef;
                    break;
                }
            }
            // parcours paramètres
            std::vector<std::string> param_types;
            bool CREATE_NEW = false;
            int i = 0;
            for (const auto& p : node->children[1]->children) {
                // évaluation
                std::string v = statementInference(def, globalTable, p, currentTable, "auto");
                param_types.push_back(v);

                if (dynamic_cast<VariableSymbol*>(TDS->findSymbol(FDEF->children[0]->children[i]->value))->type == "auto" and v != "auto")
                    CREATE_NEW = true;

                i++;
            }
            std::cout << "got here" << std::endl;
            // si AU MOINS UN param auto obtient qqch de mieux après résolution
            if (CREATE_NEW) {
                // check if function already created
                // parcours des fonctions
                for (const auto& fdef : def->children) {
                    // si même "lignée" de fonctions
                    if (fdef->children[0] == FDEF->children[0]) {
                        // obtenir la tds de fdef
                        SymbolTable* fdef_tds;
                        for (const auto& tds : globalTable->children) {
                            if (tds->scopeName == "function " + fdef->value) {
                                fdef_tds = tds.get();
                                break;
                            }
                        }
                        // comparer les types des paramètres
                        int i = 0;
                        bool all_good = true;
                        for (const auto& param_ident : fdef->children[0]->children) {
                            if ( dynamic_cast<VariableSymbol*>(fdef_tds->findSymbol(param_ident->value))->type != param_types[i]) {
                                all_good = false;
                                break;
                            }

                            i++;
                        }

                        if (all_good) {
                            // ça y est, on peut utiliser cette fonction directement (early return)
                            // simplement modifier l'appel actuel pour qu'il appelle cette fonction déjà existante
                            node->children[0]->value = fdef->value;
                            return dynamic_cast<FunctionSymbol*>(globalTable->findSymbol(fdef->value))->returnType;
                        }
                    }
                }
                std::cout << "got here!" << std::endl;
                // sinon création nouvelle fonction : modif AST
                // ajout FunctionDefinition
                std::string NEW_F_NAME = FDEF->value + std::to_string(nextTableIdCounter);
                auto new_def = std::make_shared<ASTNode>("FunctionDefinition", NEW_F_NAME);
                new_def->children.push_back(FDEF->children[0]);
                new_def->children.push_back(FDEF->children[1]);
                def->children.push_back(new_def);
                // modif FunctionCall
                node->children[0]->value = NEW_F_NAME;
                // copie TDS
                auto TDS_copy = std::make_unique<SymbolTable>("function " + NEW_F_NAME, globalTable, nextTableIdCounter++);
                for (const auto& symb : TDS->symbols) {
                    TDS_copy->symbols.push_back(dynamic_cast<VariableSymbol*>(symb.get())->clone());
                }
                // upgrade les types
                int i = 0;
                for (const auto& param : FDEF->children[0]->children) {
                    dynamic_cast<VariableSymbol*>(TDS_copy->findSymbol(param->value))->type = param_types[i];
                    i++;
                }
                // modifie la globalTable
                globalTable->children.push_back(std::move(TDS_copy));


                auto& tmp = globalTable->symbols;


                auto funcSymb = dynamic_cast<FunctionSymbol*>(globalTable->findSymbol(FDEF->value))->clone();
                funcSymb->name = NEW_F_NAME;
                tmp.insert(tmp.begin(), std::move(funcSymb)); // std::move à vérifier ?

                return dynamic_cast<FunctionSymbol*>(globalTable->findSymbol(NEW_F_NAME))->returnType;
            }
            else {
                return dynamic_cast<FunctionSymbol*>(globalTable->findSymbol(FDEF->value))->returnType;
            }
        }
    }
    else {
        for (const auto& c : node->children) {
            statementInference(def, globalTable, c, currentTable, "auto");
        }
        return "auto";
    }

}

// TODO penser à vérifier l'existence des enfants avant d'y accéder (pour graceful shutdown si l'input est incorrect)

void SymbolTableGenerator::inferTypes(const std::shared_ptr<ASTNode>& root, SymbolTable* globalTable) {

    // réinitialise les types
    for (const auto& symb : globalTable->symbols) {
        if (FunctionSymbol* v = dynamic_cast<FunctionSymbol*>(symb.get())) {
            v->returnType = "auto";
        }
        else {
            dynamic_cast<VariableSymbol*>(symb.get())->type = "auto";
        }
    }

    for (const auto& tds : globalTable->children) {
        for (const auto& symb : tds->symbols) {
            dynamic_cast<VariableSymbol*>(symb.get())->type = "auto";
        }
    }

    // 10 hard codé, peut être augmenté si nécessaire
    for (auto i = 0; i<5; ++i) {

        for (const auto& node : root->children[0]->children) {
            for (const auto& tds : globalTable->children) {
                if (tds->scopeName == "function " + node->value) {
                    statementInference(root->children[0], globalTable, node->children[1], tds.get(), "auto");
                    break;
                }
            }
        }

        statementInference(root->children[0], globalTable, root->children[1], globalTable, "auto");

    }

    // remplace les auto par autoFun
    for (const auto& symb : globalTable->symbols) {
        auto v = dynamic_cast<FunctionSymbol*>(symb.get());
        if (v and v->returnType == "auto") {
            v->returnType = "autoFun";
        }
    }

}


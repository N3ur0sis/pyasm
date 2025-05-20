#pragma once

#include <memory>
#include <string>
#include <set>
#include "parser.h"
#include "symbolTable.h"  // Ensure this is included
#include "errorManager.h"

class CodeGenerator {
public:
    explicit CodeGenerator(ErrorManager& errorManager) : m_errorManager(errorManager), symbolTable(nullptr), currentSymbolTable(nullptr), rootNode(nullptr), labelCounter(0), loopLabelCounter(0), ifLabelCounter(0), stringLabelCounter(0) {}
    
    // Updated to accept symbol table parameter
    void generateCode(const std::shared_ptr<ASTNode>& root, const std::string& filename, 
                      SymbolTable* symTable); // Made symTable non-optional for clarity

private:
    ErrorManager& m_errorManager;
    
    // Final assembly output is typically built up in sections
    // std::string asmCode; // This can be removed if textSection and dataSection are used to build final output

    // Accumulated code for different sections.
    std::string dataSection;
    std::string textSection; // Will also contain function definitions
    std::string bssSection; // Added for completeness, though not explicitly requested to be fixed.
    // Set to track variables already declared in the .data section (for true globals).
    std::set<std::string> declaredVars;
    
    // Symbol table pointers
    SymbolTable* symbolTable;       // Points to the global symbol table
    SymbolTable* currentSymbolTable; // Points to the symbol table of the current scope (e.g., function)
    
    std::shared_ptr<ASTNode> rootNode; // Root of the AST, useful for some lookups
    std::string currentFunction; // Name of the function currently being generated

    // Counters for generating unique labels
    int labelCounter;
    int loopLabelCounter;
    int ifLabelCounter;
    int stringLabelCounter;

    // Code generation routines.
    void startAssembly();
    void visitNode(const std::shared_ptr<ASTNode>& node);
    void endAssembly();
    void writeToFile(const std::string &filename);

    // Specific AST node type generators
    void genPrint(const std::string& type); // Assuming type is for the expression to print
    void genAffect(const std::shared_ptr<ASTNode>& node);
    void genFor(const std::shared_ptr<ASTNode>& node);
    void genIf(const std::shared_ptr<ASTNode>& node);
    void genFunction(const std::shared_ptr<ASTNode>& node);
    void genFunctionCall(const std::shared_ptr<ASTNode>& node);
    void genReturn(const std::shared_ptr<ASTNode>& node);
    void genList(const std::shared_ptr<ASTNode>& node); // For list literals or operations

    
    // Helper functions
    std::string getIdentifierMemoryOperand(const std::string& name); // Crucial for var access
    std::string getIdentifierType(const std::string& name); // To get type from SymbolTable
    std::string getExpressionType(const std::shared_ptr<ASTNode>& node); // To determine type of an expression node
    
    // Type system related helpers (review if still needed in this exact form or if SymbolTable handles more)
    void updateSymbolType(const std::string& name, const std::string& type); // Updates type in SymbolTable
    std::string inferFunctionReturnType(const std::shared_ptr<ASTNode>& astRoot, const std::string& funcName);
    // The following might be simplified or removed if SymbolTable directly provides this info
    //
	 bool isIntVariable(const std::string& name); // Can be: getIdentifierType(name) == "Integer"
     bool isStringVariable(const std::string& name); // Can be: getIdentifierType(name) == "String"
     void resetFunctionVarTypes(const std::string& funcName); // May not be needed with proper scoping
    void updateFunctionParamTypes(const std::string& funcName, const std::shared_ptr<ASTNode>& args); // Semantic analysis phase?
    void updateFunctionReturnType(const std::string& funcName, const std::string& returnType); // Semantic analysis phase?
    std::string getFunctionReturnType(const std::string& funcName); // Get from FunctionSymbol in SymbolTable

    std::string newLabel(const std::string& base);
    void toBool(const std::string& reg); // Converts value in reg to 0 or 1
};
#pragma once

#include <memory>
#include <string>
#include <set>
#include "parser.h"
#include "symbolTable.h"  // Add this include
#include "errorManager.h"

class CodeGenerator {
public:
    explicit CodeGenerator(ErrorManager& errorManager) : m_errorManager(errorManager) {}
    // Updated to accept symbol table parameter
    void generateCode(const std::shared_ptr<ASTNode>& root, const std::string& filename, 
                      SymbolTable* symTable = nullptr);

private:
    ErrorManager& m_errorManager;
    // Final assembly output
    std::string asmCode;

    // Accumulated code for the data section.
    std::string dataSection;
    // Accumulated code for the text section.
    std::string textSection;

    // Set to track variables already declared in the data section.
    std::set<std::string> declaredVars;
    std::string currentFunction;
    
    // Add symbol table as a member
    SymbolTable* symbolTable = nullptr;
    
    // Code generation routines.
    void startAssembly();
    void visitNode(const std::shared_ptr<ASTNode>& node);
    void endAssembly();
    void writeToFile(const std::string &filename);
    void genPrint();
    void genAffect(const std::shared_ptr<ASTNode>& node);
    void genFor(const std::shared_ptr<ASTNode>& node);
    void genIf(const std::shared_ptr<ASTNode>& node);
    void genFunction(const std::shared_ptr<ASTNode>& node);
    void genFunctionCall(const std::shared_ptr<ASTNode>& node);
    void genReturn(const std::shared_ptr<ASTNode>& node);
    void genList(const std::shared_ptr<ASTNode>& node);

    
    // Helper function
    std::string getIdentifierType(const std::string& name);
    bool isIntVariable(const std::string& name);
    bool isStringVariable(const std::string& name);
    void updateSymbolType(const std::string& name, const std::string& type);
    void resetFunctionVarTypes(const std::string& funcName);
    void updateFunctionParamTypes(const std::string& funcName, const std::shared_ptr<ASTNode>& args);
    void updateFunctionReturnType(const std::string& funcName, const std::string& returnType);
    std::string getFunctionReturnType(const std::string& funcName);

	std::string newLabel(const std::string& base);
	void toBool(const std::string& reg);
};
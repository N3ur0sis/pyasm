#pragma once

#include <memory>
#include <string>
#include <set>
#include "parser.h"

class CodeGenerator {
public:
    // Generates the complete assembly code from the AST and writes it to a file.
    void generateCode(const std::shared_ptr<ASTNode>& root, const std::string& filename);

private:
    // Final assembly output
    std::string asmCode;

    // Accumulated code for the data section.
    std::string dataSection;
    // Accumulated code for the text section.
    std::string textSection;

    // Set to track variables already declared in the data section.
    std::set<std::string> declaredVars;

    // Code generation routines.
    void startAssembly();
    void visitNode(const std::shared_ptr<ASTNode>& node);
    void endAssembly();
    void writeToFile(const std::string &filename);
    void genPrint();
    void genAffect(const std::shared_ptr<ASTNode>& node);
};

#include "codeGenerator.h"

    void CodeGenerator::generateCode(const std::shared_ptr<ASTNode>& root, SymbolTable* globalTable, const std::string& filename){
        
        //Assembly header (section .data, _start)
        startAssembly();
        // Traverse the AST
        visitNode(root);
        //Assembly footer (exit, ..)
        endAssembly();
        // Utility : write to file
        writeToFile(filename);
        (void)globalTable;
    }

    void CodeGenerator::startAssembly()
    {
    }

    void CodeGenerator::visitNode(const std::shared_ptr<ASTNode> &root)
    {
        (void)root;
    }

    void CodeGenerator::endAssembly()
    {
    }

    void CodeGenerator::writeToFile(const std::string &filename)
    {
        (void)filename;
    }

    void CodeGenerator::genPrint()
    {
    }

    void CodeGenerator::genAffect(const std::shared_ptr<ASTNode> &node)
    {
        (void)node;
    }

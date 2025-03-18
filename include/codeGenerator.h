#pragma once
#include "parser.h"
#include <string>
#include <memory>
#include "symbolTable.h"

class CodeGenerator{
    public:
        void generateCode(const std::shared_ptr<ASTNode>& root, SymbolTable* globalTable, const std::string& filename);

    private:
        std::string asmCode;

        /** CORE FUNCTIONS */
        void startAssembly();
        void visitNode(const std::shared_ptr<ASTNode>& root);
        void endAssembly();
        void writeToFile(const std::string& filename);

        /** GENREATION FUNCTIONS**/
        void genPrint();
        void genAffect(const std::shared_ptr<ASTNode>& node); 
};

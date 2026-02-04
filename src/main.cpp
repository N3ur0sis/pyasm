#include <iostream>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include "lexer.h"
#include "parser.h"
#include "errorManager.h"
#include "symbolTable.h"
#include "semanticAnalyzer.h"
#include "codeGenerator.h"

#define BOLD "\033[1m"
#define RESET "\033[0m"


int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <file>" << std::endl;
        return EXIT_FAILURE;
    }

    std::ifstream srcFile(argv[1]);
    if (!srcFile) {
        std::cerr << "Error reading source file" << std::endl;
        return EXIT_FAILURE;
    }

    ErrorManager errorManager;

    std::stringstream srcStream;
    srcStream << srcFile.rdbuf();
    Lexer lexer(srcStream.str(), errorManager);

    try {
        auto tokens = lexer.tokenize();
        Parser parser(tokens, errorManager);
        auto ast = parser.parse();
        if (!ast) {
            std::cerr << "Failed to parse input." << std::endl;
            return EXIT_FAILURE;
        }
        parser.generateDotFile(ast, "ast.dot");
        std::cout << "Abstract Syntax Tree:" << std::endl;
        parser.print(ast);

        SemanticAnalyzer semAnalyzer(errorManager);
        ast = semAnalyzer.firstPass(ast);
        parser.generateDotFile(ast, "ast2.dot");

        SymbolTableGenerator symGen(errorManager);
        auto symTable = symGen.generate(ast);

        // 1) Check for lexical/syntax errors or Symbol Table generation errors
        if (errorManager.hasErrors()) {
            std::cout << std::endl;
            errorManager.displayErrors();
        }
        // 2) Launch semantic analysis
        semAnalyzer.checkSemantics(ast, symTable.get());
        // 3) Check for semantic errors
        if (errorManager.hasErrors()) {
            std::cout << std::endl;
            errorManager.displayErrors();

        }
        
        std::cout << BOLD << "\nSymbol Table:" << RESET << std::endl;
        symTable->print(std::cout);

        // --- Code Generation Phase ---
        CodeGenerator codeGen(errorManager);
        // Generate the assembly code and write it to "output.asm"
        codeGen.generateCode(ast, "output.asm", symTable.get());
        if (errorManager.hasErrors()) {
            std::cout << std::endl;
            errorManager.displayErrors();
        }
        
        std::cout << "Assembly code generated in output.asm" << std::endl;
        //*/

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

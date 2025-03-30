#include <iostream>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include "lexer.h"
#include "parser.h"
#include "errorManager.h"
#include "symbolTable.h"
#include "semanticAnalyzer.h"
#include "codeGenerator.h"  // Include the code generator header

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
        lexer.displayTokens(tokens); // Display tokens for debugging

        Parser parser(tokens, errorManager);
        auto ast = parser.parse();
        if (!ast) {
            std::cerr << "Failed to parse input." << std::endl;
            return EXIT_FAILURE;
        }

        // Generate the AST dot file and print the AST to console
        parser.generateDotFile(ast, "ast.dot");
        std::cout << "Abstract Syntax Tree:" << std::endl;
        parser.print(ast);

        // Generate the symbol table
        SymbolTableGenerator symGen;
        auto symTable = symGen.generate(ast);

        // 1) Check for lexical/syntax errors
        if (errorManager.hasErrors()) {
            std::cout << std::endl;
            errorManager.displayErrors();
            return EXIT_FAILURE;
        }

        // 2) Launch semantic analysis
        {
            SemanticAnalyzer semAnalyzer(errorManager);
            semAnalyzer.checkSemantics(ast, symTable.get());
        }

        // 3) Check for semantic errors
        if (errorManager.hasErrors()) {
            std::cout << std::endl;
            errorManager.displayErrors();
            //return EXIT_FAILURE;
        }

        // Print the symbol table
        std::cout << "\nSymbol Table:" << std::endl;
        symTable->print(std::cout);

        std::cout << "\nNo semantic errors. Ready for code generation! :)\n" << std::endl;

        // --- Code Generation Phase ---
        CodeGenerator codeGen;
        // Generate the assembly code and write it to "output.asm"
        codeGen.generateCode(ast, "output.asm");
        std::cout << "Assembly code generated in output.asm" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

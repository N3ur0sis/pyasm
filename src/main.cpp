#include <iostream>
#include <fstream>
#include <sstream>
#include "lexer.h"
#include "parser.h"
#include "errorManager.h"
#include "symbolTable.h"   // Nouveau module pour les symboles

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
        lexer.displayTokens(tokens); // Affichage des tokens pour débogage
        Parser parser(tokens, errorManager);

        std::cout << std::endl;
        auto ast = parser.parse();
        if (ast) {
            std::cout << "Abstract Syntax Tree:" << std::endl;
            parser.generateDotFile(ast, "ast.dot");
            parser.print(ast);
        } else {
            std::cerr << "Failed to parse input." << std::endl;
        }

        // Génération de la table des symboles à partir de l'AST
        SymbolTableGenerator symGen;
        auto symTable = symGen.generate(ast);
        std::cout << "\nSymbol Table:" << std::endl;
        symTable->print(std::cout);

        if (errorManager.hasErrors()) {
            std::cout << std::endl;
            errorManager.displayErrors();
            return EXIT_FAILURE;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

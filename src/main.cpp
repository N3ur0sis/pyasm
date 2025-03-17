#include <iostream>
#include <fstream>
#include <sstream>
#include "lexer.h"
#include "parser.h"
#include "errorManager.h"
#include "symbolTable.h"
#include "semanticAnalyzer.h"

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
        auto ast = parser.parse();
        if (!ast) {
            std::cerr << "Failed to parse input." << std::endl;
            return EXIT_FAILURE;
        }

        // Affiche l'AST sur la console + dot
        parser.generateDotFile(ast, "ast.dot");
        std::cout << "Abstract Syntax Tree:" << std::endl;
        parser.print(ast);

        // Génère la table des symboles
        SymbolTableGenerator symGen;
        auto symTable = symGen.generate(ast);

        // === 1) Vérification des erreurs lexicales / syntaxiques existantes ===
        if (errorManager.hasErrors()) {
            std::cout << std::endl;
            errorManager.displayErrors();
            return EXIT_FAILURE;
        }

        // === 2) Lancement de l'analyse sémantique ===
        {
            SemanticAnalyzer semAnalyzer(errorManager);
            semAnalyzer.checkSemantics(ast, symTable.get());
        }

        // === 3) Vérification des erreurs sémantiques ===
        if (errorManager.hasErrors()) {
            std::cout << std::endl;
            errorManager.displayErrors();
            return EXIT_FAILURE;
        }

        // Affiche la table des symboles
        std::cout << "\nSymbol Table:" << std::endl;
        symTable->print(std::cout);

        // S'il n'y a pas d'erreur, on peut alors envisager la suite
        std::cout << "\nNo semantic errors. Ready for code generation! :)\n" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
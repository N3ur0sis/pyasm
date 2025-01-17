#include <iostream>
#include <fstream>
#include <sstream>
#include "lexer.h"
#include "parser.h"

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
        lexer.displayTokens(tokens); // Print tokens for debugging
        Parser parser(tokens, errorManager);

        auto ast = parser.parse();
        if (ast) {
            std::cout << "Abstract Syntax Tree:" << std::endl;
            parser.generateDotFile(ast, "ast.dot");
            parser.print(ast);
        } else {
            std::cerr << "Failed to parse input." << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
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

    std::stringstream srcStream;
    srcStream << srcFile.rdbuf();
    Lexer lexer(srcStream.str());

    try {
        // Tokenization step
        auto tokens = lexer.tokenize();
        lexer.displayTokens(tokens);

        // Parsing step
        Parser parser(tokens);
        if (parser.parse()) {
            std::cout << "Parsing completed successfully." << std::endl;
        } else {
            std::cerr << "Parsing failed." << std::endl;
            return EXIT_FAILURE;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
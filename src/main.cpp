/*
 * Created by Aymeric Robert on 12/10/2024.
 */

#include <iostream>
#include <fstream>
#include <sstream>

#include "lexer.h"




int main(int argc, char* argv[]){

    if(argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <file>" << std::endl;
        return EXIT_FAILURE;
    }

    // Read input file
    std::stringstream srcStream;
    {
        std::ifstream src(argv[1]);
        if (!src) {
            std::cerr << "Error reading source file" << std::endl;
            return EXIT_FAILURE;
        }
        srcStream << src.rdbuf();
    }

    Lexer lexer(srcStream.str());
    std::vector<Token> tokens = lexer.tokenize();

    for(auto &[type, value] : tokens) {
        if(type == TokenType::INTEGER) {
            std::cout << "Type : INTEGER" << ", Value :" << value << std::endl;
        }
        if(type == TokenType::KW_RETURN) {
            std::cout << "Type : RETURN"  << ", Value :" << value << std::endl;
        }
        if(type == TokenType::OP_PLUS) {
            std::cout << "Type : PLUS" << ", Value :" << value  << std::endl;
        }
        if(type == TokenType::IDF) {
            std::cout << "Type : IDF"  << ", Value :" << value << std::endl;
        }
        if(type == TokenType::NEWLINE) {
            std::cout << "Type : NEWLINE"  << ", Value :" << "NULL" << std::endl;
        }
        if(type == TokenType::BEGIN) {
            std::cout << "Type : BEGIN"  << ", Value :" << "NULL" << std::endl;
        }
        if(type == TokenType::END) {
            std::cout << "Type : END"  << ", Value :" << "NULL" << std::endl;
        }
    }

  return 0;
  }


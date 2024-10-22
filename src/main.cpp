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
    switch(type) {
        case TokenType::INTEGER:
            std::cout << "Type : INTEGER" << ", Value :" << value << std::endl;
            break;
        case TokenType::KW_RETURN:
            std::cout << "Type : RETURN" << ", Value :" << value << std::endl;
            break;
        case TokenType::OP_PLUS:
            std::cout << "Type : PLUS" << ", Value :" << value << std::endl;
            break;
        case TokenType::OP_MOD:
            std::cout << "Type : MOD" << ", Value :" << value << std::endl;
            break;
        case TokenType::OP_MUL:
            std::cout << "Type : MUL" << ", Value :" << value << std::endl;
            break;
        case TokenType::OP_MOINS:
            std::cout << "Type : MOINS" << ", Value :" << value << std::endl;
            break;
        case TokenType::OP_NEQ:
            std::cout << "Type : NEQ" << ", Value :" << value << std::endl;
            break;
        case TokenType::OP_LT:
            std::cout << "Type : LT" << ", Value :" << value << std::endl;
            break;
        case TokenType::OP_LE:
            std::cout << "Type : LE" << ", Value :" << value << std::endl;
            break;
        case TokenType::OP_GT:
            std::cout << "Type : GT" << ", Value :" << value << std::endl;
            break;
        case TokenType::OP_GE:
            std::cout << "Type : GE" << ", Value :" << value << std::endl;
            break;
        case TokenType::OP_AFF:
            std::cout << "Type : AFF" << ", Value :" << value << std::endl;
            break;
        case TokenType::OP_DIV:
            std::cout << "Type : DIV" << ", Value :" << value << std::endl;
            break;
        case TokenType::CAR_LPAREN:
            std::cout << "Type : LPAREN" << ", Value :" << value << std::endl;
            break;
        case TokenType::CAR_RPAREN:
            std::cout << "Type : RPAREN" << ", Value :" << value << std::endl;
            break;
        case TokenType::CAR_LBRACKET:
            std::cout << "Type : LBRACKET" << ", Value :" << value << std::endl;
            break;
        case TokenType::CAR_RBRACKET:
            std::cout << "Type : RBRACKET" << ", Value :" << value << std::endl;
            break;
        case TokenType::CAR_COMMA:
            std::cout << "Type : COMMA" << ", Value :" << value << std::endl;
            break;
        case TokenType::CAR_COLON:
            std::cout << "Type : COLON" << ", Value :" << value << std::endl;
            break;
        case TokenType::IDF:
            std::cout << "Type : IDF" << ", Value :" << value << std::endl;
            break;
        case TokenType::NEWLINE:
            std::cout << "Type : NEWLINE" << ", Value : NULL" << std::endl;
            break;
        case TokenType::BEGIN:
            std::cout << "Type : BEGIN" << ", Value : NULL" << std::endl;
            break;
        case TokenType::END:
            std::cout << "Type : END" << ", Value : NULL" << std::endl;
            break;
        case TokenType::STRING:
            std::cout << "Type : STRING" << ", Value :" << value << std::endl;
            break;
        case TokenType::ENDOFFILE:
            std::cout << "Type : ENDOFFILE" << ", Value : NULL" << std::endl;
            break;
        default:
            std::cout << "Type : UNKNOWN" << ", Value :" << value << std::endl;
            break;
    }
}


  return 0;
  }


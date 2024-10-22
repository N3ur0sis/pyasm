//
// Created by Aymeric Robert on 15/10/2024.
// Modified by Baptiste Pachot, Alexis Chavy and Johan Kaszczyk.
//
#pragma once
#include <iostream>
#include <stack>
#include <vector>
#include <unordered_map>
#include "pyasm.h"

/* */
enum class TokenType{
  INTEGER,
  IDF,
  OP_PLUS,
  OP_MINUS,
  OP_MUL,
  OP_MOD,
  OP_EQ,
  OP_NEQ,
  OP_LT,
  OP_LE,
  OP_GT,
  OP_GE,
  OP_AFF,
  OP_DIV,
  CAR_LPAREN,
  CAR_RPAREN,
  CAR_LBRACKET,
  CAR_RBRACKET,
  CAR_COMMA,
  CAR_COLON,
  KW_AND,
  KW_DEF,
  KW_ELSE,
  KW_FOR,
  KW_IF,
  KW_TRUE,
  KW_FALSE,
  KW_IN,
  KW_NOT,
  KW_OR,
  KW_PRINT,
  KW_RETURN,
  KW_NONE,
  NEWLINE,
  BEGIN,
  END,
  STRING,
  ENDOFFILE // EOF is already taken
};

struct Token{
  TokenType type;
  std::string value;
};

class Lexer {

  public:
    explicit Lexer(std::string src);

  std::vector<Token> tokenize();

  private:
    std::string m_src;
    std::unordered_map<std::string, TokenType> m_keywords;
    std::unordered_map<std::string, TokenType> m_ope_simple;
    std::unordered_map<std::string, TokenType> m_ope_double;
    std::unordered_map<std::string, TokenType> m_brackets;
    std::stack<int> m_scope;
    int m_pos = -1;
    int m_line = 1;

    char lookahead(const int ahead = 1) const {
      if(m_pos + ahead >= m_src.length()){
        return  {};
      }
      return m_src[m_pos + ahead];
    }
    char progress(){
      return m_src[++m_pos];
    }
};


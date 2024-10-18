//
// Created by Aymeric Robert on 15/10/2024.
//
#pragma once
#include <iostream>
#include <stack>
#include <vector>
#include <unordered_map>

enum class TokenType{
  INTEGER,
  IDF,
  OP_PLUS,
  OP_MOINS,
  OP_MUL,
  OP_MOD,
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
  END
};

struct Token{
  TokenType type;
  std::string value;
};




class Lexer {

  public:
  explicit Lexer(std::string src) : m_src(std::move(src)) {
    m_keywords = {
    {"and", TokenType::KW_AND},
    {"def", TokenType::KW_DEF},
    {"else", TokenType::KW_ELSE},
    {"for", TokenType::KW_FOR},
    {"if", TokenType::KW_IF},
    {"true", TokenType::KW_TRUE},
    {"false", TokenType::KW_FALSE},
    {"in", TokenType::KW_IN},
    {"not", TokenType::KW_NOT},
    {"or", TokenType::KW_OR},
    {"print", TokenType::KW_PRINT},
    {"return", TokenType::KW_RETURN},
    {"none", TokenType::KW_NONE},
    };
    m_ope_simple = {
      {"+", TokenType::OP_PLUS},
      {"*", TokenType::OP_MUL},
      {"%", TokenType::OP_MOD},
      {"-", TokenType::OP_MOINS}
    };
    m_scope.push(0);
  }

  std::vector<Token> tokenize(){
  std::vector<Token> tokens;

    std::string buffer;
    while(lookahead()){

      //Ident AND KeyWord
      if(std::isalpha(lookahead()) || lookahead() == '_'){
        buffer.push_back(progress());
        while(lookahead() && std::isalnum(lookahead()) || lookahead() == '_'){
          buffer.push_back(progress());
        }
        if(m_keywords.contains(buffer)){
          tokens.push_back({.type=m_keywords[buffer], .value=buffer });
          buffer.clear();
          continue;
        }
        tokens.push_back({.type=TokenType::IDF, .value=buffer });
        buffer.clear();
        continue;
      }

      //Integer
      else if(std::isdigit(lookahead())){
        if(lookahead() == '0'){
          buffer.push_back(progress());
        }else{
          while(lookahead() && std::isdigit(lookahead())){
              buffer.push_back(progress());
            }
        }
        tokens.push_back({.type = TokenType::INTEGER, .value = buffer });
        buffer.clear();
      }

      //Op Simple
      else if(m_ope_simple.contains(std::string(1, lookahead()))){
          buffer.push_back(progress());
          tokens.push_back({.type=m_ope_simple[buffer], .value=buffer });
          buffer.clear();
          continue;
      }

      //Space
      else if(lookahead() == ' ' || lookahead() == '\t' || lookahead() == '\r'){
        progress();
        continue;
      }
    
      //Newline
      else if(lookahead() == '\n') {
        tokens.push_back({.type = TokenType::NEWLINE, .value = ""});
        progress();
        int n = 0;
        while(lookahead() == ' '){ progress(); n++;}
        if(n == m_scope.top()){}
        else if(n > m_scope.top()) {
          m_scope.push(n);
          tokens.push_back({.type = TokenType::BEGIN, .value = ""});
        }else if(n < m_scope.top()) {
          while(n < m_scope.top()) {
            m_scope.pop();
            tokens.push_back({.type = TokenType::END, .value = ""});
          }
          if(n != m_scope.top()) {
            std::cerr << "Lexer: Indentation Error!" << std::endl;
          }
        }
      }
      
      //Error
      else {
        std::cerr << "Unexpected character: " << lookahead() << "tu est con" << std::endl;
        exit(EXIT_FAILURE);
      }
    }
    return tokens;
  }

  private:
    std::string m_src;
    std::unordered_map<std::string, TokenType> m_keywords;
    std::unordered_map<std::string, TokenType> m_ope_simple;
    std::stack<int> m_scope;
    int m_pos = -1;

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



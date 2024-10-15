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
  KW_RETURN,
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
    m_keywords = {{"return", TokenType::KW_RETURN}};
    m_scope.push(0);
  }

  std::vector<Token> tokenize(){
    std::vector<Token> tokens;

    std::string buffer;
    while(lookahead()){
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
        //Repeat for all keyword
      }else if(std::isdigit(lookahead())){
        if(lookahead() == '0'){
          buffer.push_back(progress());
        }else{
          while(lookahead() && std::isdigit(lookahead())){
              buffer.push_back(progress());
            }
        }
        tokens.push_back({.type = TokenType::INTEGER, .value = buffer });
        buffer.clear();
      }else if(lookahead() == '+'){
        buffer.push_back(progress());
        tokens.push_back({.type = TokenType::OP_PLUS, .value = buffer });
        buffer.clear();
      }else if(lookahead() == ' ' || lookahead() == '\t' || lookahead() == '\r'){
        progress();
        continue;
      }else if(lookahead() == '\n') {
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
      }else {
        std::cerr << "Unexpected character: " << lookahead() << "tu est con" << std::endl;
        exit(EXIT_FAILURE);
      }
    }
    return tokens;
  }

  private:
    std::string m_src;
    std::unordered_map<std::string, TokenType> m_keywords;
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



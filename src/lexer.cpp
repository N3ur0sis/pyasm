//
// Created by Admin on 22/10/2024.
//

#include "lexer.h"

Lexer::Lexer(std::string src) : m_src(std::move(src))  {
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
      {"-", TokenType::OP_MINUS}
    };
    m_ope_double = {
      {"==", TokenType::OP_EQ},
      {"!=", TokenType::OP_NEQ},
      {"<=", TokenType::OP_LE},
      {">=", TokenType::OP_GE},
      {"//", TokenType::OP_DIV}
    };
    m_brackets = {
      {"(", TokenType::CAR_LPAREN},
      {")", TokenType::CAR_RPAREN},
      {"[", TokenType::CAR_LBRACKET},
      {"]", TokenType::CAR_RBRACKET},
      {",", TokenType::CAR_COMMA},
      {":", TokenType::CAR_COLON}
    };
    m_scope.push(0);

  }

std::vector<Token> Lexer::tokenize(){
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
          if(isalnum(lookahead())){
            std::cerr << "Lexer: Integers cannot start with zeros (line: " << m_line << ")" <<  std::endl;
            exit(1);
          }
        }else{
          while(lookahead() && std::isdigit(lookahead())){
              buffer.push_back(progress());
            }
          if(std::isalpha(lookahead())){
            std::cerr << "Lexer: Identifier cannot start with a digit (line: " << m_line << ")" <<  std::endl;
            exit(1);
          }else if(buffer.size() > 79){
            std::cerr << "Lexer: Identifier name too long (line: " << m_line << ")" <<  std::endl;
            exit(1);
          }
        }
        if(std::atoi(buffer.c_str()) < MAX_INT_VALUE){
          tokens.push_back({.type = TokenType::INTEGER, .value = buffer });
          buffer.clear();
        }else{
          std::cerr << "Lexer: Integer too long (line: " << m_line << ")" << std::endl;
          exit(1);
          }
      }

      //Op Simple sauf '=', '<', '>'
      else if(m_ope_simple.contains(std::string(1, lookahead()))){
          buffer.push_back(progress());
          tokens.push_back({.type=m_ope_simple[buffer], .value=buffer });
          buffer.clear();
          continue;
      }

      //Op Double et '=', '<', '>'
      else if(lookahead() && (lookahead() == '<' || lookahead() == '>' || lookahead() == '=')){
        buffer.push_back(progress());
        if(lookahead() == '='){
          buffer.push_back(progress());
          tokens.push_back({.type=m_ope_double[buffer], .value=buffer });
          buffer.clear();
          continue;
        }
        else if(isalnum(lookahead()) || isspace(lookahead())){
          tokens.push_back({.type=m_ope_simple[buffer], .value=buffer });
          buffer.clear();
          continue;
        }else{
          std::cerr << "Lexer : Expected identifier or numerical value but have " << lookahead() << " (line: " << m_line << ")" << std::endl;
          exit(EXIT_FAILURE);
        }
      }
      else if(lookahead() && lookahead() == '!'){
        buffer.push_back(progress());
        if(lookahead() && lookahead() == '='){
          buffer.push_back(progress());
          tokens.push_back({.type=TokenType::OP_NEQ, .value=buffer });
          buffer.clear();
          continue;
        }
        else{
          std::cerr << "Lexer : Expected '=' but have " << lookahead() << " (line: " << m_line << ")" << std::endl;
          exit(EXIT_FAILURE);
        }
      }
      else if(lookahead() && lookahead() == '/'){
        buffer.push_back(progress());
        if(lookahead() && lookahead() == '/'){
          buffer.push_back(progress());
          tokens.push_back({.type=TokenType::OP_DIV, .value=buffer });
          buffer.clear();
          continue;
        }
        else{
          std::cerr << "Lexer : Expected '/' but have " << lookahead() << " (line: " << m_line << ")" << std::endl;
          exit(EXIT_FAILURE);
        }
      }

      //brackets
      else if(m_brackets.contains(std::string(1, lookahead()))){
        buffer.push_back(progress());
        tokens.push_back({.type=m_brackets[buffer], .value=buffer });
        buffer.clear();
        continue;
      }



      //Newline
      else if(m_pos == -1 || lookahead() == '\n') {
        if (m_pos > -1){
          tokens.push_back({.type = TokenType::NEWLINE, .value = ""});
          m_line++;
          progress();
        }
        int n = 0;
        while(lookahead() == ' ' || lookahead() == '\t'){ progress(); n++;}
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
            std::cerr << "Lexer: Indentation Error! (line: " << m_line << ")" << std::endl;
            exit(EXIT_FAILURE);
          }
        }
      }

      //Space
      else if(lookahead() == ' ' || lookahead() == '\t' || lookahead() == '\r'){
        progress();
        continue;
      }

      // Strings
      else if (lookahead() == '"') {
        progress();
        while (true) {
          if (!(lookahead())) {
            std::cerr << "Lexer: Reached end of file without closing string (line: " << m_line << ")" << std::endl;
            exit(EXIT_FAILURE);
          }
          else if (lookahead() == '"') {
            progress();
            break;
          }
          else if (lookahead() == '\\') {
            progress();
            if (!(lookahead())) continue;
            else if (lookahead() == '"' || lookahead() == '\\') buffer.push_back(progress()); // manages "\\" in addition to what's required
            else if (lookahead() == 'n') {
              progress();
              buffer.push_back('\n');
            }
            else buffer.push_back('\\');
          }
          else buffer.push_back(progress());
        }
        tokens.push_back({.type = TokenType::STRING, .value = buffer});
        buffer.clear();
      }

      // Commentary
      else if (lookahead() == '#') {
        while (lookahead() && lookahead() != '\n') progress();
      }

      //Unknown Token
      else {
        std::cerr << "Lexer: Unexpected character: " << lookahead() << " (line:" << m_line << ")"  << std::endl;
        exit(EXIT_FAILURE);
      }

    }
    // EOF

    while(m_scope.top() != 0){
      m_scope.pop();
      tokens.push_back({.type = TokenType::END, .value = ""});
    }
    tokens.push_back({.type = TokenType::ENDOFFILE, .value = ""});
    return tokens;
  }



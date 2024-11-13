#pragma once
#include <vector>
#include "lexer.h"

class Parser {
public:
    explicit Parser(const std::vector<Token>& tokens);

    bool parse();

private:
    const std::vector<Token>& m_tokens;
    int m_pos;
    Token peek();
    Token next();
    bool match(TokenType type);

    // Grammar functions
    bool file();
    bool N();
    bool D();
    bool S();
    bool suite();
    bool stmt();
    bool stmt_seconde();
    bool simple_stmt();
    bool test();
    bool expr();
    bool or_expr();
    bool or_expr_prime();
    bool and_expr();
    bool and_expr_prime();
    bool comp_expr();
    bool comp_expr_prime();
    bool arith_expr();
    bool arith_expr_prime();
    bool term();
    bool term_prime();
    bool factor();
    bool unary();
    bool primary();
    bool expr_prime();
    bool I();
    bool E();
    bool E_prime();

    // Operators
    bool comp_op();
    bool arith_op();
    bool term_op();
};
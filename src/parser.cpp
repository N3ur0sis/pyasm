#include "parser.h"
#include <iostream>

// UTIL


Parser::Parser(const std::vector<Token>& tokens) : tokens(tokens), pos(0) {}

std::shared_ptr<ASTNode> Parser::parse() {
    return parseRoot();
}

void Parser::print(const std::shared_ptr<ASTNode>& node, int depth) {
    if (!node) return;
    std::cout << std::string(depth * 2, ' ') << node->type;
    if (!node->value.empty()) std::cout << ": " << node->value;
    std::cout << std::endl;
    for (const auto& child : node->children) print(child, depth + 1);
}

Token Parser::peek() {
    if (pos < tokens.size()) return tokens[pos];
    return {TokenType::ENDOFFILE, ""};
}

Token Parser::next() {
    return pos < tokens.size() ? tokens[pos++] : Token{TokenType::ENDOFFILE, ""};
}

bool Parser::expect(TokenType type) {
    if (peek().type == type) {
        next();
        return true;
    }
    return false;
}
bool Parser::expectR(TokenType type) {
    if (peek().type == type) {
        next();
        return true;
    }
    std::cerr << "Expected " << Lexer::tokenTypeToString(type) << std::endl;
    return false;
}
void Parser::skipNewlines() {
    while (peek().type == TokenType::NEWLINE) next();
}


//FONCTIONS PARSEUR 


// file -> N D stmt S eof .                                         ?????????
std::shared_ptr<ASTNode> Parser::parseRoot() {
    auto root = std::make_shared<ASTNode>("Program");
    while (peek().type != TokenType::ENDOFFILE) {
        skipNewlines();
        auto expr = parseExpr();
        if (expr) root->children.push_back(expr);
        skipNewlines();
    }
    return root;
}

// expr -> or_expr .
std::shared_ptr<ASTNode> Parser::parseExpr() {
    return parseOrExpr();
}

// primary -> const . || ident expr_prime . || ( expr ) . || [ e ] . || not primary .
std::shared_ptr<ASTNode> Parser::parsePrimary() {
    Token tok = peek();
    if (expect(TokenType::INTEGER)) {
        return std::make_shared<ASTNode>("Integer", tok.value);
    }
    if (expect(TokenType::IDF)) {
        auto idNode = std::make_shared<ASTNode>("Identifier", tok.value); 
        if (expect(TokenType::CAR_LPAREN)) {
            auto funcCallNode = std::make_shared<ASTNode>("FunctionCall");
            funcCallNode->children.push_back(idNode);
            auto paramListNode = std::make_shared<ASTNode>("ParameterList");
            while (true) {
                auto exprNode = parseExpr();
                paramListNode->children.push_back(exprNode);
                if (!expect(TokenType::CAR_COMMA)) {
                    break;
                }
            }
            funcCallNode->children.push_back(paramListNode);
            expectR(TokenType::CAR_RPAREN);
            return funcCallNode;
        }
        return idNode;
    }
    if (expect(TokenType::CAR_LPAREN)) {
        auto expr = parseExpr();
        expectR(TokenType::CAR_RPAREN);
        return expr;
    }
    if (expect(TokenType::CAR_LBRACKET)) {
        auto expr = parseE();
        expectR(TokenType::CAR_RBRACKET);
        return expr;
    }
    if (expect(TokenType::KW_NOT)) {
        auto notNode = std::make_shared<ASTNode>("Not");
        notNode->children.push_back(parsePrimary());
        return notNode;
    }
    std::cerr << "Unexpected token: " << tok.value << std::endl;
    return nullptr;
}

// E -> expr E_prime . || .
std::shared_ptr<ASTNode> Parser::parseE() {
    auto exprNode = parseExpr(); 
    auto ePrimeNode = parseEPrime(); 
    auto listNode = std::make_shared<ASTNode>("List");
    listNode->children.push_back(exprNode);
    if (ePrimeNode) {
        auto currentNode = ePrimeNode;
        while (currentNode) {
            listNode->children.push_back(currentNode->children.front());  
            if (currentNode->children.size() > 1) {
                currentNode = currentNode->children[1];
            } else {
                currentNode = nullptr;
            } 
        }
    }
    return listNode;
}

// E_prime -> , expr E_prime . || .
std::shared_ptr<ASTNode> Parser::parseEPrime() {
    if (expect(TokenType::CAR_COMMA)) {
        auto exprNode = parseExpr(); 
        auto ePrimeNode = parseEPrime();  
        auto commaNode = std::make_shared<ASTNode>("EPrime");
        commaNode->children.push_back(exprNode);
        if (ePrimeNode) {
            commaNode->children.push_back(ePrimeNode);
        }
        return commaNode;
    }
    return nullptr;
}

// or_expr -> and_expr or_expr_prime .
std::shared_ptr<ASTNode> Parser::parseOrExpr() {
    auto left = parseAndExpr();
    while (expect(TokenType::KW_OR)) {
        auto opNode = std::make_shared<ASTNode>("Or");
        opNode->children.push_back(left);
        opNode->children.push_back(parseAndExpr());
        left = opNode;
    }
    return left;
}

// and_expr -> comp_expr and_expr_prime .
std::shared_ptr<ASTNode> Parser::parseAndExpr() {
    auto left = parseCompExpr();
    while (expect(TokenType::KW_AND)) {
        auto opNode = std::make_shared<ASTNode>("And");
        opNode->children.push_back(left);
        opNode->children.push_back(parseCompExpr());
        left = opNode;
    }
    return left;
}

// comp_expr -> arith_expr comp_expr_prime .
std::shared_ptr<ASTNode> Parser::parseCompExpr() {
    auto left = parseArithExpr();
    if (peek().type == TokenType::OP_EQ || peek().type == TokenType::OP_NEQ ||
        peek().type == TokenType::OP_LE || peek().type == TokenType::OP_GE) {
        auto compOp = next();
        auto opNode = std::make_shared<ASTNode>("Compare", compOp.value);
        opNode->children.push_back(left);
        opNode->children.push_back(parseArithExpr());
        return opNode;
    }
    return left;
}

// arith_expr -> term arith_expr_prime .
std::shared_ptr<ASTNode> Parser::parseArithExpr() {
    auto left = parseTerm();
    while (peek().type == TokenType::OP_PLUS || peek().type == TokenType::OP_MINUS) {
        auto arithOp = next();
        auto opNode = std::make_shared<ASTNode>("ArithOp", arithOp.value);
        opNode->children.push_back(left);
        opNode->children.push_back(parseTerm());
        left = opNode;
    }
    return left;
}

// term -> factor term_prime .
std::shared_ptr<ASTNode> Parser::parseTerm() {
    auto left = parseFactor();
    while (peek().type == TokenType::OP_MUL || peek().type == TokenType::OP_DIV || peek().type == TokenType::OP_MOD) {
        auto termOp = next();
        auto opNode = std::make_shared<ASTNode>("TermOp", termOp.value);
        opNode->children.push_back(left);
        opNode->children.push_back(parseFactor());
        left = opNode;
    }
    return left;
}

// factor -> unary primary .
std::shared_ptr<ASTNode> Parser::parseFactor() {
    if (expect(TokenType::OP_MINUS)) {
        auto opNode = std::make_shared<ASTNode>("UnaryOp", "-");
        opNode->children.push_back(parsePrimary());
        return opNode;
    }
    return parsePrimary();
}

/*
// test -> "=" expr .   # Locally Implemented in parseSimpleStmt
// test -> expr_prime term_prime arith_expr_prime comp_expr_prime and_expr_prime or_expr_prime .
std::shared_ptr<ASTNode> Parser::parseTest(){
    auto nodeExpr = parseExprPrime();
    auto nodeTerm = parseTermPrime();
    auto nodeArith = parseArithExprPrime();
    auto nodeComp = parseCompExprPrime();
    auto nodeAnd = parseAndExprPrime();
    auto nodeOr = parseOrExprPrime();
}
*/

// expr_prime -> "(" E ")" . || .
std::shared_ptr<ASTNode> Parser::parseExprPrime() {
    if (expect(TokenType::CAR_LPAREN)) {
        auto exprNode = parseE();
        expectR(TokenType::CAR_RPAREN);
        return exprNode;
    }
    return nullptr;
}


//simple_stmt -> ident test .
//simple_stmt -> "return" expr .
//simple_stmt -> "print" "(" expr ")" .
std::shared_ptr<ASTNode> Parser::parseSimpleStmt() {
    Token tok = peek();
    if (expect(TokenType::IDF)) {               
        auto idNode = std::make_shared<ASTNode>("Identifier", tok.value);
        if (expect(TokenType::OP_EQ)) {                                     // test -> "=" expr .
            auto opNode = std::make_shared<ASTNode>("Affect", "=");
            opNode->children.push_back(idNode);
            opNode->children.push_back(parseExpr());
            return opNode;
        }
        auto exprNode = parseTest();                       // test -> expr_prime term_prime arith_expr_prime comp_expr_prime and_expr_prime or_expr_prime .
        // ???? JE FAIS QUOI ICI ?????        
    }
    if (expect(TokenType::KW_RETURN)) {
        auto returnNode = std::make_shared<ASTNode>("Return");
        returnNode->children.push_back(parseExpr());
        return returnNode;
    }
    if (expect(TokenType::KW_PRINT)) {
        expectR(TokenType::CAR_LPAREN);
        auto printNode = std::make_shared<ASTNode>("Print");
        printNode->children.push_back(parseExpr());
        expectR(TokenType::CAR_RPAREN);
        return printNode;
    }
    std::cerr << "Unexpected token: " << tok.value << std::endl;
    return nullptr;
}


//Attend parseSuite() pour fonctionner :

/*
// stmt_seconde -> else ":" suite .
// stmt_seconde -> .
std::shared_ptr<ASTNode> Parser::parseStmtSeconde() {
    if (expect(TokenType::KW_ELSE)) {
        expectR(TokenType::CAR_COLON);
        auto elseNode = std::make_shared<ASTNode>("Else");
        elseNode->children.push_back(parseSuite());
        return elseNode;
    }
    return nullptr;
}
*/
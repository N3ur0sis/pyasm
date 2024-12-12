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


// file -> N D stmt S eof .                                          ????????????
std::shared_ptr<ASTNode> Parser::parseRoot() {
    auto root = std::make_shared<ASTNode>("Program");
    while (peek().type != TokenType::ENDOFFILE) {
        skipNewlines();
        auto expr = parseSimpleStmt();                     //Modifier ici l'entrée du parseur
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
    if (peek().type == TokenType::OP_EQ){
        expectR(TokenType::OP_EQ_EQ);
        auto comOpFalse = next();
        auto opNode = std::make_shared<ASTNode>("Compare", "==");
        opNode->children.push_back(left);
        opNode->children.push_back(parseArithExpr());
        return opNode;
    }
    if (peek().type == TokenType::OP_EQ_EQ || peek().type == TokenType::OP_NEQ ||
        peek().type == TokenType::OP_LE || peek().type == TokenType::OP_GE ||
        peek().type == TokenType::OP_LE_EQ || peek().type == TokenType::OP_GE_EQ) {
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


// expr_prime -> "(" E ")" . || .
std::shared_ptr<ASTNode> Parser::parseExprPrime() {
    if (expect(TokenType::CAR_LPAREN)) {
        auto exprNode = parseE();
        expectR(TokenType::CAR_RPAREN);
        return exprNode;
    }
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
        
        auto testNode = parseTest(idNode);
        return testNode;            // test -> expr_prime term_prime arith_expr_prime comp_expr_prime and_expr_prime or_expr_prime .
           
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



// test -> "=" expr .                                               # Locally Implemented in parseSimpleStmt
// test -> expr_prime term_prime arith_expr_prime comp_expr_prime and_expr_prime or_expr_prime .
std::shared_ptr<ASTNode> Parser::parseTest(const std::shared_ptr<ASTNode>& idNode) {
    auto testNode = std::make_shared<ASTNode>("Test");
    auto currentNode = idNode; // Commence avec l'identifiant fourni

    // Parsing de expr_prime
    // Si l'identifiant est suivi de parenthèses, il s'agit ici d'un appel de fonction
    if (peek().type == TokenType::CAR_LPAREN) {
        auto funcCallNode = std::make_shared<ASTNode>("FunctionCall");
        funcCallNode->children.push_back(idNode);

        // Parsing des paramètres
        auto paramListNode = std::make_shared<ASTNode>("ParameterList");
        expectR(TokenType::CAR_LPAREN);
        while (peek().type != TokenType::CAR_RPAREN) {
            auto exprNode = parseExpr();
            if (exprNode) {
                paramListNode->children.push_back(exprNode);
            }
            if (!expect(TokenType::CAR_COMMA)) break;
        }
        expectR(TokenType::CAR_RPAREN);

        funcCallNode->children.push_back(paramListNode);
        currentNode = funcCallNode;
    }

    // Parsing des opérations term_prime
    while (peek().type == TokenType::OP_MUL || peek().type == TokenType::OP_DIV || peek().type == TokenType::OP_MOD) {
        auto termOp = next();
        auto opNode = std::make_shared<ASTNode>("TermOp", termOp.value);

        // Le côté gauche de l'opération est le nœud courant
        opNode->children.push_back(currentNode);

        // Parsez le côté droit de l'opération
        opNode->children.push_back(parseTerm());

        // Le nouvel opérateur devient le nœud courant
        currentNode = opNode;
    }
    
    // Parsing des opérations arith_expr_prime
    if (peek().type == TokenType::OP_PLUS || peek().type == TokenType::OP_MINUS) {
        while (peek().type == TokenType::OP_PLUS || peek().type == TokenType::OP_MINUS) {
            auto arithOp = next();
            auto opNode = std::make_shared<ASTNode>("ArithOp", arithOp.value);

            // Le côté gauche de l'opération est le nœud courant
            opNode->children.push_back(currentNode);

            // Parsez le côté droit de l'opération
            opNode->children.push_back(parseTerm());

            // Le nouvel opérateur devient le nœud courant
            currentNode = opNode;
        }       
    }

    // Parsing des opérations comp_expr_prime
    if (peek().type == TokenType::OP_EQ_EQ || peek().type == TokenType::OP_NEQ ||
        peek().type == TokenType::OP_LE || peek().type == TokenType::OP_GE ||
        peek().type == TokenType::OP_LE_EQ || peek().type == TokenType::OP_GE_EQ) {
        
        auto compOp = next();
        auto opNode = std::make_shared<ASTNode>("Compare", compOp.value);

        opNode->children.push_back(currentNode);

        opNode->children.push_back(parseArithExpr());

        currentNode = opNode;
    }

    // Parsing des opérations and_expr_prime
    if (peek().type == TokenType::KW_AND) {   
        while (expect(TokenType::KW_AND)) {
            auto opNode = std::make_shared<ASTNode>("And");

            opNode->children.push_back(currentNode);

            opNode->children.push_back(parseCompExpr());

            currentNode = opNode;
        }
    }
    
    // Parsing des opérations or_expr_prime
    if (peek().type == TokenType::KW_OR) {
        while (expect(TokenType::KW_OR)) {
            auto opNode = std::make_shared<ASTNode>("Or");
            
            opNode->children.push_back(currentNode);
            
            opNode->children.push_back(parseAndExpr());
            
            currentNode = opNode;
        }
    }

    // Ajoutez le nœud final au nœud Test
    testNode->children.push_back(currentNode);              // On peut peut-être enlever ce noeud "Test"
    return testNode;
}
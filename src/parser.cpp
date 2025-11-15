#include "parser.h"
#include <iostream>
#include <fstream>
#include <sstream>


Parser::Parser(const std::vector<Token>& tokens, ErrorManager& errorManager) 
    : tokens(tokens), m_errorManager(errorManager), pos(0) {}

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
    if ( ((unsigned long)pos) < tokens.size()) return tokens[pos];
    return {TokenType::ENDOFFILE, ""};
}

Token Parser::next() {
    return ((unsigned long)pos) < tokens.size() ? tokens[pos++] : Token{TokenType::ENDOFFILE, ""};
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
    m_errorManager.addError(Error{"Expected ", Lexer::tokenTypeToString(type), "Syntax", peek().line});
    continueParsing();
    return false;
}
void Parser::skipNewlines() {
    while (peek().type == TokenType::NEWLINE) next();
}

void Parser::continueParsing(){
    while (peek().type != TokenType::ENDOFFILE && peek().type != TokenType::NEWLINE) {
        next();
    }
    if (peek().type == TokenType::ENDOFFILE && !EOF_bool) {
        EOF_bool = true;
        m_errorManager.addError(Error{
                "Missing newline at the end of the file",
                "",
                "Syntax",
                peek().line
                });
        return;
    }
}


//FONCTIONS PARSEUR 

// file -> N D stmt S eof .
// N -> NEWLINE .
// N -> .
// S -> stmt S .
// S -> .
// et un peu plus de tolérance avec les NEWLINES en trop que ce qui est rigoureusement spécifié dans la grammaire
std::shared_ptr<ASTNode> Parser::parseRoot() {
    auto root = std::make_shared<ASTNode>("Program");
    auto DEF = std::make_shared<ASTNode>("Definitions");
    root->children.push_back(DEF);
    auto OP = std::make_shared<ASTNode>("Instructions");
    root->children.push_back(OP);

    skipNewlines();
    auto def = parseDefinition();
    while ( def != nullptr ) {
        DEF->children.push_back(def);
        skipNewlines();
        def = parseDefinition();
    }

    skipNewlines();
    auto old_pos = pos-1;
    while (peek().type != TokenType::ENDOFFILE and old_pos < pos) {
        old_pos = pos;
        auto expr = parseStmt();
        if (expr) OP->children.push_back(expr);
        skipNewlines();
    }

    if (peek().type == TokenType::ENDOFFILE && !tokens.empty()) {
        if (tokens.size() > 1) {
            const auto& penultimateToken = tokens[tokens.size() - 2];
            if (penultimateToken.type != TokenType::NEWLINE && !EOF_bool) {
                EOF_bool = true;
                m_errorManager.addError(Error{
                "Missing newline at the end of the file",
                "",
                "Syntax",
                penultimateToken.line
                });
                continueParsing();
            }
        }
    }
    return root;
}

// D -> "def" ident "(" I ")" ":" suite_def D .
// D -> .
// I -> ident I_prime .
// I-> .
// I_prime -> "," ident I_prime .
// I_prime -> .
std::shared_ptr<ASTNode> Parser::parseDefinition() {
    if (expect(TokenType::KW_DEF)) {
        auto tok = peek();
        auto def_root = std::make_shared<ASTNode>("FunctionDefinition", tok.value);
        def_root->line = std::to_string(tok.line);
        if (tok.value == "list" || tok.value == "len" || tok.value == "range" || tok.value == "print") {
            m_errorManager.addError(Error{"Function name cannot be list, len, range or print (reserved names). Got: ", tok.value, "Semantic", tok.line});
        }
        expectR(TokenType::IDF);
        expectR(TokenType::CAR_LPAREN);
        auto formal_param_list = std::make_shared<ASTNode>("FormalParameterList");
        formal_param_list->line = std::to_string(peek().line); 
        tok = peek();
        if (expect(TokenType::IDF)) {
            auto idNode = std::make_shared<ASTNode>("Identifier", tok.value);
            idNode->line = std::to_string(tok.line);
            formal_param_list->children.push_back(idNode); 
            while (expect(TokenType::CAR_COMMA)) {
                tok = peek();
                expectR(TokenType::IDF);
                auto paramNode = std::make_shared<ASTNode>("Identifier", tok.value);
                paramNode->line = std::to_string(tok.line);
                formal_param_list->children.push_back(paramNode);
            }
        }
        expectR(TokenType::CAR_RPAREN);
        expectR(TokenType::CAR_COLON);
        def_root->children.push_back(formal_param_list);

        auto [defs,suite] = parseSuiteDef();
        defs->line = std::to_string(peek().line);
        defs->type = "Definitions";
        def_root->children.push_back(defs);
        suite->line = std::to_string(peek().line); // because of the news definitions, this value is incorrect, but i don't think it's used anyways so...
        suite->type = "FunctionBody";
        def_root->children.push_back(suite);

        return def_root;
    }
    else return nullptr;
}


// suite_def -> simple_stmt NEWLINE .
// suite_def -> NEWLINE BEGIN D stmt S END .
// toute la différence avec parseSuite tient dans ce petit D 
// idem, plus de tolérance pour les newlines que ce qui est rigoureusement spécifié
std::pair<std::shared_ptr<ASTNode>, std::shared_ptr<ASTNode>> Parser::parseSuiteDef() {
    auto suite_root = std::make_shared<ASTNode>("");
    auto DEF = std::make_shared<ASTNode>("");
    if (expect(TokenType::NEWLINE)) {
        skipNewlines();
        expectR(TokenType::BEGIN);

        skipNewlines();
        auto def = parseDefinition();
        while ( def != nullptr ) {
            DEF->children.push_back(def);
            skipNewlines();
            def = parseDefinition();
        }

        skipNewlines();
        auto old_pos = pos-1;
        while (peek().type != TokenType::END and old_pos < pos) {
            old_pos = pos;
            auto expr = parseStmt();
            if (expr) suite_root->children.push_back(expr);
            skipNewlines();
        }
        expectR(TokenType::END);
    }
    else {
        suite_root->children.push_back(parseSimpleStmt());
        if (!expect(TokenType::NEWLINE)) {
            m_errorManager.addError(Error{"Expected newline", "", "Syntax", peek().line});
            continueParsing();
        }
    }
    return {DEF, suite_root};
}

// suite -> simple_stmt NEWLINE .
// suite -> NEWLINE BEGIN stmt S END .
// idem, plus de tolérance pour les newlines que ce qui est rigoureusement spécifié
std::shared_ptr<ASTNode> Parser::parseSuite() {
    auto suite_root = std::make_shared<ASTNode>("");
    if (expect(TokenType::NEWLINE)) {
        skipNewlines();
        expectR(TokenType::BEGIN);
        skipNewlines();
        auto old_pos = pos-1;
        while (peek().type != TokenType::END and old_pos < pos) {
            old_pos = pos;
            auto expr = parseStmt();
            if (expr) suite_root->children.push_back(expr);
            skipNewlines();
        }
        expectR(TokenType::END);
    }
    else {
        suite_root->children.push_back(parseSimpleStmt());
        if (!expect(TokenType::NEWLINE)) {
            m_errorManager.addError(Error{"Expected newline", "", "Syntax", peek().line});
            continueParsing();
        }
    }
    return suite_root;
}

// expr -> or_expr .
std::shared_ptr<ASTNode> Parser::parseExpr() {
    return parseOrExpr();
}

// primary -> const . || ident expr_prime . || ( expr ) . || [ e ] . || not primary .
std::shared_ptr<ASTNode> Parser::parsePrimary() {
    Token tok = peek();
    if (expect(TokenType::INTEGER)) {
        auto node = std::make_shared<ASTNode>("Integer", tok.value);
        node->line = std::to_string(tok.line);
        return node;
    }
    if (expect(TokenType::STRING)) {
        auto node = std::make_shared<ASTNode>("String", tok.value);
        node->line = std::to_string(tok.line);
        return node;
    }
    if (expect(TokenType::KW_TRUE)) {
        auto node = std::make_shared<ASTNode>("True");
        node->line = std::to_string(tok.line);
        return node;
    }
    if (expect(TokenType::KW_FALSE)) {
        auto node = std::make_shared<ASTNode>("False");
        node->line = std::to_string(tok.line);
        return node;
    }
    if (expect(TokenType::KW_NONE)) {
        auto node = std::make_shared<ASTNode>("None");
        node->line = std::to_string(tok.line);
        return node;
    }
    if (expect(TokenType::IDF)) {
        auto idNode = std::make_shared<ASTNode>("Identifier", tok.value);
        idNode->line = std::to_string(tok.line);
        if (expect(TokenType::CAR_LPAREN)) {
            auto funcCallNode = std::make_shared<ASTNode>("FunctionCall");
            funcCallNode->line = idNode->line; 
            funcCallNode->children.push_back(idNode);
            auto paramListNode = std::make_shared<ASTNode>("ParameterList");
            paramListNode->line = std::to_string(peek().line);
            while (peek().type != TokenType::CAR_RPAREN) {
                auto exprNode = parseExpr();
                if (exprNode) {
                    paramListNode->children.push_back(exprNode);
                }
                if (!expect(TokenType::CAR_COMMA)) break;
            }
            funcCallNode->children.push_back(paramListNode);
            expectR(TokenType::CAR_RPAREN);
            return funcCallNode;
        }
        
        if (expect(TokenType::CAR_LBRACKET)){
            auto node = std::make_shared<ASTNode>("ListCall");
            node->line = std::to_string(tok.line);
            node->children.push_back(idNode);
            node->children.push_back(parseExpr());
            expectR(TokenType::CAR_RBRACKET);
            
            if (expect(TokenType::OP_EQ)) {                                     
                auto opNode = std::make_shared<ASTNode>("Affect", "=");
                opNode->line = std::to_string(tok.line);
                opNode->children.push_back(node);
                opNode->children.push_back(parseExpr());
                return opNode;
            }
            return node;
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
        notNode->line = std::to_string(tok.line);
        notNode->children.push_back(parsePrimary());
        return notNode;
    }


    m_errorManager.addError(Error{"Unexpected ", Lexer::tokenTypeToString(tok.type), "Syntax", tok.line});
    continueParsing();
    return nullptr;
}

// E -> expr E_prime . || .
std::shared_ptr<ASTNode> Parser::parseE() {
    auto listNode = std::make_shared<ASTNode>("List");
    listNode->line = std::to_string(peek().line);
    // E can be replaced with nothing!!
    if (peek().type == TokenType::CAR_RBRACKET) return listNode;
    auto exprNode = parseExpr(); 
    auto ePrimeNode = parseEPrime(); 
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
        commaNode->line = std::to_string(peek().line);
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
        opNode->line = std::to_string(peek().line);
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
        opNode->line = std::to_string(peek().line);
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
        opNode->line = std::to_string(comOpFalse.line);
        opNode->children.push_back(left);
        opNode->children.push_back(parseArithExpr());
        return opNode;
    }
    if (peek().type == TokenType::OP_EQ_EQ || peek().type == TokenType::OP_NEQ ||
        peek().type == TokenType::OP_LE || peek().type == TokenType::OP_GE ||
        peek().type == TokenType::OP_LE_EQ || peek().type == TokenType::OP_GE_EQ) {
        auto compOp = next();
        auto opNode = std::make_shared<ASTNode>("Compare", compOp.value);
        opNode->line = std::to_string(compOp.line);
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
        opNode->line = std::to_string(arithOp.line);
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
        opNode->line = std::to_string(termOp.line);
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
        opNode->line = std::to_string(peek().line);
        opNode->children.push_back(parsePrimary());
        return opNode;
    }
    return parsePrimary();
}


// expr_prime -> "(" E ")" . || .                                               # Probalement pas utilisée
std::shared_ptr<ASTNode> Parser::parseExprPrime() {
    if (expect(TokenType::CAR_LPAREN)) {
        auto exprNode = parseE();
        expectR(TokenType::CAR_RPAREN);
        return exprNode;
    }
    return nullptr;
}


//stmt -> simple_stmt NEWLINE .
//stmt -> if expr ":" suite stmt_seconde .
//stmt -> for ident in expr ":" suite .
//stmt -> while expr ":" suite .
std::shared_ptr<ASTNode> Parser::parseStmt() {
    Token tok = peek();
    if (expect(TokenType::KW_IF)) {
        auto ifNode = std::make_shared<ASTNode>("If");
        ifNode->line = std::to_string(tok.line);
        ifNode->children.push_back(parseExpr());
        expectR(TokenType::CAR_COLON);
        auto suite = parseSuite();
        suite->type = "IfBody";
        ifNode->children.push_back(suite);
        ifNode->children.push_back(parseStmtSeconde());
        return ifNode;
    }
    if (expect(TokenType::KW_FOR)) {
        auto forNode = std::make_shared<ASTNode>("For");
        forNode->line = std::to_string(tok.line);
        Token tok = peek();
        if (expect(TokenType::IDF)) {
            auto idNode = std::make_shared<ASTNode>("Identifier", tok.value);
            idNode->line = std::to_string(tok.line);
            forNode->children.push_back(idNode);
            expectR(TokenType::KW_IN);
            forNode->children.push_back(parseExpr());
            expectR(TokenType::CAR_COLON);
            auto suite = parseSuite();
            suite->type = "ForBody";
            forNode->children.push_back(suite);
            return forNode;
        }
        //std::cerr << "Unexpected token: " << tok.value << std::endl;
        m_errorManager.addError(Error{"Unexpected ", Lexer::tokenTypeToString(tok.type), "Syntax", tok.line});
        continueParsing();
        //m_errorManager.addError("Lexer: Unexpected token: " + tok.value + " (line:" + std::to_string(tok.line) + ")");
    }
    if (expect(TokenType::KW_WHILE)) {
        auto whileNode = std::make_shared<ASTNode>("While");
        whileNode->line = std::to_string(tok.line);
        whileNode->children.push_back(parseExpr());
        expectR(TokenType::CAR_COLON);
        auto suite = parseSuite();
        suite->type = "WhileBody";
        whileNode->children.push_back(suite);
        return whileNode;
    }
    auto simpleStmt = parseSimpleStmt();
    if (simpleStmt) {
        expectR(TokenType::NEWLINE);
        return simpleStmt;
    }
    //std::cerr << "Unexpected token: " << tok.value << std::endl;
    m_errorManager.addError(Error{"Unexpected ", Lexer::tokenTypeToString(tok.type), "Syntax", tok.line});
    continueParsing();
    //m_errorManager.addError("Lexer: Unexpected token: " + tok.value + " (line:" + std::to_string(tok.line) + ")");
    return nullptr;
}

// stmt_seconde -> else ":" suite .
// stmt_seconde -> .
std::shared_ptr<ASTNode> Parser::parseStmtSeconde() {
    if (expect(TokenType::KW_ELSE)) {
        expectR(TokenType::CAR_COLON);
        auto suite = parseSuite();
        suite->type = "ElseBody";
        return suite;
    }
    return nullptr;
}


// simple_stmt -> "print" "(" print_args ")"      (no NEWLINE here)
std::shared_ptr<ASTNode> Parser::parsePrint() {
    if (!expect(TokenType::KW_PRINT)) return nullptr;
    Token printTok = tokens[pos-1]; // Get the PRINT token for its line number
    expectR(TokenType::CAR_LPAREN);
    auto printNode = std::make_shared<ASTNode>("Print");
    printNode->line = std::to_string(printTok.line);
    if (peek().type != TokenType::CAR_RPAREN) {
        do {
            auto expr = parseExpr();
            if (expr) { // Ensure expression was parsed successfully
                printNode->children.push_back(expr);
            } else {
                // If parseExpr returns nullptr, it means an error occurred during parsing the argument.
                // The error should have been reported by parseExpr or its callees.
                // We might want to stop parsing this print statement or try to recover.
                // For now, we break, assuming expectR(CAR_RPAREN) will catch further issues.
                break; 
            }
        } while (expect(TokenType::CAR_COMMA));
    }
    expectR(TokenType::CAR_RPAREN);
    return printNode;
}


//simple_stmt -> ident test .
//simple_stmt -> "return" expr .
//simple_stmt -> "print" "(" expr ")" .
//simple_stmt -> "-" indent expr_prime term_prime arith_expr_prime comp_expr_prime and_expr_prime or_expr_prime .
std::shared_ptr<ASTNode> Parser::parseSimpleStmt() {
    Token tok = peek();
    if (tok.type == TokenType::KW_PRINT) { // Check for print keyword first
        return parsePrint();
    }
    if (expect(TokenType::IDF)) {               
        auto idNode = std::make_shared<ASTNode>("Identifier", tok.value);
        idNode->line = std::to_string(tok.line);
        if (expect(TokenType::OP_EQ)) {                                     // test -> "=" expr .
            auto opNode = std::make_shared<ASTNode>("Affect", "=");
            opNode->line = std::to_string(tok.line);
            opNode->children.push_back(idNode);
            opNode->children.push_back(parseExpr());
            return opNode;
        }
        if (expect(TokenType::CAR_LBRACKET)){
            auto node = std::make_shared<ASTNode>("ListCall");
            node->line = std::to_string(tok.line);
            node->children.push_back(idNode);
            node->children.push_back(parseExpr());
            expectR(TokenType::CAR_RBRACKET);
            
            if (expect(TokenType::OP_EQ)) {                                     // test -> "=" expr .
                auto opNode = std::make_shared<ASTNode>("Affect", "=");
                opNode->line = std::to_string(tok.line);
                opNode->children.push_back(node);
                opNode->children.push_back(parseExpr());
                return opNode;
            }
            return node;
        }
        
        auto testNode = parseTest(idNode);
        return testNode;            // test -> expr_prime term_prime arith_expr_prime comp_expr_prime and_expr_prime or_expr_prime .
           
    }
    if (expect(TokenType::KW_RETURN)) {
        auto returnNode = std::make_shared<ASTNode>("Return");
        returnNode->line = std::to_string(tok.line);
        returnNode->children.push_back(parseExpr());
        return returnNode;
    }
    if (expect(TokenType::OP_MINUS)) {
        auto defNode = std::make_shared<ASTNode>("Negative", "-");
        defNode->line = std::to_string(tok.line);
        tok = peek();
        if (expect(TokenType::IDF)) {
            auto idNode = std::make_shared<ASTNode>("Identifier", tok.value);
            idNode->line = std::to_string(tok.line);
            auto testNode = parseTest(idNode);
            defNode->children.push_back(testNode);
            return defNode;
        }
        //std::cerr << "Unexpected token: " << tok.value << std::endl;
        m_errorManager.addError(Error{"Unexpected ", Lexer::tokenTypeToString(tok.type), "Syntax", tok.line});
        //m_errorManager.addError("Lexer: Unexpected token: " + tok.value + " (line:" + std::to_string(tok.line) + ")");
        return nullptr;
    }
    auto node = parseExpr();
    if(node) {
        return node;
    }
    //std::cerr << "Unexpected token: " << tok.value << std::endl;
    m_errorManager.addError(Error{"Unexpected ", Lexer::tokenTypeToString(tok.type), "Syntax", tok.line});
    continueParsing();
    //m_errorManager.addError("Lexer: Unexpected token: " + tok.value + " (line:" + std::to_string(tok.line) + ")");
    return nullptr;
}



// test -> "=" expr .                           
// test -> expr_prime term_prime arith_expr_prime comp_expr_prime and_expr_prime or_expr_prime .
// Fonction réutilisée pour compléter la 4e règle de simple_stmt
std::shared_ptr<ASTNode> Parser::parseTest(const std::shared_ptr<ASTNode>& idNode) {
    auto testNode = std::make_shared<ASTNode>("Test");
    testNode->line = std::to_string(peek().line);
    auto currentNode = idNode; // Commence avec l'identifiant fourni
    printf("Call of %s\n", idNode->value.c_str());
    // Parsing de expr_prime
    // Si l'identifiant est suivi de parenthèses, il s'agit ici d'un appel de fonction
    if (peek().type == TokenType::CAR_LPAREN) {
        auto funcCallNode = std::make_shared<ASTNode>("FunctionCall");
        funcCallNode->line = std::to_string(peek().line);
        funcCallNode->children.push_back(idNode);

        // Parsing des paramètres
        auto paramListNode = std::make_shared<ASTNode>("ParameterList");
        paramListNode->line = std::to_string(peek().line);
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
        opNode->line = std::to_string(termOp.line);

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
            opNode->line = std::to_string(arithOp.line);

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
        opNode->line = std::to_string(compOp.line);

        opNode->children.push_back(currentNode);

        opNode->children.push_back(parseArithExpr());

        currentNode = opNode;
    }

    // Parsing des opérations and_expr_prime
    if (peek().type == TokenType::KW_AND) {   
        while (expect(TokenType::KW_AND)) {
            auto opNode = std::make_shared<ASTNode>("And");
            opNode->line = std::to_string(peek().line);

            opNode->children.push_back(currentNode);

            opNode->children.push_back(parseCompExpr());

            currentNode = opNode;
        }
    }
    
    // Parsing des opérations or_expr_prime
    if (peek().type == TokenType::KW_OR) {
        while (expect(TokenType::KW_OR)) {
            auto opNode = std::make_shared<ASTNode>("Or");
            opNode->line = std::to_string(peek().line);
            
            opNode->children.push_back(currentNode);
            
            opNode->children.push_back(parseAndExpr());
            
            currentNode = opNode;
        }
    }

    // Ajoutez le nœud final au nœud Test
    testNode->children.push_back(currentNode);              // On peut peut-être enlever ce noeud "Test"
    return testNode->children.back();
}


void Parser::exportToDot(const std::shared_ptr<ASTNode>& node, std::ostream& out) {
    static int counter = 0;
    int currentId = counter++;
    
    // Écrire le nœud courant
    out << "  node" << currentId << " [label=\"" << node->type;
    if (!node->value.empty()) {
        out << "\\n" << node->value;
    }
    out << "\"];\n";

    // Écrire les relations parent-enfant
    for (const auto& child : node->children) {
        int childId = counter;
        if (child) {
            exportToDot(child, out);
            out << "  node" << currentId << " -> node" << childId << ";\n";
        }
    }
}

void Parser::generateDotFile(const std::shared_ptr<ASTNode>& root, const std::string& filename) {
    std::ofstream file(filename);
    if (!file) {
        throw std::runtime_error("Unable to open file for writing");
    }

    file << "digraph AST {\n";
    file << "  node [shape=box];\n";
    exportToDot(root, file);
    file << "}\n";
}

void Parser::handleInvalidNewlines(TokenType closingToken) {
    bool hasNewlines = false;
    while (peek().type == TokenType::NEWLINE) {
        hasNewlines = true;
        next();
    }
    if (hasNewlines) {
        m_errorManager.addError(Error{
            "Newlines are not allowed inside lists or parameter definitions.",
            "",
            "Syntax",
            peek().line
        });
        continueParsing();
    }
    while (peek().type != closingToken && peek().type != TokenType::ENDOFFILE) {
        next();
    }
    if (peek().type == closingToken) {
        next();
    }
    else{
        expectR(closingToken);
    }
}


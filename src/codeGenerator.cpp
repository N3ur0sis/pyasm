#include "codeGenerator.h"
#include <fstream>
#include <stdexcept>
#include <cstdlib>
#include <sstream>
#include <functional>

static std::string asmCode; // Stores the final assembly code

SymbolTable* currentSymbolTable = nullptr;
std::string currentFunction; // Keep track of the current function for return

// Helper function to get the memory operand for an identifier
// This function encapsulates the logic to access globals, parameters, or locals.
// It assumes the SymbolTable has been populated with correct offsets and categories.
std::string CodeGenerator::getIdentifierMemoryOperand(const std::string& name) {
    Symbol* sym = nullptr;
    SymbolTable* tableToSearch = currentSymbolTable; 

    // Robust symbol lookup: start from current scope and go up to global
    // SymbolTable::findSymbol should handle this hierarchical search.
    if (tableToSearch) {
        sym = tableToSearch->findSymbol(name); 
    }
    
    if (sym) {
        if (auto vs = dynamic_cast<VariableSymbol*>(sym)) {
            if (vs->isGlobal) {
                if (declaredVars.find(name) == declaredVars.end()) {
                    Symbol* funcCheckSym = symbolTable ? symbolTable->findImmediateSymbol(name) : nullptr;
                    if(!(funcCheckSym && funcCheckSym->symCat == "function")){
                        dataSection += name + ": dq 0\n";
                        declaredVars.insert(name);
                    }
                }
                return "qword [" + name + "]";
            } else if (vs->category == "parameter") {
                return "qword [rbp + " + std::to_string(vs->offset) + "]";
            } else { // Local variable (category == "variable" and isGlobal == false)
                // Local offsets are stored negative (e.g., -8, -16)
                if (vs->offset == 0) {
                     m_errorManager.addError({"Local variable has zero offset (error): ", name, "CodeGeneration", 0});
                     return "qword [rbp - 0]"; // Syntactically valid, but indicates an issue
                } else if (vs->offset > 0) {
                     // This should not happen for a local variable if offsets are assigned correctly.
                     m_errorManager.addError({"Local variable has positive offset (error): ", name, "CodeGeneration", 0});
                     // Attempt to generate a valid [rbp - offset] form, assuming it was meant to be negative
                     return "qword [rbp - " + std::to_string(vs->offset) + "]";
                }
                // If vs->offset is already negative, e.g., -8, this becomes "qword [rbp -8]"
                return "qword [rbp " + std::to_string(vs->offset) + "]";
            }
        } else if (sym->symCat == "function") {
            return name; // Function name used as a label
        }
    }

    // Fallback for undeclared identifiers - this is risky and should ideally be caught by semantic analysis
    m_errorManager.addError({"Undeclared identifier or symbol not found, assuming global: ", name, "CodeGeneration", 0});
    if (declaredVars.find(name) == declaredVars.end()) {
        // Avoid declaring function names as data if they exist as functions
        Symbol* funcCheckSym = symbolTable ? symbolTable->findSymbol(name) : nullptr; // Check all scopes for function
        if(!(funcCheckSym && funcCheckSym->symCat == "function")){
            dataSection += name + ": dq 0 ; Fallback global for undeclared " + name + "\n";
            declaredVars.insert(name);
        } else {
             return name; // It's an undeclared item but matches a function name, treat as label
        }
    }
    return "qword [" + name + "]"; // Fallback to global memory access
}


void CodeGenerator::generateCode(const std::shared_ptr<ASTNode>& root, const std::string& filename, SymbolTable* symTable) {
    this->symbolTable = symTable;
    this->currentSymbolTable = symbolTable; // Initially global
    this->rootNode = root;
    this->declaredVars.clear();
    // stringLabelCounter, etc., are already members and should be initialized in constructor or startAssembly

    // Call startAssembly to initialize .data, .bss sections and get initial .text headers.
    // startAssembly also clears textSection, dataSection, bssSection.
    startAssembly(); 

    std::string functionDefinitionsContent = ""; // Buffer for all function definitions
    std::string mainInstructionsContent = "";  // Buffer for main program instructions

    // First pass: Generate code for functions and main instructions into separate buffers.
    // This uses this->textSection as a temporary buffer for visitNode calls.
    for (const auto& childNodeOfProgram : root->children) {
        if (childNodeOfProgram->type == "Definitions") {
            for (const auto& definitionNode : childNodeOfProgram->children) {
                if (definitionNode->type == "FunctionDefinition") {
                    this->textSection = ""; // Clear member textSection to collect only this function's code
                    visitNode(definitionNode); // Function's code is now in this->textSection
                    functionDefinitionsContent += this->textSection; // Append to our dedicated function buffer
                }
            }
        } else { // Assuming this is the "Instructions" node or similar top-level executable block
            this->textSection = ""; // Clear member textSection to collect only main instructions
            visitNode(childNodeOfProgram); // Main instructions are now in this->textSection
            mainInstructionsContent += this->textSection; // Append to our dedicated main instructions buffer
        }
    }

    // Now, assemble the final this->textSection in the correct order.
    // Start with the headers already prepared by startAssembly() in the previous call,
    // or re-initialize if startAssembly() was changed to not set them globally.
    // For clarity, let's reconstruct textSection fully here.
    this->textSection = ""; // Start fresh for the final assembly of .text content
    this->textSection += "global _start\n";
    this->textSection += "\nsection .text\n";
    this->textSection += "_start:\n";                 // Define the _start label
    this->textSection += mainInstructionsContent;     // Add the main program instructions
    
    // endAssembly appends runtime helper functions (like print_number, exit syscall)
    // to the *end* of the current this->textSection.
    endAssembly(); 

    // Build the final assembly file content
    std::stringstream finalAsm;
    finalAsm << this->dataSection;      // dataSection was prepared by startAssembly()
    finalAsm << this->bssSection;       // bssSection was prepared by startAssembly()
    finalAsm << this->textSection;      // Contains headers, _start:, main code, runtime helpers
    
    if (!functionDefinitionsContent.empty()) {
        finalAsm << "\n; Functions\n";
        finalAsm << functionDefinitionsContent; // Add the collected function definitions
    }
    
    asmCode = finalAsm.str();
    writeToFile(filename);
}

void CodeGenerator::startAssembly() {
    this->textSection.clear();
    this->dataSection.clear();
    this->bssSection.clear();
    this->declaredVars.clear();
    this->labelCounter = 0;
    this->loopLabelCounter = 0;
    this->ifLabelCounter = 0;
    this->stringLabelCounter = 0;
    this->currentFunction.clear();

    // Initialize .data section
    this->dataSection += "section .data\n";
    this->dataSection += "    concat_buffer: times 2048 db 0\n";
    this->dataSection += "    concat_offset: dq 0\n";
    this->dataSection += "    div_zero_msg: db 'Error: Division by zero', 10, 0\n";
    this->dataSection += "    div_zero_len: equ $ - div_zero_msg\n";
    this->dataSection += "    list_buffer: times 8192 dq 0\n"; 
    this->dataSection += "    list_offset: dq 0\n";
    this->dataSection += "    open_bracket: db '['\n";
    this->dataSection += "    close_bracket: db ']'\n";
    this->dataSection += "    comma_space: db ',', 32\n";
    this->dataSection += "    index_error_msg: db 'Error: Index out of bounds', 10, 0\n";
    this->dataSection += "    index_error_len: equ $ - index_error_msg\n";
    this->dataSection += "    newline: db 10\n";
    this->dataSection += "    space: db 32\n";
    this->dataSection += "    minus_sign: db '-'\n";

    // Initialize .bss section
    this->bssSection += "\nsection .bss\n";
    this->bssSection += "    buffer: resb 32 ; For print_number\n";

    // Note: Initial .text headers ("global _start", "section .text") are now added
    // directly in generateCode when reconstructing the final textSection for clarity.
    // Alternatively, startAssembly could initialize textSection with these, and generateCode would append.
    // The chosen approach in the modified generateCode above is to build textSection from scratch there.
}

/* Génère un label unique */
std::string CodeGenerator::newLabel(const std::string& base) {
    static int ctr = 0;
    return "." + base + "_" + std::to_string(ctr++);
}

/* Met reg à 0/1 selon sa « véracité » ; ne détruit pas les drapeaux ZF/SF */
void CodeGenerator::toBool(const std::string& reg) {
    textSection += "    cmp " + reg + ", 0\n";   // ZF = 1 ssi reg == 0
    textSection += "    setne al\n";             // al = (reg != 0)
    textSection += "    movzx " + reg + ", al\n"; // reg = 0/1
}

void CodeGenerator::visitNode(const std::shared_ptr<ASTNode>& node) {
    if (!node) return;
    
    if (node->type == "Affect") {
        genAffect(node);
    } else if (node->type == "FunctionDefinition") {
        // Before processing children, set currentSymbolTable to this function's scope
        SymbolTable* previousTable = currentSymbolTable;
        // Find the specific function scope table created by SymbolTableGenerator
        // bool foundScope = false; // REMOVE/COMMENT OUT: This variable is set but not used.
        if (symbolTable) { // Check if global table exists
            for (const auto& childScope : symbolTable->children) { 
                if (childScope->scopeName == "function " + node->value) {
                    currentSymbolTable = childScope.get();
                    // foundScope = true; // REMOVE/COMMENT OUT
                    break;
                }
            }
        }
        // If the function scope wasn't found as a child of global (e.g. if currentSymbolTable was already a function scope - nested functions?)
        // This logic might need refinement based on how nested scopes are parented.
        // For now, assume functions are children of global or currentSymbolTable is already correctly set if nested.

        currentFunction = node->value;
        genFunction(node); // genFunction will use currentSymbolTable and currentFunction
        currentSymbolTable = previousTable; // Restore previous scope
        currentFunction = "";
    } else if (node->type == "FunctionCall") {
        genFunctionCall(node);
    } else if (node->type == "Return") {
        genReturn(node);
    } else if (node->type == "For") {
            genFor(node);
    } else if (node->type == "If") {
        genIf(node);
    } else if (node->type == "And") {
        std::string lblFalse = this->newLabel("and_false");
        std::string lblEnd   = this->newLabel("and_end");

        visitNode(node->children[0]);        // e1 -> rax
        textSection += "    mov rbx, rax\n"; // on garde e1 (au cas où)
        toBool("rax");
        textSection += "    cmp rax, 0\n";
        textSection += "    je  " + lblFalse + "\n"; // e1 est « falsy » ⇒ résultat = e1

        visitNode(node->children[1]);        // e1 était « truthy » → calcule e2 (résultat = e2)
        textSection += "    jmp " + lblEnd + "\n";

        textSection += lblFalse + ":\n";
        textSection += "    mov rax, rbx\n"; // résultat = e1
        textSection += lblEnd + ":\n";
    } else if (node->type == "Not") {
        visitNode(node->children[0]);        // valeur -> rax
        toBool("rax");
        textSection += "    xor rax, 1\n";   // inversion
    } else if (node->type == "Or") {
        std::string lblTrue = this->newLabel("or_true");
        std::string lblEnd  = this->newLabel("or_end");

        visitNode(node->children[0]);        // e1 -> rax
        textSection += "    mov rbx, rax\n";
        toBool("rax");
        textSection += "    cmp rax, 0\n";
        textSection += "    jne " + lblTrue + "\n"; // e1 est « truthy » ⇒ résultat = e1

        visitNode(node->children[1]);        // e1 était « falsy » → calcule e2
        textSection += "    jmp " + lblEnd + "\n";

        textSection += lblTrue + ":\n";
        textSection += "    mov rax, rbx\n"; // résultat = e1
        textSection += lblEnd + ":\n";
    } else if (node->type == "True") {
        textSection += "mov rax, 1\n";  
    } else if (node->type == "False") {
        textSection += "mov rax, 0\n";  
    } else if (node->type == "List") {
        genList(node);
    } else if (node->type == "String") {
            // static int strCounter = 0; // Make stringLabelCounter a member of CodeGenerator class
            std::string strLabel = "str_" + std::to_string(this->stringLabelCounter++); // Use member counter
            // dataSection += strLabel + ": db "; // This line is not needed if the next line includes the label
            
            std::string strValue = node->value;
            // Ensure quotes within the string are handled if necessary, though NASM usually handles simple strings well.
            // For example, if strValue could contain a quote, it would need escaping.
            this->dataSection += strLabel + ": db \"" + strValue + "\", 0\n"; // Use this->dataSection
            this->textSection += "    mov rax, " + strLabel + "\n"; // Use this->textSection
       
    } else if (node->type == "Print") { // Or "PrintStatement"
        if (!node->children.empty()) {
            std::shared_ptr<ASTNode> exprNodeToPrint = node->children[0];
            // Handle the AST structure: Print -> List -> ActualExpression
            if (exprNodeToPrint->type == "List" && !exprNodeToPrint->children.empty()) {
                exprNodeToPrint = exprNodeToPrint->children[0];
            }

            // Evaluate the expression, result in RAX
            visitNode(exprNodeToPrint); 

            // Determine type and call appropriate print function
            std::string resultType = getExpressionType(exprNodeToPrint); // Ensure getExpressionType is robust

            if (resultType == "Integer" || resultType == "Boolean") {
                this->textSection += "    call print_number\n";
            } else if (resultType == "String") {
                 this->textSection += "    call print_string\n"; 
            } else if (resultType == "List") {
                 // TODO: Implement a proper print_list function in assembly (added to endAssembly)
                 // This function would iterate through the list (using its size at base address)
                 // and call print_number, print_string, or print_list recursively for elements.
                 m_errorManager.addError({"List printing is rudimentary. Consider implementing print_list.", "", "CodeGeneration", std::stoi(node->line)});
                 this->textSection += "    ; RAX contains address of the list. Call a dedicated list printer here.\n";
                 this->textSection += "    ; Example: call print_list_custom\n";
                 this->textSection += "    call print_number ; Fallback: prints the list's base address as a number\n";
            } else if (resultType == "autoFun") {
                // Attempt to refine autoFun if possible, e.g. for known functions like fib
                if (exprNodeToPrint->type == "FunctionCall" && !exprNodeToPrint->children.empty() && exprNodeToPrint->children[0]->value == "fib") {
                    this->textSection += "    call print_number\n";
                } else {
                    m_errorManager.addError({"Printing result of a function with 'autoFun' type. Assuming integer.", resultType, "CodeGeneration", std::stoi(node->line)});
                    this->textSection += "    call print_number ; Fallback for autoFun, prints RAX as number\n";
                }
            }
            else { // Fallback for other unknown or unhandled types
                m_errorManager.addError({"Unhandled type for print: ", resultType, "CodeGeneration", std::stoi(node->line)});
                this->textSection += "    call print_number ; Fallback: prints RAX content as number\n";
            }

            // Print newline
            this->textSection += "    mov rax, 1\n";
            this->textSection += "    mov rdi, 1\n";
            this->textSection += "    mov rsi, newline\n";
            this->textSection += "    mov rdx, 1\n";
            this->textSection += "    syscall\n";
        }
    } else if (node->type == "Compare") {
        // Left
        visitNode(node->children[0]); 
        textSection += "push rax\n";  

        // Right
        visitNode(node->children[1]); 
        textSection += "mov rbx, rax\n";
        textSection += "pop rax\n";
        
        textSection += "cmp rax, rbx\n";
        
        if (node->value == "==") {
            textSection += "sete al\n";  
        } else if (node->value == "!=") {
            textSection += "setne al\n"; 
        } else if (node->value == "<") {
            textSection += "setl al\n"; 
        } else if (node->value == ">") {
            textSection += "setg al\n";  
        } else if (node->value == "<=") {
            textSection += "setle al\n"; 
        } else if (node->value == ">=") {
            textSection += "setge al\n"; 
        }
        textSection += "movzx rax, al\n"; // Correctly set rax to 0 or 1
    } else if (node->type == "UnaryOp") {
        if (node->children[0]->type == "Integer" || (node->children[0]->type == "Identifier" && isIntVariable(node->children[0]->value.c_str()))) {
            visitNode(node->children[0]);
            textSection += "neg rax\n";
        }
        else{
            m_errorManager.addError(Error{
                "Expected Int for an Unary Operation ; ", 
                "Got " + std::string(node->children[0]->type.c_str()), 
                "Semantics", 
                0
            });
        }
        
    
    } else if (node->type == "ArithOp") {
        if (node->value == "+") {
            static int opCounter = 0;
            std::string opId = std::to_string(opCounter++);
            std::string stringOpLabel = ".string_op_" + opId;
            std::string intOpLabel = ".int_op_" + opId;
            std::string endOpLabel = ".end_op_" + opId;
            
            // Evaluate the left child
            visitNode(node->children[0]);
            textSection += "push rax\n";
            
            // Evaluate the right child
            visitNode(node->children[1]);
            textSection += "mov rbx, rax\n";
            textSection += "pop rax\n";
            
            auto type0 = node->children[0]->type;
            if (node->children[0]->type == "Identifier") {
                type0 = getIdentifierType(node->children[0]->value);
            } else if (type0 == "FunctionCall") {
                type0 = inferFunctionReturnType(this->rootNode, node->children[0]->value);
                
            }

            auto type1 = node->children[1]->type;
            if (node->children[1]->type == "Identifier") {
                type1 = getIdentifierType(node->children[1]->value);
            }
            else if (type1 == "FunctionCall") {
                type1 = inferFunctionReturnType(this->rootNode, node->children[1]->value);
            }

            if (type0 == "auto") {
                m_errorManager.addError(Error{
                    "Undefined Variable; ", 
                    "Used " + std::string(node->children[0]->value.c_str()) + " before assignment",
                    "Semantics", 
                    std::stoi(node->line)
                });
            }
            if (type1 == "auto") {
                m_errorManager.addError(Error{
                    "Undefined Variable; ", 
                    "Used " + std::string(node->children[1]->value.c_str())+ " before assignment",
                    "Semantics", 
                    std::stoi(node->line)
                });
                return;
            }
            if (type0 != type1) {
                m_errorManager.addError(Error{
                    "Expected same type for an Arith Operation ; ", 
                    "Got " + std::string(type0.c_str()) + " and " + std::string(type1.c_str()), 
                    "Semantics", 
                    0
                });
                 return;

            }
            if (type0 != "Integer" && type0 != "String" && type0 != "List" && type0 != "auto") {
                m_errorManager.addError(Error{
                    "Expected Int or String for an Arith Operation ; ", 
                    "Got " + std::string(type0.c_str()), 
                    "Semantics", 
                    0
                });
            }
            if (type1 != "Integer" && type1 != "String" && type1 != "List" && type0 != "auto") {
                m_errorManager.addError(Error{
                    "Expected Int or String or List for an Arith Operation ; ", 
                    "Got " + std::string(type0.c_str()), 
                    "Semantics", 
                    0
                });
            }
            if (type0 == "String"){
                textSection += stringOpLabel + ":\n";
                textSection += "mov rdi, rax\n";
                textSection += "mov rsi, rbx\n";
                textSection += "call str_concat\n";
                
                textSection += endOpLabel + ":\n";
                return;
            }

            // D'abord vérifier si l'un des éléments est une liste
            textSection += "mov rcx, list_buffer\n";
            textSection += "cmp rax, rcx\n";
            textSection += "jl .not_list_left_" + opId + "\n";
            textSection += "cmp rbx, rcx\n";
            textSection += "jl .not_list_right_" + opId + "\n";
            
            // Les deux sont des listes, appeler list_concat
            textSection += "mov rdi, rax\n";   
            textSection += "mov rsi, rbx\n";    
            textSection += "call list_concat\n";
            textSection += "jmp " + endOpLabel + "\n";
            
            textSection += ".not_list_left_" + opId + ":\n";
            textSection += ".not_list_right_" + opId + ":\n";
            
            // CHeck if String
            textSection += "cmp rax, 10000\n";        
            textSection += "jge " + stringOpLabel + "\n";
            
            textSection += "cmp rbx, 10000\n";        
            textSection += "jge " + stringOpLabel + "\n";
            
            //IF int
            textSection += intOpLabel + ":\n";
            textSection += "add rax, rbx\n";
            textSection += "jmp " + endOpLabel + "\n";
            
            //IF String
            textSection += stringOpLabel + ":\n";
            textSection += "mov rdi, rax\n";
            textSection += "mov rsi, rbx\n";
            textSection += "call str_concat\n";
            
            textSection += endOpLabel + ":\n";
        }
        else if (node->value == "-") {
            visitNode(node->children[0]); 
            auto type0 = node->children[0]->type;
            if (type0 == "Identifier") {
                type0 = getIdentifierType(node->children[0]->value);
            }
            if (type0 != "Integer"){
                m_errorManager.addError(Error{
                    "Expected Int for Sub Operation ; ", 
                    "Got " + std::string(node->children[0]->type.c_str()), 
                    "Semantics", 
                    0
                });
            }
            textSection += "push rax\n";
            visitNode(node->children[1]); 
            auto type1 = node->children[1]->type;
            if (type1 == "Identifier") {
                type1 = getIdentifierType(node->children[1]->value);
            }
            if (type1 != "Integer"){
                m_errorManager.addError(Error{
                    "Expected Int for Sub Operation ; ", 
                    "Got " + std::string(node->children[1]->type.c_str()), 
                    "Semantics", 
                    0
                });
            }
            textSection += "mov rbx, rax\n";
                textSection += "pop rax\n";
                textSection += "sub rax, rbx\n";
        }
    
    } else if (node->type == "TermOp"){
        if (node->value == "*") {
            // Evaluate the left child
            if (node->children[0]->type == "Integer" || (node->children[0]->type == "Identifier" && this->isIntVariable(node->children[0]->value))) {
                visitNode(node->children[0]);
                textSection += "push rax\n";
            }
            else{
                m_errorManager.addError(Error{
                    "Expected Int for Mul Operation ; ", 
                    "Got " + std::string(node->children[0]->type.c_str()), 
                    "Semantics", 
                    0
                });
            }
            // Evaluate the right child
            if (node->children[1]->type == "Integer" || (node->children[1]->type == "Identifier" && this->isIntVariable(node->children[1]->value))) {
                visitNode(node->children[1]);
                textSection += "mov rbx, rax\n";
                textSection += "pop rax\n";
                textSection += "imul rax, rbx\n";
            }
            else{
                m_errorManager.addError(Error{
                    "Expected Int for Mul Operation ; ", 
                    "Got " + std::string(node->children[1]->type.c_str()), 
                    "Semantics", 
                    0
                });
            }
        }
        else if (node->value == "//") {
            // Evaluate the left child
            if (node->children[0]->type == "Integer" || (node->children[0]->type == "Identifier" && this->isIntVariable(node->children[0]->value))) {
                visitNode(node->children[0]);
                textSection += "push rax\n";
            }
            else{
                m_errorManager.addError(Error{
                    "Expected Int for Integer Division Operation ; ", 
                    "Got " + std::string(node->children[0]->type.c_str()), 
                    "Semantics", 
                    0
                });
            }

            // Evaluate the right child
            if (node->children[1]->type == "Integer" || (node->children[1]->type == "Identifier" && this->isIntVariable(node->children[1]->value))) {
                visitNode(node->children[1]);
                
                // Check if the right child is zero
                textSection += "cmp rax, 0\n";
                textSection += "je .division_by_zero_error\n";

                textSection += "mov rbx, rax\n";
                textSection += "pop rax\n";
                textSection += "xor rdx, rdx\n";  
                textSection += "div rbx\n";       
            }
            else{
                m_errorManager.addError(Error{
                    "Expected Int for Integer Division Operation ; ", 
                    "Got " + std::string(node->children[1]->type.c_str()), 
                    "Semantics", 
                    0
                });
            }    
        }
        else if (node->value == "%") {
            // Evaluate the left child
            if (node->children[0]->type == "Integer" || (node->children[0]->type == "Identifier" && this->isIntVariable(node->children[0]->value))) {
                visitNode(node->children[0]);
                textSection += "push rax\n";
            }
            else{
                m_errorManager.addError(Error{
                    "Expected Int for Modulo Operation ; ", 
                    "Got " + std::string(node->children[0]->type.c_str()), 
                    "Semantics", 
                    0
                });
            }
            
            // Evaluate the right child
            if (node->children[1]->type == "Integer" || (node->children[1]->type == "Identifier" && isIntVariable(node->children[1]->value.c_str()))) {
                visitNode(node->children[1]);
                
                // Check if the right child is zero
                textSection += "cmp rax, 0\n";
                textSection += "je .division_by_zero_error\n";

                textSection += "mov rbx, rax\n";
                textSection += "pop rax\n";
                textSection += "xor rdx, rdx\n";  
                textSection += "div rbx\n";       
                textSection += "mov rax, rdx\n";  
            }
            else{
                m_errorManager.addError(Error{
                    "Expected Int for Modulo Operation ; ", 
                    "Got " + std::string(node->children[1]->type.c_str()), 
                    "Semantics", 
                    0
                });
            }
        }
    } else if (node->type == "Identifier") {
        // textSection += "mov rax, qword [" + node->value + "]\n"; // Old way
        textSection += "mov rax, " + getIdentifierMemoryOperand(node->value) + "\n";
    } else if (node->type == "Integer") {
        textSection += "mov rax, " + node->value + "\n";
    } else {
        // For any other node types, recursively visit children.
        for (auto &child : node->children) {
            visitNode(child);
        }
    }
}
void CodeGenerator::endAssembly() {
    // --- Program Exit ---
    textSection += "\n; Program exit\n";
    textSection += "mov rax, 60      ; syscall: exit\n";
    textSection += "xor rdi, rdi     ; exit code 0\n";
    textSection += "syscall\n\n";

    // --- Division by Zero Error Handler ---
    textSection += "\n; Division by zero error handler\n";
    textSection += ".division_by_zero_error:\n";
    textSection += "    ; Print error message\n";
    textSection += "    mov rax, 1          ; syscall: write\n";
    textSection += "    mov rdi, 1          ; file descriptor: stdout\n";
    textSection += "    mov rsi, div_zero_msg\n";
    textSection += "    mov rdx, div_zero_len\n";
    textSection += "    syscall\n";
    textSection += "    ; Exit with error code\n";
    textSection += "    mov rax, 60         ; syscall: exit\n";
    textSection += "    mov rdi, 1          ; exit code 1 (error)\n";
    textSection += "    syscall\n\n";

    // --- Print Number Function ---
    textSection += "; Function to print a number in RAX\n";
    textSection += "print_number:\n";
    textSection += "    push rbp\n";
    textSection += "    mov rbp, rsp\n";
    textSection += "    mov r12, rax          ; save original number in r12\n";

    // Handle negative numbers
    textSection += "    cmp r12, 0\n";
    textSection += "    jge .print_positive\n";
    textSection += "    ; Handle negative number\n";
    textSection += "    push r12\n";
    textSection += "    mov rax, 1\n";
    textSection += "    mov rdi, 1\n";
    textSection += "    mov rsi, minus_sign\n";
    textSection += "    mov rdx, 1\n";
    textSection += "    syscall\n";
    textSection += "    pop r12\n";
    textSection += "    neg r12      ; Make the number positive\n";

    textSection += ".print_positive:\n";
    textSection += "    lea rdi, [buffer+31]  ; point rdi to end of buffer\n";
    textSection += "    mov byte [rdi], 0     ; null-terminate the buffer\n";
    textSection += "    cmp r12, 0\n";
    textSection += "    jne .print_convert\n";
    textSection += "    mov byte [rdi-1], '0'\n";
    textSection += "    lea rdi, [rdi-1]\n";
    textSection += "    jmp .print_output\n";
     
    textSection += ".print_convert:\n";
    textSection += "    mov rax, r12\n";
    textSection += "    mov rbx, 10\n";
    textSection += ".print_convert_loop:\n";
    textSection += "    xor rdx, rdx\n";
    textSection += "    div rbx             ; rax = quotient, rdx = remainder\n";
    textSection += "    add rdx, '0'        ; convert digit to ASCII\n";
    textSection += "    dec rdi             ; move pointer left\n";
    textSection += "    mov [rdi], dl       ; store digit in buffer\n";
    textSection += "    cmp rax, 0\n";
    textSection += "    jne .print_convert_loop\n";
    
    textSection += ".print_output:\n";
    textSection += "    lea rsi, [rdi]      ; rsi points to start of the string\n";
    textSection += "    mov rdx, buffer+31\n";
    textSection += "    sub rdx, rdi        ; compute string length\n";
    textSection += "    mov rax, 1          ; syscall: write\n";
    textSection += "    mov rdi, 1          ; file descriptor: stdout\n";
    textSection += "    syscall\n";
    textSection += "    pop rbp\n";
    textSection += "    ret\n\n";
    

    // Dans la fonction endAssembly(), remplacer la partie list_concat
textSection += "\n; Function to concatenate two lists\n";
textSection += "list_concat:\n";
textSection += "    push rbp\n";
textSection += "    mov rbp, rsp\n";
textSection += "    push r12\n";
textSection += "    push r13\n";
textSection += "    push r14\n";
textSection += "    push rbx\n";
textSection += "    push r15\n";
    
// Sauvegarder les listes d'entrée
textSection += "    mov r12, rdi        ; r12 = liste1\n";
textSection += "    mov r13, rsi        ; r13 = liste2\n";
    
// Lire les tailles des deux listes
textSection += "    mov r14, [r12]      ; r14 = taille de liste1\n";
textSection += "    mov r15, [r13]      ; r15 = taille de liste2\n";
    
// Obtenir un nouvel emplacement pour la liste résultat
textSection += "    mov rbx, [list_offset]\n";
textSection += "    mov rax, list_buffer\n";
textSection += "    add rax, rbx        ; rax = adresse de la nouvelle liste\n";
textSection += "    push rax            ; sauvegarder l'adresse de la nouvelle liste\n";
    
// Calculer et stocker la taille totale
textSection += "    mov rcx, [list_offset]\n";
textSection += "    mov rax, r14\n";
textSection += "    add rax, r15\n";
textSection += "    mov [list_buffer + rcx], rax  ; stocker la taille totale\n";
textSection += "    add rcx, 8\n";
textSection += "    mov [list_offset], rcx\n";
    
// Copier les éléments de la première liste (sauter la taille)
textSection += "    mov rsi, r12\n";
textSection += "    add rsi, 8          ; sauter la taille de liste1\n";
textSection += "    mov rbx, rcx        ; offset de destination\n";
textSection += "    mov rcx, r14        ; nombre d'éléments à copier\n";
textSection += "    cmp rcx, 0\n";
textSection += "    je .list_copy1_done\n";
    
textSection += ".list_copy1_loop:\n";
textSection += "    mov rdx, [rsi]      ; charger élément de liste1\n";
textSection += "    mov [list_buffer + rbx], rdx  ; copier l'élément\n";
textSection += "    add rsi, 8          ; avancer dans liste1\n";
textSection += "    add rbx, 8          ; avancer dans nouvelle liste\n";
textSection += "    dec rcx\n";
textSection += "    jnz .list_copy1_loop\n";
textSection += ".list_copy1_done:\n";
    
// Copier les éléments de la deuxième liste (sauter la taille)
textSection += "    mov rsi, r13\n";
textSection += "    add rsi, 8          ; sauter la taille de liste2\n";
textSection += "    mov rcx, r15        ; nombre d'éléments à copier\n";
textSection += "    cmp rcx, 0\n";
textSection += "    je .list_copy2_done\n";
    
textSection += ".list_copy2_loop:\n";
textSection += "    mov rdx, [rsi]      ; charger élément de liste2\n";
textSection += "    mov [list_buffer + rbx], rdx  ; copier l'élément\n";
textSection += "    add rsi, 8          ; avancer dans liste2\n";
textSection += "    add rbx, 8          ; avancer dans nouvelle liste\n";
textSection += "    dec rcx\n";
textSection += "    jnz .list_copy2_loop\n";
textSection += ".list_copy2_done:\n";
    
// Mettre à jour list_offset
textSection += "    mov [list_offset], rbx\n";
    
// Retourner l'adresse de la nouvelle liste
textSection += "    pop rax             ; récupérer l'adresse de la liste résultat\n";
    
// Nettoyage
textSection += "    pop r15\n";
textSection += "    pop rbx\n";
textSection += "    pop r14\n";
textSection += "    pop r13\n";
textSection += "    pop r12\n";
textSection += "    pop rbp\n";
textSection += "    ret\n\n";

    textSection += "; Function to concatenate two strings with offset\n";
    textSection += "str_concat:\n";
    textSection += "    push rbp\n";
    textSection += "    mov rbp, rsp\n";
    textSection += "    push r12\n";
    textSection += "    push r13\n";
    textSection += "    push r14\n";
    textSection += "    push rbx\n";

    textSection += "    ; Save input strings\n";
    textSection += "    mov r12, rdi        ; r12 = str1\n";
    textSection += "    mov r13, rsi        ; r13 = str2\n";
    textSection += "    mov r14, concat_buffer\n";
    textSection += "    mov rbx, [concat_offset]\n";
    textSection += "    add r14, rbx        ; r14 = destination address (buffer + offset)\n";

    // Label ID pour éviter les conflits
    static int concatId = 0;
    std::string id = std::to_string(concatId++);

    // Sauvegarder le pointeur de début de concaténation à retourner
    textSection += "    mov rax, r14\n";

    // --- Copier str1 ---
    textSection += "    mov rsi, r12\n";
    textSection += ".copy_str1_" + id + ":\n";
    textSection += "    mov al, [rsi]\n";
    textSection += "    cmp al, 0\n";
    textSection += "    je .done_str1_" + id + "\n";
    textSection += "    mov [r14], al\n";
    textSection += "    inc rsi\n";
    textSection += "    inc r14\n";
    textSection += "    jmp .copy_str1_" + id + "\n";
    textSection += ".done_str1_" + id + ":\n";

    // --- Copier str2 ---
    textSection += "    mov rsi, r13\n";
    textSection += ".copy_str2_" + id + ":\n";
    textSection += "    mov al, [rsi]\n";
    textSection += "    cmp al, 0\n";
    textSection += "    je .done_str2_" + id + "\n";
    textSection += "    mov [r14], al\n";
    textSection += "    inc rsi\n";
    textSection += "    inc r14\n";
    textSection += "    jmp .copy_str2_" + id + "\n";
    textSection += ".done_str2_" + id + ":\n";

    // Ajout du null terminator
    textSection += "    mov byte [r14], 0\n";
    textSection += "    inc r14\n";

    // Mettre à jour concat_offset = r14 - concat_buffer
    textSection += "    mov rbx, r14\n";
    textSection += "    sub rbx, concat_buffer\n";
    textSection += "    mov [concat_offset], rbx\n";

    // Nettoyage
    textSection += "    pop rbx\n";
    textSection += "    pop r14\n";
    textSection += "    pop r13\n";
    textSection += "    pop r12\n";
    textSection += "    pop rbp\n";
    textSection += "    ret\n\n";

    // Routine pour les chaînes
    textSection += "print_string:\n";
    textSection += "    push rbp\n";
    textSection += "    mov rbp, rsp\n";
    textSection += "    mov rsi, rax\n";
    textSection += "    mov rdx, 0\n";
    textSection += ".print_strlen_loop:\n";
    textSection += "    cmp byte [rsi+rdx], 0\n";
    textSection += "    je .print_strlen_done\n";
    textSection += "    inc rdx\n";
    textSection += "    jmp .print_strlen_loop\n";
    textSection += ".print_strlen_done:\n";
    textSection += "    mov rax, 1\n";
    textSection += "    mov rdi, 1\n";
    textSection += "    syscall\n";
    textSection += "    pop rbp\n";
    textSection += "    ret\n\n";

    // Dans la fonction endAssembly(), remplacer la partie list_range
textSection += "; Function to create a range list (0...n-1)\n";
textSection += "list_range:\n";
textSection += "    push rbp\n";
textSection += "    mov rbp, rsp\n";
textSection += "    push rbx\n";
textSection += "    push r12\n";
textSection += "    push r13\n";
        
textSection += "    ; rax = n (size of the range)\n";
textSection += "    mov r12, rax        ; r12 = n\n";
        
textSection += "    ; Get new list address\n";
textSection += "    mov rbx, [list_offset]\n";
textSection += "    mov rax, list_buffer\n";
textSection += "    add rax, rbx        ; rax = address of the new list\n";
textSection += "    push rax            ; save list address\n";
        
textSection += "    ; Store the size first\n";
textSection += "    mov rcx, [list_offset]\n";
textSection += "    mov [list_buffer + rcx], r12\n";
textSection += "    add rcx, 8\n";
textSection += "    mov [list_offset], rcx\n";
        
textSection += "    ; Initialize counter\n";
textSection += "    xor r13, r13        ; r13 = 0 (counter)\n";
textSection += "    cmp r12, 0\n";
textSection += "    je .list_range_done\n";
        
textSection += ".list_range_loop:\n";
textSection += "    ; Add counter value to list\n";
textSection += "    mov rcx, [list_offset]\n";
textSection += "    mov [list_buffer + rcx], r13\n";
textSection += "    add rcx, 8\n";
textSection += "    mov [list_offset], rcx\n";
        
textSection += "    ; Increment counter\n";
textSection += "    inc r13\n";
        
textSection += "    ; Check if counter < n\n";
textSection += "    cmp r13, r12\n";
textSection += "    jl .list_range_loop\n";
        
textSection += ".list_range_done:\n";
textSection += "    ; Return list address\n";
textSection += "    pop rax\n";
        
textSection += "    ; Cleanup\n";
textSection += "    pop r13\n";
textSection += "    pop r12\n";
textSection += "    pop rbx\n";
textSection += "    pop rbp\n";
textSection += "    ret\n\n";
}

void CodeGenerator::writeToFile(const std::string &filename) {
    std::ofstream outFile(filename);
    if (!outFile) {
        throw std::runtime_error("Failed to open output file: " + filename);
    }
    outFile << asmCode;
    outFile.close();
}

void CodeGenerator::genPrint(const std::string& type) {
    if (type == "String") {
        textSection += "call print_string\n";
    }
    else {
        textSection += "call print_not_string\n";
    }
}
    

void CodeGenerator::genAffect(const std::shared_ptr<ASTNode>& node) {
    if (node->children.size() < 2) {
        throw std::runtime_error("Invalid ASTNode structure for assignment");
    }
    
    std::string varName = node->children[0]->value; 
    auto leftNode = node->children[0];
    auto rightValue = node->children[1];

    // Evaluate the right-hand side first, result will be in rax
    visitNode(rightValue);

    if (leftNode->type == "ListCall") {
        // ... existing ListCall assignment logic ...
        // This part seems to handle list element assignment correctly using listName and index.
        // Ensure listName itself is accessed correctly (global or stack) if it's an identifier.
        std::string listName = leftNode->children[0]->value;
        auto indexNode = leftNode->children[1];
        textSection += "; List element assignment\n";
        // textSection += "mov rbx, qword [" + listName + "]\n"; // Old way
        textSection += "mov rbx, " + getIdentifierMemoryOperand(listName) + "\n";


        // Vérifier si l'index est valide
        textSection += "; Calculate index\n";
        // Temporarily save rax (value to be assigned) if visitNode for indexNode uses rax
        textSection += "push rax\n";
        visitNode(indexNode); // Index in rax
        textSection += "mov rcx, rax\n"; // Save index in rcx
        textSection += "pop rax\n"; // Restore value to be assigned into rax


        textSection += "; Check if index is valid\n";
        textSection += "cmp rcx, 0\n"; // Compare index (rcx)
        static int errorCount = 0;
        std::string errorLabel = ".index_error_" + std::to_string(errorCount++);
        textSection += "jl " + errorLabel + "\n";
        textSection += "cmp rcx, [rbx]\n";  // Compare index (rcx) with size stored at list base (rbx)
        textSection += "jge " + errorLabel + "\n";
        
        textSection += "; Calculate element address\n";
        textSection += "add rbx, 8\n"; 
        textSection += "shl rcx, 3\n"; // Multiply index (rcx) by 8
        textSection += "add rbx, rcx\n"; // Add to base address of elements
        
        // Value to be stored is already in rax from visitNode(rightValue)
        textSection += "; Store value in list element\n";
        textSection += "mov qword [rbx], rax\n";
        
        std::string endLabel = ".end_list_assign_" + std::to_string(errorCount-1);
        textSection += "jmp " + endLabel + "\n";
        
        textSection += errorLabel + ":\n";
        textSection += "mov rax, 1\n";
        textSection += "mov rdi, 1\n";
        textSection += "mov rsi, index_error_msg\n";
        textSection += "mov rdx, index_error_len\n";
        textSection += "syscall\n";
        textSection += "mov rax, 60\n"; 
        textSection += "mov rdi, 1\n";  
        textSection += "syscall\n";
        
        textSection += endLabel + ":\n";
        return;
    }
    
    // Standard variable assignment
    // The result of rightValue is in rax. Now store it.
    // getIdentifierMemoryOperand handles whether it's global, local, or param.
    // No need for dataSection += varName + ": dq 0\n" here for locals/params.
    // That should be handled by getIdentifierMemoryOperand for true globals if they are new.
    textSection += "mov " + getIdentifierMemoryOperand(varName) + ", rax\n";
    
    // Type update logic (remains largely the same, but ensure it uses the correct symbol table context)
    std::string valueType;
    // ... existing type inference logic ...
    if (rightValue->type == "True" || rightValue->type == "False") {
        valueType = "Boolean";
    }
    else if (rightValue->type == "FunctionCall") {
        // Ensure this->rootNode is the correct AST root for inferFunctionReturnType
        valueType = inferFunctionReturnType(this->rootNode, rightValue->children[0]->value);
    }
    // ... other type cases ...
    else if (rightValue->type == "Identifier") {
        valueType = getIdentifierType(rightValue->value); // Get type of the RHS identifier
    }
    else {
        valueType = "Integer"; // Default or more specific based on node
    }


    if (symbolTable) { // Ensure symbolTable is valid
        // updateSymbolType should find the symbol in the correct scope (local or global)
        // and update its type information.
        updateSymbolType(varName, valueType);
    }
}



void CodeGenerator::genFor(const std::shared_ptr<ASTNode>& node) {
    
    std::string loopVar = node->children[0]->value;

    // If the loop variable is not yet declared, declare it in the data section
    if (declaredVars.find(loopVar) == declaredVars.end()) {
        dataSection += loopVar + ": dq 0\n";
        declaredVars.insert(loopVar);
    }
    
    // Generate unique labels 
    static int loopCounter = 0;
    std::string loopId = std::to_string(loopCounter++);
    std::string startLabel = ".loop_start_" + loopId;
    std::string endLabel = ".loop_end_" + loopId;
    
    // Check if this is a range function call
    if (node->children[1]->type == "FunctionCall" && 
        node->children[1]->children[0]->value == "range") {
        
        auto paramList = node->children[1]->children[1];
        if (paramList->children.size() != 1){
            m_errorManager.addError(Error{
                    "Expected one parameter for range ; ", 
                    "Got " + std::to_string(paramList->children.size()), 
                    "Semantics", 
                    0
                });
            return;
        }
        

        if (paramList->children.size() >= 1) {
            textSection += "; Initialize loop\n";
            textSection += "mov qword [" + loopVar + "], 0\n";
            
            // Get the end value of the range
            visitNode(paramList->children[0]); 
            /*
            if ((paramList->children[0]->type == "Identifier" && !isIntVariable(paramList->children[0]->value.c_str()))){
                m_errorManager.addError(Error{"Expected Int for range ; ", "Got " + std::string(paramList->children[0]->type.c_str()), "Semantic", 0});
                return;
            }
            if (!(paramList->children[0]->type == "Integer")){
                m_errorManager.addError(Error{"Expected Int for range ; ", "Got " + std::string(paramList->children[0]->type.c_str()), "Semantic", 0});
                return;
            }
            if (std::stoi(paramList->children[0]->value) < 0){
                m_errorManager.addError(Error{"Expected positive Int for range ; ", "Got " + std::string(paramList->children[0]->type.c_str()), "Semantic", 0});
                return;
            } 
            */
            textSection += "push rax\n";
            
            // Start of loop
            textSection += startLabel + ":\n";
            textSection += "; Check loop condition\n";
            textSection += "mov rax, qword [" + loopVar + "]\n";
            textSection += "pop rbx\n"; 
            textSection += "push rbx\n"; 
            textSection += "cmp rax, rbx\n";
            textSection += "jge " + endLabel + "\n";
            
            // Execute loop body
            textSection += "; Loop body\n";
            visitNode(node->children[2]); 
            
            // Increment loop variable
            textSection += "; Increment loop variable\n";
            textSection += "mov rax, qword [" + loopVar + "]\n";
            textSection += "inc rax\n";
            textSection += "mov qword [" + loopVar + "], rax\n";
            textSection += "jmp " + startLabel + "\n";
            
            // End of loop
            textSection += endLabel + ":\n";
            textSection += "pop rbx\n"; 
        }
    } else {
        textSection += "; Warning: Non-range for loop not implemented\n";
    }
}

void CodeGenerator::genIf(const std::shared_ptr<ASTNode>& node) {
    // Generate unique labels for this if statement
    static int ifCounter = 0;
    std::string ifId = std::to_string(ifCounter++);
    std::string elseLabel = ".else_" + ifId;
    std::string endLabel = ".endif_" + ifId;
    
    // Generate code for the condition expression
    textSection += "; If condition\n";
    visitNode(node->children[0]); // rax <- valeur
    toBool("rax");            // rax <- 0 / 1
    
    // Test if the condition is false (0)
    textSection += "cmp rax, 0\n";
    
    if (node->children.size() > 2 && node->children[2]) {
        textSection += "je " + elseLabel + "\n"; // si faux on saute
    } else {
        textSection += "je " + endLabel + "\n"; // si faux on saute
    }
    
    // Generate code for IfBody
    textSection += "; If body\n";
    visitNode(node->children[1]);
    
    // Skip if IfBody
    if (node->children.size() > 2 && node->children[2]) {
        textSection += "jmp " + endLabel + "\n";
        
        textSection += elseLabel + ":\n";
        textSection += "; Else body\n";
        visitNode(node->children[2]);
    }
    
    textSection += endLabel + ":\n";
}

void CodeGenerator::genFunction(const std::shared_ptr<ASTNode>& node) {
    std::string funcName = node->value;
    // currentFunction is already set by visitNode
    // currentSymbolTable should point to the function's specific scope

    FunctionSymbol* funcSym = nullptr;
    // Find the FunctionSymbol in the global table (parent of the function's scope)
    if (symbolTable) { // Global table must exist
        Symbol* sym = symbolTable->findImmediateSymbol(funcName); // Functions are defined globally
        if (sym && sym->symCat == "function") {
            funcSym = dynamic_cast<FunctionSymbol*>(sym);
        }
    }

    int frameSizeForSub = 0; 
    if (funcSym) {
        frameSizeForSub = funcSym->frameSize; 
    } else {
        m_errorManager.addError({"Function symbol not found for frame size: ", funcName, "CodeGeneration", std::stoi(node->line)});
        // Default to a minimal size to allow compilation, but this is an error.
        // For fib, if no locals, frameSize was calculated to be 8.
        // This value (localsTotalSize + padding_needed for alignment with callee-saved)
        // is what `sub rsp, frameSizeForSub` uses.
    }

    textSection += "\n" + funcName + ":\n";
    // Prologue
    textSection += "    push rbp\n";
    textSection += "    mov rbp, rsp\n";
    if (frameSizeForSub > 0) {
        textSection += "    sub rsp, " + std::to_string(frameSizeForSub) + " ; Allocate space for locals and stack alignment\n";
    }
    // Save callee-saved registers (System V ABI: rbx, r12, r13, r14, r15)
    textSection += "    push rbx\n";
    textSection += "    push r12\n";
    textSection += "    push r13\n";
    textSection += "    push r14\n";
    textSection += "    push r15\n";

    // Generate function body
    // Parameters are accessed via [rbp + offset]
    // Locals are accessed via [rbp - offset]
    if (node->children.size() > 1 && node->children[1] && node->children[1]->type == "FunctionBody") {
         visitNode(node->children[1]);
    } else if (node->children.size() > 0 && node->children[0]->type != "FormalParameterList" && node->children[0]) {
        // Case where there are no formal parameters, and child[0] is the body
        visitNode(node->children[0]);
    }
    // Ensure a return path if no explicit return is in the AST (e.g., for void functions or if last statement isn't return)
    // For now, relying on explicit return statements to jump to epilogue.

    // Epilogue
    textSection += ".return_" + funcName + ":\n"; 
    // Restore callee-saved registers in reverse order of push
    textSection += "    pop r15\n";
    textSection += "    pop r14\n";
    textSection += "    pop r13\n";
    textSection += "    pop r12\n";
    textSection += "    pop rbx\n";
    
    textSection += "    leave          ; mov rsp, rbp; pop rbp\n"; 
    textSection += "    ret\n";
}

void CodeGenerator::genList(const std::shared_ptr<ASTNode>& node) {
    int listSize = node->children.size();

    // Récupérer l'adresse de la nouvelle liste
    textSection += "mov rbx, [list_offset]\n";
    textSection += "mov rax, list_buffer\n";
    textSection += "add rax, rbx\n";  
    textSection += "push rax\n";     

    
    
    if ((listSize == 1) && node->children[0] == nullptr) {
        textSection += "mov rcx, [list_offset]\n";
        textSection += "mov qword [list_buffer + rcx], 0\n";  
        textSection += "add rcx, 8\n";
        textSection += "mov [list_offset], rcx\n";
    } 
    else {
        // Stocker la taille comme premier élément
        textSection += "mov rcx, [list_offset]\n";
        textSection += "mov qword [list_buffer + rcx], " + std::to_string(listSize) + "\n";
        textSection += "add rcx, 8\n";
        textSection += "mov [list_offset], rcx\n";
        for (int i = 0; i < listSize; i++) {
            visitNode(node->children[i]); 
            auto type0 = node->children[i]->type;
            if (node->children[i]->type == "Identifier") {
                type0 = getIdentifierType(node->children[i]->value);
            }
            if (type0 == "auto") {
                m_errorManager.addError(Error{
                    "Undefined Variable; ", 
                    "Used " + std::string(node->children[i]->value.c_str())+ " before assignment",
                    "Semantics", 
                    std::stoi(node->line)
                });
                return;
            }
            textSection += "mov rcx, [list_offset]\n";
            textSection += "mov [list_buffer + rcx], rax\n";
            textSection += "add rcx, 8\n";
            textSection += "mov [list_offset], rcx\n";
        }
    }   

    textSection += "pop rax\n";  // rax = adresse de début de la liste
}

void CodeGenerator::genFunctionCall(const std::shared_ptr<ASTNode>& node) {
    if (node->children.empty() || node->children[0]->type != "Identifier") {
        m_errorManager.addError({"Invalid function call AST structure", "", "CodeGeneration", std::stoi(node->line)});
        return;
    }

    std::string funcName = node->children[0]->value;

    if (funcName == "range") {
        // ... existing range handling ...
        if (node->children.size() > 1 && 
            (node->children[1]->type == "ActualParameterList" || node->children[1]->type == "ParameterList") && 
            !node->children[1]->children.empty()) {
            visitNode(node->children[1]->children[0]); 
            textSection += "    call list_range      ; rax should contain address of new list\n";
        } else {
            m_errorManager.addError({"Range function expects one argument", "", "CodeGeneration", std::stoi(node->line)});
            textSection += "    mov rax, 0 ; Error case, return null or handle\n";
        }
        return;
    }
    
    int argCount = 0;
    const std::vector<std::shared_ptr<ASTNode>>* argListPtr = nullptr;

    if (node->children.size() > 1 && (node->children[1]->type == "ParameterList" || node->children[1]->type == "ActualParameterList")) {
        argListPtr = &node->children[1]->children;
        argCount = argListPtr->size();
    }

    // Stack Alignment: RSP must be 16-byte aligned BEFORE the call.
    // The call pushes 8 bytes (return address).
    // If (argCount % 2 != 0) (i.e., odd number of qword arguments),
    // the total bytes pushed for args will be 16k + 8.
    // To make RSP 16-byte aligned before call, we need to subtract an additional 8 bytes
    // if the number of arguments is odd.
    bool alignment_padding_added = false;
    if ((argCount % 2) != 0) { // If odd number of arguments
        textSection += "    sub rsp, 8           ; Align stack for odd number of arguments\n";
        alignment_padding_added = true;
    }
    
    // Push arguments in reverse order (right to left)
    if (argListPtr && argCount > 0) {
        for (auto it = argListPtr->rbegin(); it != argListPtr->rend(); ++it) {
            visitNode(*it); // Evaluate argument, result in rax
            textSection += "    push rax\n";
        }
    }
    
    textSection += "    call " + funcName + "\n";
    
    // Cleanup: remove arguments from stack
    if (argCount > 0) {
        textSection += "    add rsp, " + std::to_string(argCount * 8) + " ; Pop arguments\n";
    }

    if (alignment_padding_added) {
        textSection += "    add rsp, 8           ; Remove stack alignment padding\n";
    }
}

void CodeGenerator::genReturn(const std::shared_ptr<ASTNode>& node) {
    // Evaluate return expression if any
    if (!node->children.empty()) {
        visitNode(node->children[0]);
    } else {
        textSection += "    xor rax, rax\n";  
    }
    
    // Jump to return label
    textSection += "    jmp .return_" + currentFunction + "\n";
}


// Fonction pour obtenir le type d'un identifiant à partir de la table des symboles
std::string CodeGenerator::getIdentifierType(const std::string& name) {
    if (!symbolTable) return "auto"; // Should not happen if called correctly

    SymbolTable* tableToSearch = currentSymbolTable; // Start with current function's scope

    std::function<Symbol*(const std::string&, SymbolTable*)> findSymbolRecursive = 
        [&](const std::string& symName, SymbolTable* table) -> Symbol* {
        if (!table) return nullptr;
        for (const auto& sym_ptr : table->symbols) {
            if (sym_ptr->name == symName) {
                return sym_ptr.get();
            }
        }
        // Correctly search in parent scope if currentSymbolTable is a function scope
        if (table->parent && (table->scopeName.rfind("function ", 0) == 0 || table->parent == symbolTable)) {
             return findSymbolRecursive(symName, table->parent);
        } else if (table != symbolTable) { // If not in function scope and not global, search global
            return findSymbolRecursive(symName, symbolTable);
        }
        return nullptr; // Reached top or no relevant parent
    };
    
    Symbol* sym = nullptr;
    if (tableToSearch) { 
        sym = findSymbolRecursive(name, tableToSearch);
    }
    // If not found in current scope hierarchy, try global scope directly if not already searched
    if (!sym && tableToSearch != symbolTable && symbolTable) {
        sym = findSymbolRecursive(name, symbolTable);
    }


    if (sym) {
        if (sym->symCat == "variable" || sym->symCat == "parameter") {
            if (auto varSym = dynamic_cast<VariableSymbol*>(sym)) {
                return varSym->type;
            }
        } else if (sym->symCat == "function") {
            if (auto funcSym = dynamic_cast<FunctionSymbol*>(sym)) {
                return funcSym->returnType; // Or a special "function_type"
            }
        } else if (sym->symCat == "array") {
            // You might have a specific type string for arrays, e.g., "list" or "array"
            // Or derive from ElementType if stored. For now, "List" seems to be used.
            return "List"; 
        }
        return "auto"; // Default if category is known but type isn't (shouldn't happen)
    }
    
    return "auto"; // Not found or type unknown
}


bool CodeGenerator::isStringVariable(const std::string& name) {
    std::string type = getIdentifierType(name);
    return type == "String" || type == "auto"; // Consider if "auto" should be accepted here too
}

bool CodeGenerator::isIntVariable(const std::string& name) {
    std::string type = getIdentifierType(name);
    return type == "Integer" || type == "auto"; // Accept "auto" as potentially integer
}


void CodeGenerator::updateSymbolType(const std::string& name, const std::string& type) {
    if (!symbolTable) return;

    std::function<bool(SymbolTable*, const std::string&, const std::string&)> updateInTableHierarchy = 
        [&](SymbolTable* table, const std::string& symName, const std::string& newType) -> bool {
        while (table) {
            for (auto& sym_ptr : table->symbols) {
                if (sym_ptr->name == symName && (sym_ptr->symCat == "variable" || sym_ptr->symCat == "parameter")) {
                    if (auto varSym = dynamic_cast<VariableSymbol*>(sym_ptr.get())) {
                        varSym->type = newType;
                        return true;
                    }
                }
            }
            table = table->parent;
        }
        return false;
    };

    // Try to update in current function scope hierarchy first
    if (currentSymbolTable && currentSymbolTable->scopeName.rfind("function ", 0) == 0) {
        if (updateInTableHierarchy(currentSymbolTable, name, type)) {
            return;
        }
    }
    // If not found or not in a function, try global scope hierarchy
    updateInTableHierarchy(symbolTable, name, type);
}

void CodeGenerator::resetFunctionVarTypes(const std::string& funcName) {
    // This function's utility might change. If types are part of the Symbol objects
    // and these objects are correctly scoped, explicit reset might only be needed
    // if you are re-using symbol table entries across multiple analyses without
    // proper re-initialization. With stack-based locals, their "type" state
    // is effectively gone when the function returns.
    // However, if the SymbolTable objects persist and are reused, this might be needed.

    if (!symbolTable) return;
    
    SymbolTable* functionScope = nullptr;
    for (const auto& child : symbolTable->children) { // Assuming function scopes are children of global
        if (child->scopeName == "function " + funcName) {
            functionScope = child.get();
            break;
        }
    }

    if (functionScope) {
        for (auto& sym_ptr : functionScope->symbols) {
            if (sym_ptr->symCat == "variable" || sym_ptr->symCat == "parameter") {
                if (auto varSym = dynamic_cast<VariableSymbol*>(sym_ptr.get())) {
                    varSym->type = "auto"; // Reset to default/uninitialized type
                }
            }
        }
    }
}

std::string CodeGenerator::inferFunctionReturnType(const std::shared_ptr<ASTNode>& root, const std::string& funcName) {
    // Cette fonction analyserait l'AST de la fonction pour déterminer son type de retour
    // En se basant sur les instructions `return`
    if (!root) return "Integer"; // Or "auto"

    // First, try to get it from the symbol table if semantic analysis populated it
    if (symbolTable) {
        Symbol* sym = symbolTable->findSymbol(funcName);
        if (sym && sym->symCat == "function") {
            if (auto funcSym = dynamic_cast<FunctionSymbol*>(sym)) {
                if (funcSym->returnType != "autoFun" && !funcSym->returnType.empty()) { // "autoFun" was a placeholder
                    return funcSym->returnType;
                }
            }
        }
    }


    for (const auto& child : root->children) {
        if (child->type == "Definitions") {
            for (const auto& def : child->children) {
                if (def->type == "FunctionDefinition" && def->value == funcName) {
                    // Trouver tous les noeuds Return dans le corps de la fonction
                    std::string returnType = "Integer"; // Type par défaut
                    bool typeSet = false;
                    
                    std::function<void(const std::shared_ptr<ASTNode>&)> findReturns;
                    findReturns = [&](const std::shared_ptr<ASTNode>& node) {
                        if (!node || typeSet) return; // Stop if type already determined by a concrete return
                        
                        if (node->type == "Return" && !node->children.empty()) {
                            auto returnExpr = node->children[0];
                            std::string currentExprType = "Integer"; // Default for this return expression

                            if (returnExpr->type == "String") {
                                currentExprType = "String";
                            } else if (returnExpr->type == "List") {
                                currentExprType = "List";
                            } else if (returnExpr->type == "Integer") {
                                currentExprType = "Integer";
                            } else if (returnExpr->type == "True" || returnExpr->type == "False") {
                                currentExprType = "Boolean"; // Assuming Boolean type
                            } else if (returnExpr->type == "FunctionCall") {
                                if (returnExpr->children[0]->value == funcName) {
                                    // Recursive call, type is what we are trying to determine.
                                    // Avoid setting returnType here unless it's the only return.
                                    // If other returns exist, they should dictate the type.
                                    // If this is the *only* kind of return, it might remain "Integer" or "auto".
                                } else {
                                    // For non-recursive calls, try to get the known return type.
                                    currentExprType = getFunctionReturnType(returnExpr->children[0]->value);
                                }
                            } else if (returnExpr->type == "Identifier") {
                                 currentExprType = getIdentifierType(returnExpr->value);
                            }
                            // More complex expressions (ArithOp, etc.) would need their type inferred here.
                            // For simplicity, we assume direct types or simple lookups for now.

                            if (!typeSet) { // First return statement found
                                returnType = currentExprType;
                                if (returnType != "auto") { // If we found a concrete type
                                   // typeSet = true; // Uncomment if first concrete return dictates type
                                }
                            } else if (returnType != currentExprType && currentExprType != "auto") {
                                // Multiple return statements with different concrete types - issue a warning/error or adopt a common type
                                // For now, let's stick with the first one found or a default.
                                // Or, if one is "Integer" and another is "String", this is a type conflict.
                                // m_errorManager.addError({"Type conflict in return statements for function: ", funcName, "Semantic", 0});
                            }
                            return; // Stop searching this branch once a return is processed
                        }
                        
                        for (const auto& childNode : node->children) {
                            findReturns(childNode);
                        }
                    };
                    
                    // Body is child 1 (FormalParameterList is child 0)
                    if (def->children.size() > 1 && def->children[1]->type == "FunctionBody") {
                        findReturns(def->children[1]); 
                    } else if (def->children.size() > 0 && def->children[0]->type != "FormalParameterList" && def->children[0]) {
                        // Case where there are no formal parameters, and child[0] is the body
                        findReturns(def->children[0]);
                    }
                    
                    return returnType;
                }
            }
        }
    }
    
    return "Integer"; // Type par défaut
}

std::string CodeGenerator::getExpressionType(const std::shared_ptr<ASTNode>& node) {
    if (!node) return "auto"; // Or throw an error

    if (node->type == "Integer") {
        return "Integer";
    } else if (node->type == "String") {
        return "String";
    } else if (node->type == "True" || node->type == "False") {
        return "Boolean";
    } else if (node->type == "Identifier") {
        return getIdentifierType(node->value);
    } else if (node->type == "FunctionCall") {
        if (!node->children.empty() && node->children[0]->type == "Identifier") {
            // Special handling for built-in functions if their return types are fixed
            // and not covered by inferFunctionReturnType or symbol table.
            // Example:
            // if (node->children[0]->value == "range") return "List";
            return inferFunctionReturnType(this->rootNode, node->children[0]->value);
        }
        return "autoFun"; // Default for function calls if specific type can't be inferred yet
    } else if (node->type == "List") {
        return "List";
    } else if (node->type == "ArithOp" || node->type == "TermOp") {
        // For arithmetic operations, the type often depends on the operands.
        // A more sophisticated type inference would check operand types.
        // For now, let's assume Integer if not clearly string/list.
        // If children exist, try to infer from them.
        if (!node->children.empty()) {
            std::string type1 = getExpressionType(node->children[0]);
            if (node->children.size() > 1) {
                std::string type2 = getExpressionType(node->children[1]);
                // Simple rule: if both are integer, result is integer.
                // If one is string (for '+'), result is string.
                // If one is list (for '+'), result is list.
                if (node->value == "+") {
                    if (type1 == "String" || type2 == "String") return "String";
                    if (type1 == "List" || type2 == "List") return "List";
                }
                // Default to type of first operand or Integer
                if (type1 != "auto") return type1;
                return "Integer"; 
            }
            if (type1 != "auto") return type1;
        }
        return "Integer"; // Default for ArithOp/TermOp
    } else if (node->type == "Compare" || node->type == "And" || node->type == "Or" || node->type == "Not") {
        return "Boolean";
    } else if (node->type == "UnaryOp") {
        if (!node->children.empty()) {
            return getExpressionType(node->children[0]); // Type is same as operand for negation
        }
        return "Integer"; // Default for UnaryOp (typically negation)
    }
    // Add more cases as needed for other expression types

    // Fallback or if type is genuinely dynamic/unknown at this stage
    m_errorManager.addError({"Cannot determine expression type for node type: ", node->type, "CodeGeneration", std::stoi(node->line)});
    return "auto"; 
}

std::string CodeGenerator::getFunctionReturnType(const std::string& funcName) {
    if (!symbolTable) return "auto";

    for (const auto& sym : symbolTable->symbols) {
        if (sym->name == funcName && sym->symCat == "function") {
            if (auto funcSym = dynamic_cast<FunctionSymbol*>(sym.get())) {
                return funcSym->returnType;
            }
        }
    }

    return "auto"; // Type par défaut si la fonction n'est pas trouvée
}
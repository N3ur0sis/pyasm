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
    SymbolTable* lookupScope = currentSymbolTable ? currentSymbolTable : symbolTable; // Start with current, fallback to global

    if (lookupScope) {
        sym = lookupScope->findSymbol(name); // findSymbol searches current then parent scopes
    }

    if (sym) {
        if (auto vs = dynamic_cast<VariableSymbol*>(sym)) {
            if (vs->isGlobal) {
                // Ensure global variables are declared in .bss or .data
                // emitGlobals should handle this.
                return "qword [" + name + "]";
            } else if (vs->category == "parameter") {
                // Parameters are at positive offsets from rbp
                return "qword [rbp + " + std::to_string(vs->offset) + "]";
            } else { // Catches "variable" (locals, loop vars with negative offsets)
                return std::string("qword [rbp") + (vs->offset < 0 ? " - " : " + ") 
                       + std::to_string(std::abs(vs->offset)) + "]";
            }
        } else if (dynamic_cast<FunctionSymbol*>(sym)) { // Check type without assigning to unused fs
            return name; // Function name used as a label
        }
    }

    // If symbol not found or not a recognized type for memory operand
    m_errorManager.addError({"Undefined identifier or unhandled symbol category: ", name, "CodeGeneration", 0 /* TODO: Get line number if possible */});
    return "qword [error_undefined_" + name + "]"; // Fallback to prevent assembler error, but indicates a problem
}


void CodeGenerator::emitGlobals(SymbolTable* globalScope)
{
    if (!globalScope) return;
    for (auto& sp : globalScope->symbols)
        if (auto *vs = dynamic_cast<VariableSymbol*>(sp.get());
            vs && vs->isGlobal)                       // <-- only globals
        {
            // Avoid duplicate emission (e.g., multi-pass)
            if (declaredVars.insert(vs->name).second)
                bssSection += "    " + vs->name + ":  resq 1\n";
        }
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
    emitGlobals(symTable);                        // <<< NEW

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
    this->textSection += "    push rbp\n";
    this->textSection += "    mov rbp, rsp\n";
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
    // static int ctr = 0; // Old
    return "." + base + "_" + std::to_string(this->labelCounter++); // Use member counter
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
        SymbolTable* previousTable = currentSymbolTable;
        if (symbolTable) { 
            for (const auto& childScope : symbolTable->children) { 
                if (childScope->scopeName == "function " + node->value) {
                    currentSymbolTable = childScope.get();
                    break;
                }
            }
        }
        currentFunction = node->value; 
        genFunction(node); 
        currentFunction.clear(); 
        currentSymbolTable = previousTable; 
        resetFunctionVarTypes(node->value);
    } else if (node->type == "FunctionCall") {
        genFunctionCall(node);
    } else if (node->type == "Return") {
        genReturn(node);
    } else if (node->type == "Identifier") {
        std::string name = node->value;
        textSection += "    mov rax, " + getIdentifierMemoryOperand(name) + "\n";
    } else if (node->type == "Integer") {
        textSection += "    mov rax, " + node->value + "\n";
    } else if (node->type == "String") {
        std::string strLabel = "str_" + std::to_string(this->stringLabelCounter++); // Removed leading dot
        std::string strValue = node->value;
        // Basic escaping for quotes inside the string for NASM
        size_t pos = 0;
        while ((pos = strValue.find('"', pos)) != std::string::npos) {
            strValue.replace(pos, 1, "\", '\"', \""); // NASM way to include a quote
            pos += 9; 
        }
        this->dataSection += strLabel + ": db \"" + strValue + "\", 0\n";
        this->textSection += "    mov rax, " + strLabel + "\n";
    } else if (node->type == "List") {
        genList(node); // genList should put the list address in rax
    } else if (node->type == "True") {
        textSection += "    mov rax, 1\n";
    } else if (node->type == "False") {
        textSection += "    mov rax, 0\n";
    } else if (node->type == "If") { 
        genIf(node);
    } else if (node->type == "For") {
        genFor(node);
    } else if (node->type == "Print") {
        if (!node->children.empty()) {
            for (size_t i = 0; i < node->children.size(); ++i) {
                const auto& argNode = node->children[i];
                visitNode(argNode); // Evaluate argument, result in RAX
                std::string argType = getExpressionType(argNode);
				if (argType == "auto" || argType == "autoFun")
    argType = "Integer";
                if (argType == "Integer" || argType == "Boolean" || argType == "autoFun" /*fallback*/) {
                    this->textSection += "    call print_number\n";
                } else if (argType == "String") {
                    this->textSection += "    call print_string\n";
                } else if (argType == "List") {
                    this->textSection += "    call print_not_string\n"; // Correct function for lists
                } else {
                    m_errorManager.addError({"Unhandled type for print: ", argType, "CodeGeneration", std::stoi(node->line)});
                    this->textSection += "    call print_number ; Fallback: prints RAX as number\n";
                }

                // Add a space if not the last argument
                if (i < node->children.size() - 1) {
                    textSection += "    mov rax, 1\n";
                    textSection += "    mov rdi, 1\n";
                    textSection += "    mov rsi, space\n"; // Assuming 'space' is defined in .data
                    textSection += "    mov rdx, 1\n";
                    textSection += "    syscall\n";
                }
            }
        }
        // Print newline after all arguments
        this->textSection += "    mov rax, 1\n";
        this->textSection += "    mov rdi, 1\n";
        this->textSection += "    mov rsi, newline\n"; // Assuming 'newline' is defined in .data
        this->textSection += "    mov rdx, 1\n";
        this->textSection += "    syscall\n";

    } else if (node->type == "Compare") {
        visitNode(node->children[0]); 
        textSection += "    push rax\n";  
        visitNode(node->children[1]); 
        textSection += "    mov rbx, rax\n";
        textSection += "    pop rax\n";
        
        textSection += "    cmp rax, rbx\n";
        
        if (node->value == "==") textSection += "    sete al\n";  
        else if (node->value == "!=") textSection += "    setne al\n"; 
        else if (node->value == "<") textSection += "    setl al\n"; 
        else if (node->value == ">") textSection += "    setg al\n";  
        else if (node->value == "<=") textSection += "    setle al\n"; 
        else if (node->value == ">=") textSection += "    setge al\n"; 
        else {
            m_errorManager.addError({"Unknown comparison operator: ", node->value, "CodeGeneration", std::stoi(node->line)});
            textSection += "    mov al, 0 ; Error case\n";
        }
        textSection += "    movzx rax, al\n";
    } else if (node->type == "ArithOp") { // Handles + and -
        if (node->children.size() < 2) {
             m_errorManager.addError({"ArithOp requires two children", node->value, "CodeGeneration", std::stoi(node->line)});
             return;
        }
        visitNode(node->children[0]); // Left operand in rax
        textSection += "    push rax\n";
        visitNode(node->children[1]); // Right operand in rax
        textSection += "    mov rbx, rax\n";   // Right operand in rbx
        textSection += "    pop rax\n";      // Left operand in rax

        std::string typeL = getExpressionType(node->children[0]);
        std::string typeR = getExpressionType(node->children[1]);

		

        if (node->value == "+") {
			if (typeL == "auto")  typeL = "Integer";
if (typeR == "auto")  typeR = "Integer";
            if (typeL == "List" || typeR == "List") { // Promote to list concat if either is list
                 if (typeL != "List") { /* TODO: Convert rax (non-list) to list if necessary, or error */ }
                 if (typeR != "List") { /* TODO: Convert rbx (non-list) to list if necessary, or error */ }
                 textSection += "    mov rdi, rax\n";
                 textSection += "    mov rsi, rbx\n";
                 textSection += "    call list_concat\n"; // rax will contain result address
            } else if (typeL == "String" || typeR == "String") { // Promote to string concat
                 if (typeL != "String") { /* TODO: Convert rax to string if necessary, or error */ }
                 if (typeR != "String") { /* TODO: Convert rbx to string if necessary, or error */ }
                 textSection += "    mov rdi, rax\n";
                 textSection += "    mov rsi, rbx\n";
                 textSection += "    call str_concat\n"; // rax will contain result address
            } else if (typeL == "Integer" && typeR == "Integer") {
                textSection += "    add rax, rbx\n";
            } else {
                 m_errorManager.addError({"Type mismatch or unsupported types for '+': ", typeL + ", " + typeR, "CodeGeneration", std::stoi(node->line)});
                 textSection += "    mov rax, 0 ; Error for + op\n";
            }
        } else if (node->value == "-") {
    auto left  = node->children[0];
    auto right = node->children[1];

    std::string tL = getExpressionType(left);
    std::string tR = getExpressionType(right);

    // auto ➜ Integer upgrade
    if (tL == "auto") tL = "Integer";
    if (tR == "auto") tR = "Integer";

    if (!isNumeric(tL) || !isNumeric(tR)) {
        m_errorManager.addError({"Type mismatch for '-': ", tL + ", " + tR,
                                 "CodeGeneration", std::stoi(node->line)});
        textSection += "    mov rax, 0\n";
    } else {
        visitNode(left);              // → rax
        textSection += "    push rax\n";
        visitNode(right);             // → rax
        textSection += "    mov rbx, rax\n";
        textSection += "    pop rax\n";
        textSection += "    sub rax, rbx\n";
    }
} else {
            m_errorManager.addError({"Unknown ArithOp: ", node->value, "CodeGeneration", std::stoi(node->line)});
        }
    } else if (node->type == "TermOp") { // Handles *, //, /, %
        if (node->children.size() < 2) {
             m_errorManager.addError({"TermOp requires two children", node->value, "CodeGeneration", std::stoi(node->line)});
             return;
        }
        visitNode(node->children[0]);
        textSection += "    push rax\n";
        visitNode(node->children[1]);
        textSection += "    mov rbx, rax\n"; // Denominator/second operand
        textSection += "    pop rax\n";      // Numerator/first operand

        std::string typeL = getExpressionType(node->children[0]);
        std::string typeR = getExpressionType(node->children[1]);

		if (typeL == "auto") typeL = "Integer";
if (typeR == "auto") typeR = "Integer";

        if (typeL != "Integer" || typeR != "Integer") {
            m_errorManager.addError({"TermOp requires Integer operands. Got: ", typeL + ", " + typeR, "CodeGeneration", std::stoi(node->line)});
            textSection += "    mov rax, 0 ; Error for TermOp\n";
            return;
        }

        if (node->value == "*") {
            textSection += "    imul rax, rbx\n";
        } else if (node->value == "/" || node->value == "//") { // Integer division
            textSection += "    cmp rbx, 0\n";
            textSection += "    je division_by_zero_error\n";
            textSection += "    cqo ; Sign-extend rax into rdx:rax for idiv\n";
            textSection += "    idiv rbx\n"; // Quotient in rax, remainder in rdx
        } else if (node->value == "%") {
            textSection += "    cmp rbx, 0\n";
            textSection += "    je division_by_zero_error\n";
            textSection += "    cqo\n";
            textSection += "    idiv rbx\n";
            textSection += "    mov rax, rdx\n"; // Remainder is the result
        } else {
            m_errorManager.addError({"Unknown TermOp: ", node->value, "CodeGeneration", std::stoi(node->line)});
        }
    } else if (node->type == "And") {
        std::string falseLbl = newLabel("and_false_or_rhs");
        std::string endLbl = newLabel("and_end");
        visitNode(node->children[0]); // Eval left
        textSection += "    push rax ; Save left value\n";
        toBool("rax");
        textSection += "    cmp rax, 0\n";    // if left is false
        textSection += "    je " + falseLbl + "\n"; // then result is left value (already on stack), skip right
        // Left was true, result is right value
        textSection += "    pop rax ; Discard left value from stack\n";
        visitNode(node->children[1]); // Eval right, result in rax
        textSection += "    jmp " + endLbl + "\n";
        textSection += falseLbl + ":\n";
        // Result is the left value, which is on top of stack
        textSection += "    pop rax ; Left value (which was false-equivalent) is the result\n";
        textSection += endLbl + ":\n";
    } else if (node->type == "Or") {
        std::string trueLbl = newLabel("or_true_skip_rhs");
        std::string endLbl = newLabel("or_end");
        visitNode(node->children[0]); // Eval left
        textSection += "    push rax ; Save left value\n";
        toBool("rax");
        textSection += "    cmp rax, 1\n";    // if left is true
        textSection += "    je " + trueLbl + "\n";  // then result is left value, skip right
        // Left was false, result is right value
        textSection += "    pop rax ; Discard left value from stack\n";
        visitNode(node->children[1]); // Eval right, result in rax
        textSection += "    jmp " + endLbl + "\n";
        textSection += trueLbl + ":\n";
        // Result is the left value, which is on top of stack
        textSection += "    pop rax ; Left value (which was true-equivalent) is the result\n";
        textSection += endLbl + ":\n";
    } else if (node->type == "Not") {
        visitNode(node->children[0]);
        toBool("rax");
        textSection += "    xor rax, 1\n"; // Invert boolean value (0 to 1, 1 to 0)
    } else if (node->type == "UnaryOp" && node->value == "-") {
         if (node->children.empty()) {
            m_errorManager.addError({"Unary '-' has no operand.", "", "CodeGeneration", std::stoi(node->line)});
            return;
         }
         visitNode(node->children[0]);
         std::string operandType = getExpressionType(node->children[0]);
         if (operandType == "Integer") {
            textSection += "    neg rax\n";
         } else {
            m_errorManager.addError({"Unary '-' expects Integer operand, got: ", operandType, "CodeGeneration", std::stoi(node->line)});
            textSection += "    mov rax, 0 ; Error for unary -\n";
         }
    }
    // For Program, Definitions, Instructions, FunctionBody, FormalParameterList, ActualParameterList etc.
    else if (node->type == "Program" || node->type == "Definitions" || node->type == "Instructions" ||
             node->type == "FunctionBody" || node->type == "FormalParameterList" ||
             node->type == "ActualParameterList" || node->type == "ParameterList" || // ParameterList is used by parser for func calls
             node->type == "IfBody" || node->type == "ElseBody" || node->type == "ForBody" ) {
        for (auto &child : node->children) {
            visitNode(child);
        }
    }else if (node->type == "ListCall") {           // read  L[i]  -> rax
    auto listId  = node->children[0];          // Identifier
    auto indexNd = node->children[1];          // expression for i

    // evaluate the index, result in rax
    visitNode(indexNd);
    textSection += "    push rax        ; save index\n";

    // load list base address in rbx
    textSection += "    mov rbx, " + getIdentifierMemoryOperand(listId->value) + "\n";
    textSection += "    pop  rcx        ; rcx = index\n";

    std::string bad = newLabel("index_error_read");
    std::string ok  = newLabel("index_ok");

    textSection += "    cmp rcx, 0\n";
    textSection += "    jl " + bad + "\n";
    textSection += "    cmp rcx, [rbx]\n";
    textSection += "    jge " + bad + "\n";

    textSection += "    add rbx, 8      ; skip size field\n";
    textSection += "    imul rcx, 8\n";
    textSection += "    add rbx, rcx\n";
    textSection += "    mov rax, [rbx]  ; <- element value\n";
    textSection += "    jmp " + ok + "\n";

    textSection += bad + ":\n";
    textSection += "    mov rax, 1\n";
    textSection += "    mov rdi, 1\n";
    textSection += "    mov rsi, index_error_msg\n";
    textSection += "    mov rdx, index_error_len\n";
    textSection += "    syscall\n";
    textSection += "    mov rax, 60\n";
    textSection += "    mov rdi, 1\n";
    textSection += "    syscall\n";

    textSection += ok + ":\n";
}
     else {
        m_errorManager.addError({"Unrecognized or unhandled ASTNode type in visitNode: ", node->type, "CodeGeneration", std::stoi(node->line)});
        // Default: visit children if any, though this might not be correct for all unhandled types.
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
    textSection += "division_by_zero_error:\n";
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
    textSection += "    push rbx          ; Preserve callee-saved rbx\n";
    textSection += "    push r12          ; Preserve callee-saved r12\n";
    textSection += "    mov r12, rax      ; save original number in r12 (after r12 is saved)\n";

    // Handle negative numbers
    textSection += "    cmp r12, 0\n";
    textSection += "    jge .print_positive\n";
    textSection += "    ; Handle negative number\n";
    textSection += "    push rax          ; Syscalls clobber rax, rdi, rsi, rdx. r12 is safe here.\n";
    textSection += "    push rdi\n";
    textSection += "    push rsi\n";
    textSection += "    push rdx\n";
    textSection += "    mov rax, 1\n";
    textSection += "    mov rdi, 1\n";
    textSection += "    mov rsi, minus_sign\n";
    textSection += "    mov rdx, 1\n";
    textSection += "    syscall\n";
    textSection += "    pop rdx\n";
    textSection += "    pop rsi\n";
    textSection += "    pop rdi\n";
    textSection += "    pop rax\n";
    textSection += "    neg r12           ; Make the number positive. r12 was preserved.\n";

    textSection += ".print_positive:\n";
    textSection += "    lea rdi, [buffer+31]  ; point rdi to end of buffer\n";
    textSection += "    mov byte [rdi], 0     ; null-terminate the buffer\n";
    textSection += "    cmp r12, 0\n";
    textSection += "    jne .print_convert\n";
    textSection += "    mov byte [rdi-1], '0'\n";
    textSection += "    lea rdi, [rdi-1]\n";
    textSection += "    jmp .print_output\n";
     
    textSection += ".print_convert:\n";
    textSection += "    mov rax, r12          ; Use r12 (which holds the number) for conversion\n";
    textSection += "    mov rbx, 10           ; Divisor (rbx was saved)\n";
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
    textSection += "    syscall             ; rax, rdi, rsi, rdx might be clobbered\n";

    textSection += "    pop r12           ; Restore callee-saved r12\n";
    textSection += "    pop rbx           ; Restore callee-saved rbx\n";
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

        // --- revised print_not_string ----------------------------------------
    // Remplacer la fonction print_not_string par celle-ci:

textSection += "; Function to print a list (elements separated by space, enclosed in brackets)\n";
textSection += "print_not_string:          ; RAX = address of list\n";
textSection += "    push rbp\n";
textSection += "    mov  rbp, rsp\n";
textSection += "    push rbx\n";
textSection += "    push r12            ; will be our loop-counter\n";
textSection += "    push rdx\n";
textSection += "    push rsi\n";
textSection += "    push rdi\n";
textSection += "    push r8\n";
textSection += "    push r9\n";

textSection += "    mov  rbx, rax          ; rbx = base address of the list structure\n";
textSection += "    mov  r12, [rbx]        ; r12 = size of the list\n";
textSection += "    add  rbx, 8            ; rbx -> first element\n";

// print '['
textSection += "    mov  rax, 1\n";
textSection += "    mov  rdi, 1\n";
textSection += "    mov  rsi, open_bracket\n";
textSection += "    mov  rdx, 1\n";
textSection += "    syscall\n";

textSection += ".print_list_loop:\n";
textSection += "    cmp  r12, 0\n";
textSection += "    je   .print_list_done\n";

textSection += "    mov  r8, [rbx]         ; element -> r8\n";
textSection += "    ; Check if element is a likely pointer (above certain address threshold)\n";
textSection += "    cmp  r8, 0x1000        ; Addresses below this are likely integers\n";
textSection += "    jb   .print_as_number\n";

textSection += "    mov  rax, r8\n";
textSection += "    ; Try to read the first byte safely\n";
textSection += "    push rcx\n";
textSection += "    mov  rcx, r8\n";
textSection += "    shr  rcx, 48           ; Get top 16 bits - if non-zero, likely invalid addr\n";
textSection += "    cmp  rcx, 0\n";
textSection += "    jne  .not_a_string\n";
textSection += "    pop  rcx\n";

textSection += "    ; Now test if this might be a string (has ASCII characters and null-terminator)\n";
textSection += "    push rcx\n";
textSection += "    xor  rcx, rcx\n";
textSection += ".check_string_loop:\n";
textSection += "    cmp  rcx, 20           ; Limite max: 20 caractères à vérifier\n";
textSection += "    jge  .not_a_string\n";
textSection += "    mov  r9b, byte [rax+rcx]\n";
textSection += "    cmp  r9b, 0            ; Fin de la chaîne?\n";
textSection += "    je   .print_as_string\n";
textSection += "    cmp  r9b, 32\n";
textSection += "    jl   .not_a_string\n";
textSection += "    cmp  r9b, 126\n";
textSection += "    jg   .not_a_string\n";
textSection += "    inc  rcx\n";
textSection += "    jmp  .check_string_loop\n";

textSection += ".not_a_string:\n";
textSection += "    pop  rcx\n";
textSection += "    jmp  .print_as_number\n";

textSection += ".print_as_string:\n";
textSection += "    pop  rcx\n";
textSection += "    mov  rax, r8\n";
textSection += "    call print_string\n";
textSection += "    jmp  .after_element_print\n";

textSection += ".print_as_number:\n";
textSection += "    mov  rax, r8\n";
textSection += "    call print_number\n";

textSection += ".after_element_print:\n";
textSection += "    add  rbx, 8\n";
textSection += "    dec  r12\n";
textSection += "    cmp  r12, 0\n";
textSection += "    je   .print_list_done\n";

// print ", "
textSection += "    mov  rax, 1\n";
textSection += "    mov  rdi, 1\n";
textSection += "    mov  rsi, comma_space\n";
textSection += "    mov  rdx, 2\n";
textSection += "    syscall\n";
textSection += "    jmp  .print_list_loop\n";

textSection += ".print_list_done:\n";
// print ']'
textSection += "    mov  rax, 1\n";
textSection += "    mov  rdi, 1\n";
textSection += "    mov  rsi, close_bracket\n";
textSection += "    mov  rdx, 1\n";
textSection += "    syscall\n";

textSection += "    pop  r9\n";
textSection += "    pop  r8\n";
textSection += "    pop  rdi\n";
textSection += "    pop  rsi\n";
textSection += "    pop  rdx\n";
textSection += "    pop  r12\n";
textSection += "    pop  rbx\n";
textSection += "    pop  rbp\n";
textSection += "    ret\n";
    // ----------------------------------------------------------------------

    // Legacy alias for print_list to keep old code working, now points to the real implementation
    bool aliasExists = textSection.find("print_list:\n    jmp print_not_string") != std::string::npos;
    if (!aliasExists) { // Add if not already present from a previous patch
        textSection += "\n; legacy alias – keep older code paths working\n";
        textSection += "print_list:\n";
        textSection += "    jmp print_not_string\n\n";
    }
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
        // m_errorManager.addError({"Invalid ASTNode structure for assignment", "", "CodeGeneration", std::stoi(node->line)}); // Already have throw
        throw std::runtime_error("Invalid ASTNode structure for assignment on line " + node->line);
    }
    
    std::string varName = node->children[0]->value; 
    auto leftNode = node->children[0];
    auto rightValueNode = node->children[1];

    // Evaluate the right-hand side first, result will be in rax
    visitNode(rightValueNode);

    if (leftNode->type == "ListCall") {
        // ... existing ListCall assignment logic ...
        std::string listName = leftNode->children[0]->value;
        auto indexNode = leftNode->children[1];
        textSection += "; List element assignment for " + listName + "\n";
        textSection += "    mov rbx, " + getIdentifierMemoryOperand(listName) + "\n"; // Base address of list in rbx

        textSection += "    push rax\n"; // Save value to be assigned (from RHS)
        visitNode(indexNode); // Index in rax
        textSection += "    mov rcx, rax\n"; // Save index in rcx
        textSection += "    pop rax\n"; // Restore value to be assigned into rax

        textSection += "    ; Check if index is valid\n";
        textSection += "    cmp rcx, 0\n"; 
        std::string errorLabel = newLabel("index_error"); // Use member newLabel
        std::string endLabel = newLabel("end_list_assign");

        textSection += "    jl " + errorLabel + "\n";
        textSection += "    cmp rcx, [rbx]\n";  
        textSection += "    jge " + errorLabel + "\n";
        
        textSection += "    ; Calculate element address\n";
        textSection += "    add rbx, 8          ; Skip size qword\n"; 
        textSection += "    imul rcx, 8         ; Multiply index by 8 (size of qword)\n";
        textSection += "    add rbx, rcx        ; Add to base address of elements\n";
        
        textSection += "    ; Store value in list element\n";
        textSection += "    mov qword [rbx], rax\n";
        textSection += "    jmp " + endLabel + "\n";
        
        textSection += errorLabel + ":\n";
        textSection += "    mov rax, 1\n";
        textSection += "    mov rdi, 1\n";
        textSection += "    mov rsi, index_error_msg\n";
        textSection += "    mov rdx, index_error_len\n";
        textSection += "    syscall\n";
        textSection += "    mov rax, 60\n"; 
        textSection += "    mov rdi, 1\n";  
        textSection += "    syscall\n";
        
        textSection += endLabel + ":\n";
        // Note: Type update for list elements is not handled here.
        // The list itself retains its type "List". Element types are dynamic.
        return;
    }
    
    // Standard variable assignment
    textSection += "    mov " + getIdentifierMemoryOperand(varName) + ", rax\n";
    
    // Update type of the variable in the symbol table
    std::string valueType = getExpressionType(rightValueNode);
	
    if (valueType != "auto" && valueType != "autoFun") { // Only update if a concrete type is known
        updateSymbolType(varName, valueType);
    } else if (rightValueNode->type == "Integer") { // Explicit override for literals
        updateSymbolType(varName, "Integer");
    } else if (rightValueNode->type == "String") {
        updateSymbolType(varName, "String");
    } else if (rightValueNode->type == "List") {
        updateSymbolType(varName, "List");
    } else if (rightValueNode->type == "True" || rightValueNode->type == "False") {
        updateSymbolType(varName, "Boolean");
    }
    // If it's a function call and inferFunctionReturnType gave autoFun,
    // we might not want to overwrite a potentially more specific type of varName
    // unless varName itself was 'auto'.
}


void CodeGenerator::genFor(const std::shared_ptr<ASTNode>& node) {
    if (node->children.size() < 3) {
        m_errorManager.addError({"Invalid For ASTNode structure: requires variable, iterable, and body.", "", "CodeGeneration", std::stoi(node->line)});
        return;
    }

    auto loopVarNode = node->children[0];
    auto iterableNode = node->children[1];
    auto bodyNode = node->children[2];

    if (loopVarNode->type != "Identifier") {
        m_errorManager.addError({"For loop variable must be an Identifier.", "", "CodeGeneration", std::stoi(loopVarNode->line)});
        return;
    }
    std::string loopVarName = loopVarNode->value;
    std::string loopVarMem = getIdentifierMemoryOperand(loopVarName);

    auto type = getExpressionType(iterableNode);
    if (iterableNode->type == "FunctionCall" && 
        !iterableNode->children.empty() &&
        iterableNode->children[0]->type == "Identifier" &&
        iterableNode->children[0]->value == "range") {

        if (iterableNode->children.size() < 2 || iterableNode->children[1]->type != "ParameterList" || iterableNode->children[1]->children.empty()) {
            m_errorManager.addError({"Invalid 'range' call in for loop: ParameterList expected.", "", "CodeGeneration", std::stoi(iterableNode->line)});
            return;
        }
        
        auto paramListNode = iterableNode->children[1];
        if (paramListNode->children.size() != 1) {
            m_errorManager.addError({"range() in for loop expects exactly one argument.", "", "CodeGeneration", std::stoi(paramListNode->line)});
            return;
        }
        auto rangeArgNode = paramListNode->children[0];

        std::string startLabel = newLabel("for_start"); // Use member newLabel
        std::string endLabel = newLabel("for_end");

        // Initialize loop variable i = 0
        textSection += "    ; Initialize loop variable " + loopVarName + " = 0\n";
        textSection += "    mov qword " + loopVarMem + ", 0\n";

        // Evaluate range limit N, store it on stack
        textSection += "    ; Evaluate range limit for " + loopVarName + "\n";
        visitNode(rangeArgNode); // Limit in rax
        textSection += "    push rax          ; Push range limit N onto stack\n";

        textSection += startLabel + ":\n";
        // Condition: i < N
        textSection += "    mov rax, " + loopVarMem + "   ; Load i into rax\n";
        textSection += "    mov rbx, [rsp]      ; Load N from stack into rbx (rsp still points to N)\n";
        textSection += "    cmp rax, rbx\n";
        textSection += "    jge " + endLabel + "     ; if i >= N, jump to end_for\n";

        // Loop body
        textSection += "    ; Loop body for " + loopVarName + "\n";
        visitNode(bodyNode);

        // Increment: i = i + 1
        textSection += "    ; Increment " + loopVarName + "\n";
        textSection += "    mov rax, " + loopVarMem + "\n";
        textSection += "    inc rax\n";
        textSection += "    mov " + loopVarMem + ", rax\n";
        textSection += "    jmp " + startLabel + "\n";

        textSection += endLabel + ":\n";
        textSection += "    add rsp, 8        ; Pop range limit N from stack\n";

    } 
    else{
        std::string startLabel = newLabel("for_list_start");
        std::string endLabel = newLabel("for_list_end");
        
        visitNode(iterableNode);
        
        textSection += "    ; Itération sur liste\n";
        textSection += "    push rax          ; Sauvegarder l'adresse de la liste\n";
        textSection += "    mov rbx, rax      ; rbx = adresse de la liste\n";
        textSection += "    xor rcx, rcx      ; rcx = 0 (compteur d'itération)\n";
        
        textSection += startLabel + ":\n";
        textSection += "    mov rbx, [rsp]    ; rbx = adresse de la liste (depuis la pile)\n";
        textSection += "    mov rdx, [rbx]    ; rdx = taille de la liste\n";
        textSection += "    cmp rcx, rdx      ; comparer compteur avec taille\n";
        textSection += "    jge " + endLabel + "    ; si compteur >= taille, sortir\n";
        
        textSection += "    ; Récupérer l'élément courant\n";
        textSection += "    lea r8, [rbx + 8]   ; r8 = adresse du premier élément\n";
        textSection += "    mov r9, rcx        ; r9 = index courant\n";
        textSection += "    imul r9, 8         ; r9 = décalage en octets (index * 8)\n";
        textSection += "    mov rax, [r8 + r9] ; rax = liste[rcx] (élément courant)\n";
        textSection += "    mov " + loopVarMem + ", rax ; assigner à la variable de boucle\n";
        
        textSection += "    push rcx           ; Sauvegarder le compteur d'itération\n";
        
        textSection += "    ; Corps de la boucle for\n";
        visitNode(bodyNode);
        
        textSection += "    pop rcx            ; Restaurer le compteur\n";
        
        textSection += "    inc rcx            ; Incrémenter l'index\n";
        textSection += "    jmp " + startLabel + "\n";
        
        textSection += endLabel + ":\n";
        textSection += "    add rsp, 8        ; Libérer l'adresse de la liste\n";
    }
    
}

void CodeGenerator::genIf(const std::shared_ptr<ASTNode>& node) {
    // Generate unique labels for this if statement
    // static int ifCounter = 0; // Already uses this->ifLabelCounter in full code
    std::string ifId = std::to_string(this->ifLabelCounter++);
    std::string elseLabel = ".else_" + ifId;
    std::string endLabel = ".endif_" + ifId;
    
    // Generate code for the condition expression
    textSection += "; If condition\n";
    auto condNode = node->children[0];
    std::string condType = getExpressionType(condNode);
    
    visitNode(condNode); // rax <- valeur
    
    // L = [] ou L = "" ou L = 0 => false
    if (condType == "List") {
        textSection += "    ; Check if list is empty\n";
        textSection += "    cmp qword [rax], 0    ; Check size at first qword\n";
        textSection += "    setnz al              ; al = 1 if list is not empty (size > 0)\n";
        textSection += "    movzx rax, al         ; rax = 0/1\n";
    } else if (condType == "String") {
        textSection += "    ; Check if string is empty\n";
        textSection += "    cmp byte [rax], 0     ; Check if first byte is null\n";
        textSection += "    setnz al              ; al = 1 if string is not empty\n";
        textSection += "    movzx rax, al         ; rax = 0/1\n";
    } else {
        toBool("rax");            // rax <- 0 / 1
    }   
    
    // Test if the condition is false (0)
    textSection += "cmp rax, 0\n";
    
    if (node->children.size() > 2 && node->children[2]) { // Has an else block
        textSection += "    je " + elseLabel + "\n"; // si faux on saute to else
    } else { // No else block
        textSection += "    je " + endLabel + "\n"; // si faux on saute to end
    }
    
    // Generate code for IfBody
    textSection += "; If body\n";
    visitNode(node->children[1]);
    
    if (node->children.size() == 2) { // No else branch, so jump to end after then-body
        textSection += "    jmp " + endLabel + "\n";
    }
    
    // Skip if IfBody / Handle Else Body
    if (node->children.size() > 2 && node->children[2]) { // Has an else block
        textSection += "    jmp " + endLabel + "\n"; // After then-body, jump to end (if not already done for no-else case)
        
        textSection += elseLabel + ":\n";
        textSection += "; Else body\n";
        visitNode(node->children[2]);
    }
    
    textSection += endLabel + ":\n";
}

void CodeGenerator::genFunction(const std::shared_ptr<ASTNode>& node) {
    std::string funcName = node->value;
    FunctionSymbol* funcSym = nullptr;
	currentFuncSym = funcSym;
    if (symbolTable) { // Assuming global symbol table
        Symbol* sym = symbolTable->findImmediateSymbol(funcName);
        funcSym = dynamic_cast<FunctionSymbol*>(sym);
    }

    if (!funcSym) {
        m_errorManager.addError({"Function symbol not found for: ", funcName, "CodeGeneration", std::stoi(node->line)});
        // Potentially add a dummy function body to prevent cascading assembler errors
        textSection += "\n" + funcName + ":\n";
        textSection += "    ; ERROR: Function symbol not found\n";
        textSection += "    ret\n";
        return;
    }
    
    // int frameSizeForSub = funcSym->frameSize; // This is the total size for locals + padding for callee-saved
                                              // The actual 'sub rsp' should be for locals + alignment for the call itself if needed
                                              // The frameSize in FunctionSymbol is calculated considering callee-saved registers for alignment.
                                              // So, this frameSize should be what RSP is decremented by AFTER rbp is pushed and RSP is moved to RBP,
                                              // and BEFORE callee-saved are pushed.
                                              // The current calculation of frameSize in SymbolTableGenerator is:
                                              // addedFuncSym->frameSize = localsTotalSize + padding_needed;
                                              // where padding_needed aligns (localsTotalSize + callee_saved_bytes).
                                              // This means 'sub rsp, frameSize' is for locals and their alignment relative to RBP after callee-saved are pushed.

    textSection += "\n" + funcName + ":\n";
    // Prologue
    textSection += "    push rbp\n";
    textSection += "    mov rbp, rsp\n";
    
    // Space for locals is allocated from RBP downwards.
    // The frameSize calculated in SymbolTable includes padding for alignment
    // considering callee-saved registers that will be pushed *after* this sub rsp.
    // So, the value from funcSym->frameSize should be correct for 'sub rsp, X'
    // if it represents the space needed for locals and padding to align the stack
    // *before* pushing callee-saved registers, such that *after* pushing them,
    // RSP is 16-byte aligned for further calls.
    // Let's assume funcSym->frameSize is the amount to subtract for locals and padding.
    if (funcSym->frameSize > 0) {
        textSection += "    sub rsp, " + std::to_string(funcSym->frameSize) + " ; Allocate space for locals and stack alignment\n";
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

   auto args = node->children[1];

   if (funcName != "len"   && funcName != "range" &&  // built-ins
    funcName != "list"  && funcName != "print")
{
    std::vector<std::shared_ptr<ASTNode>> argsList =
        args ? args->children : std::vector<std::shared_ptr<ASTNode>>{};
    updateFunctionParamTypes(funcName, argsList);
}
   
    if (funcName == "list"){
        if (args->children.size() == 1 && 
            args->children[0]->type == "FunctionCall" && 
            args->children[0]->children[0]->value == "range"){
            
            auto rangeArgs = args->children[0]->children[1];
            visitNode(rangeArgs->children[0]);
            textSection += "call list_range\n";
            return; 

        }
    }
    if (funcName == "len"){
        if (args->children.size() == 1){
            auto param = args->children[0];
            visitNode(param);  
            auto type0 = param->type;
            if (type0 == "Identifier") {
                type0 = getIdentifierType(param->value);
            } else if (type0 == "FunctionCall") {
                type0 = inferFunctionReturnType(this->rootNode, param->children[0]->value);
                
            }
            if (type0 == "auto") {
                // Assume it's a list that we don't know the static type of yet.
                textSection += "    mov rax, [rax]      ; len(auto-list)\n";
                return;
            }
                    if (type0 != "String" && type0 != "List") {
                        m_errorManager.addError(Error{
                            "len Error; ", 
                            "Used len on non-list or non-string variable",
                            "Semantics", 
                            std::stoi(node->line)
                        });
                        return;
                    }



                if (type0 == "List") {
                    textSection += "mov rax, [rax]  ; Taille de la liste\n";
                } else { // String
                    textSection += "mov rsi, rax    ; rsi = adresse de la chaîne\n";
                    textSection += "mov rax, 0      ; rax = compteur\n";
                    textSection += ".len_strlen_loop:\n";
                    textSection += "cmp byte [rsi+rax], 0\n";
                    textSection += "je .len_strlen_done\n";
                    textSection += "inc rax\n";
                    textSection += "jmp .len_strlen_loop\n";
                    textSection += ".len_strlen_done:\n";
                }
                
                return; 
            }
            else{
                m_errorManager.addError(Error{
                    "len Error; ", 
                    "Too many arguments for len()",
                    "Semantics", 
                    std::stoi(node->line)
                });
                return;
            }
    }
    
    int argCount = 0;
    const std::vector<std::shared_ptr<ASTNode>>* argListPtr = nullptr;

	

    if (node->children.size() > 1 && (node->children[1]->type == "ParameterList" || node->children[1]->type == "ActualParameterList")) {
        argListPtr = &node->children[1]->children;
        argCount = argListPtr->size();
    }

	if (argListPtr)
        updateFunctionParamTypes(funcName, *argListPtr);

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
	if (currentFuncSym && currentFuncSym->returnType == "autoFun") {
    currentFuncSym->returnType = getExpressionType(node->children[0]);
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
    std::string t = getIdentifierType(name); // Ensure getIdentifierType is robust
    return t == "Integer" || t == "auto" || t == "autoFun";
}

void CodeGenerator::updateSymbolType(const std::string& name, const std::string& newType) {
    if (!symbolTable) return;

    SymbolTable* scopeToSearch = currentSymbolTable ? currentSymbolTable : symbolTable;
    if (scopeToSearch) {
        Symbol* s = scopeToSearch->findSymbol(name); // findSymbol searches current then parent scopes
        if (s && (s->symCat == "variable" || s->symCat == "parameter")) {
            if (auto v = dynamic_cast<VariableSymbol*>(s)) {
                if (v->type != "auto" && v->type != "autoFun") { // Keep the first real type
                    return;
                }
            }
        }
    }

    std::function<bool(SymbolTable*, const std::string&, const std::string&)> updateInTableHierarchy = 
        [&](SymbolTable* table, const std::string& symName, const std::string& typeToSet) -> bool {
        while (table) {
            for (auto& sym_ptr : table->symbols) {
                if (sym_ptr->name == symName && (sym_ptr->symCat == "variable" || sym_ptr->symCat == "parameter")) {
                    if (auto varSym = dynamic_cast<VariableSymbol*>(sym_ptr.get())) {
                        varSym->type = typeToSet;
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
        if (updateInTableHierarchy(currentSymbolTable, name, newType)) {
            return;
        }
    }
    // If not found or not in a function, try global scope hierarchy
    updateInTableHierarchy(symbolTable, name, newType);
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

	if (funcName == "list" || funcName == "range") return "List";
    
    return "Integer"; // Type par défaut
}

std::string CodeGenerator::getExpressionType(const std::shared_ptr<ASTNode>& node) {
	
    if (!node) return "auto"; // Or throw an error
	if (node->type == "auto" || node->type == "autoFun") return "Integer";
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
            if (node->children[0]->value == "len") return "Integer"; // len() always returns Integer
            if (node->children[0]->value == "print") return "void"; // print() doesn't return a value
            if (node->children[0]->value == "list") return "List"; // list() returns a List
            if (node->children[0]->value == "range") return "List"; // str() returns a String
            return inferFunctionReturnType(this->rootNode, node->children[0]->value);
        }
        return "autoFun"; // Default for function calls if specific type can't be inferred yet
    } else if (node->type == "List") {
        return "List";
    } else if (node->type == "ListCall") {
    // assume lists of integers for now
    return "Integer";
}else if (node->type == "ArithOp" || node->type == "TermOp") {
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

void CodeGenerator::updateFunctionParamTypes(
        const std::string&           funcName,
        const std::vector<std::shared_ptr<ASTNode>>& actualArgs)
{
    /* 1) locate the FunctionSymbol in the global scope */
    auto *fs = dynamic_cast<FunctionSymbol*>(symbolTable
                                            ? symbolTable->findSymbol(funcName)
                                            : nullptr);
    if (!fs) return;                       // unknown function (built-ins etc.)

    /* 2) locate the symbol-table that holds the parameters          */
    SymbolTable *funcScope = nullptr;
    for (auto &sub : symbolTable->children)
        if (sub->scopeName == ("function " + funcName)) {
            funcScope = sub.get();
            break;
        }
    if (!funcScope) return;                // should not happen

    /* 3) zip(actualArgs, parameters) and fix the “auto” holes       */
    std::size_t argIdx = 0;
    for (auto &symPtr : funcScope->symbols) {
        if (argIdx >= actualArgs.size()) break;          // too many formals
        //if (symPtr->symCat != "parameter")   continue;   // locals start here

        auto *param = dynamic_cast<VariableSymbol*>(symPtr.get());
        if (!param) continue;                           // safety

        /* infer dynamic type flowing through the call */
        std::string argType = getExpressionType(actualArgs[argIdx]);
        if ((argType == "auto" || argType == "autoFun") &&
            actualArgs[argIdx]->type == "Identifier")
        {
            /* for  x  we really want the *declared* type of x, not   *
             * “Identifier”.                                         */
            argType = getIdentifierType(actualArgs[argIdx]->value);
        }

        /* promote: only if still unknown */
        if (param->type == "auto" && argType != "auto" && !argType.empty())
            param->type = argType;

        ++argIdx;
        if (argIdx >= actualArgs.size()) break; // too many formals
    }
}

#include "codeGenerator.h"
#include <fstream>
#include <stdexcept>
#include <cstdlib>
#include <sstream>

void CodeGenerator::generateCode(const std::shared_ptr<ASTNode>& root, const std::string& filename) {
    // Initialize our sections.
    dataSection = "";
    textSection = "";
    std::string functionSection = ""; // Add a separate section for functions
    declaredVars.clear();
    
    // Generate the text (and data) from the AST.
    startAssembly();
    
    // Separate main instructions from functions
    for (const auto& child : root->children) {
        if (child->type == "Definitions") {
            // Process function definitions
            for (const auto& def : child->children) {
                if (def->type == "FunctionDefinition") {
                    // Save functions in separate section
                    std::string oldText = textSection;
                    textSection = "";
                    visitNode(def);
                    functionSection += textSection;
                    textSection = oldText;
                }
            }
        } else {
            // Process main program instructions
            visitNode(child);
        }
    }
    
    endAssembly();

    // Build the final assembly code.
    std::stringstream finalAsm;
    finalAsm << "global _start\n\n";
    
    // Data section: our variable declarations plus newline.
    finalAsm << "section .data\n";
    finalAsm << dataSection;
    finalAsm << "newline: db 10\n";
    finalAsm << "minus_sign: db '-'\n\n";
    
    // BSS section: for our conversion buffer.
    finalAsm << "section .bss\n";
    finalAsm << "buffer: resb 32\n\n";
    
    // Text section
    finalAsm << "section .text\n";
    finalAsm << "_start:\n";
    finalAsm << textSection;
    finalAsm << "\n; Functions\n";
    finalAsm << functionSection;
    
    asmCode = finalAsm.str();
    
    // Write the final assembly code to file.
    writeToFile(filename);
}

void CodeGenerator::startAssembly() {
    // Nothing to do here for now.
}

void CodeGenerator::visitNode(const std::shared_ptr<ASTNode>& node) {
    if (!node) return;
    
    if (node->type == "Affect") {
        genAffect(node);
    } else if (node->type == "FunctionDefinition") {
        genFunction(node);
    } else if (node->type == "FunctionCall") {
        genFunctionCall(node);
    } else if (node->type == "Return") {
        genReturn(node);
    } else if (node->type == "For") {
            genFor(node);
    } else if (node->type == "If") {
        genIf(node);
    } else if (node->type == "Print") {
        // Visit children of the print node (which should compute an expression)
        for (const auto& child : node->children) {
            visitNode(child);
        }
        genPrint();
        // Optionally, print a newline after printing the value.
        textSection += "mov rax, 1\n";
        textSection += "mov rdi, 1\n";
        textSection += "mov rsi, newline\n";
        textSection += "mov rdx, 1\n";
        textSection += "syscall\n";
    } else if (node->type == "Compare") {
        // Handle comparison operations
        visitNode(node->children[0]);  // Evaluate left operand
        textSection += "push rax\n";   // Save left operand
        visitNode(node->children[1]);  // Evaluate right operand
        textSection += "mov rbx, rax\n"; // Move right operand to rbx
        textSection += "pop rax\n";    // Restore left operand to rax
        
        // Now compare rax (left) with rbx (right)
        textSection += "cmp rax, rbx\n";
        
        // Set rax to 1 (true) or 0 (false) based on the comparison
        if (node->value == "==") {
            textSection += "mov rax, 0\n";
            textSection += "sete al\n";  // Set al if equal
        } else if (node->value == "!=") {
            textSection += "mov rax, 0\n";
            textSection += "setne al\n"; // Set al if not equal
        } else if (node->value == "<") {
            textSection += "mov rax, 0\n";
            textSection += "setl al\n";  // Set al if less than
        } else if (node->value == ">") {
            textSection += "mov rax, 0\n";
            textSection += "setg al\n";  // Set al if greater than
        } else if (node->value == "<=") {
            textSection += "mov rax, 0\n";
            textSection += "setle al\n"; // Set al if less than or equal
        } else if (node->value == ">=") {
            textSection += "mov rax, 0\n";
            textSection += "setge al\n"; // Set al if greater than or equal
        }
    } else if (node->type == "UnaryOp") {
        // Visit the operand
        visitNode(node->children[0]);
        
        if (node->value == "-") {
            // Negate the value
            textSection += "neg rax\n";
        } 
        // Add other unary operators as needed
    
    } else if (node->type == "ArithOp") {
        // The node's value field holds the operator ("+")
        if (node->value == "+") {
            // Evaluate the left child.
            visitNode(node->children[0]);
            textSection += "push rax\n";
            // Evaluate the right child.
            visitNode(node->children[1]);
            textSection += "pop rbx\n";
            textSection += "add rax, rbx\n";
        }
        else if (node->value == "-") {
            // Evaluate the left child
            visitNode(node->children[0]);
            textSection += "push rax\n";
            // Evaluate the right child
            visitNode(node->children[1]);
            textSection += "mov rbx, rax\n";
            textSection += "pop rax\n";
            textSection += "sub rax, rbx\n";
        }
    
    } else if (node->type == "TermOp"){
        if (node->value == "*") {
            // Evaluate the left child
            visitNode(node->children[0]);
            textSection += "push rax\n";
            // Evaluate the right child
            visitNode(node->children[1]);
            textSection += "mov rbx, rax\n";
            textSection += "pop rax\n";
            textSection += "imul rax, rbx\n";
        }
        else if (node->value == "//") {
            // Evaluate the left child
            visitNode(node->children[0]);
            textSection += "push rax\n";
            // Evaluate the right child
            visitNode(node->children[1]);
            textSection += "mov rbx, rax\n";
            textSection += "pop rax\n";
            textSection += "xor rdx, rdx\n";  // Clear RDX before division
            textSection += "div rbx\n";       // RAX = RAX / RBX, RDX = RAX % RBX
        }
        else if (node->value == "%") {
            // Evaluate the left child
            visitNode(node->children[0]);
            textSection += "push rax\n";
            // Evaluate the right child
            visitNode(node->children[1]);
            textSection += "mov rbx, rax\n";
            textSection += "pop rax\n";
            textSection += "xor rdx, rdx\n";  // Clear RDX before division
            textSection += "div rbx\n";       // RAX = RAX / RBX, RDX = RAX % RBX
            textSection += "mov rax, rdx\n";  // Move remainder to RAX
        }
    } else if (node->type == "Identifier") {
        // Generate code to load the variable's value from memory into rax.
        textSection += "mov rax, qword [" + node->value + "]\n";
    } else if (node->type == "Integer") {
        // Generate code to load an immediate integer into rax.
        textSection += "mov rax, " + node->value + "\n";
    } else {
        // For any other node types, recursively visit children.
        for (auto &child : node->children) {
            visitNode(child);
        }
    }
}

void CodeGenerator::endAssembly() {
    // Append exit syscall.
    textSection += "\n";
    textSection += "mov rax, 60      ; syscall: exit\n";
    textSection += "xor rdi, rdi     ; exit code 0\n";
    textSection += "syscall\n\n";

    // Append the print routine that converts the integer in RAX to a string.
    textSection += "print_number:\n";
    textSection += "    push rbp\n";
    textSection += "    mov rbp, rsp\n";
    textSection += "    mov r12, rax          ; save original number in r12\n";

    // Add code to handle negative numbers
    textSection += "    cmp r12, 0\n";
    textSection += "    jge .positive\n";
    textSection += "    ; Handle negative number\n";
    textSection += "    push r12\n";
    textSection += "    mov rax, 1\n";
    textSection += "    mov rdi, 1\n";
    textSection += "    mov rsi, minus_sign\n";
    textSection += "    mov rdx, 1\n";
    textSection += "    syscall\n";
    textSection += "    pop r12\n";
    textSection += "    neg r12      ; Make the number positive\n";

    textSection += ".positive:\n";
    textSection += "    lea rdi, [buffer+31]  ; point rdi to end of buffer\n";
    textSection += "    mov byte [rdi], 0     ; null-terminate the buffer\n";
    textSection += "    cmp r12, 0\n";
    textSection += "    jne .convert\n";
    textSection += "    mov byte [rdi-1], '0'\n";
    textSection += "    lea rdi, [rdi-1]\n";
    textSection += "    jmp .print\n";
     
    textSection += ".convert:\n";
    textSection += "    mov rax, r12\n";
    textSection += "    mov rbx, 10\n";
    textSection += ".convert_loop:\n";
    textSection += "    xor rdx, rdx\n";
    textSection += "    div rbx             ; rax = quotient, rdx = remainder\n";
    textSection += "    add rdx, '0'        ; convert digit to ASCII\n";
    textSection += "    dec rdi             ; move pointer left\n";
    textSection += "    mov [rdi], dl       ; store digit in buffer\n";
    textSection += "    cmp rax, 0\n";
    textSection += "    jne .convert_loop\n";
    textSection += ".print:\n";
    textSection += "    lea rsi, [rdi]      ; rsi points to start of the string\n";
    textSection += "    mov rdx, buffer+31\n";
    textSection += "    sub rdx, rdi        ; compute string length\n";
    textSection += "    mov rax, 1          ; syscall: write\n";
    textSection += "    mov rdi, 1          ; file descriptor: stdout\n";
    textSection += "    syscall\n";
    textSection += "    pop rbp\n";
    textSection += "    ret\n";
}

void CodeGenerator::writeToFile(const std::string &filename) {
    std::ofstream outFile(filename);
    if (!outFile) {
        throw std::runtime_error("Failed to open output file: " + filename);
    }
    outFile << asmCode;
    outFile.close();
}

void CodeGenerator::genPrint() {
    // Assumes the computed expression's result is in RAX.
    textSection += "call print_number\n";
}

void CodeGenerator::genAffect(const std::shared_ptr<ASTNode>& node) {
    if (node->children.size() < 2) {
        throw std::runtime_error("Invalid ASTNode structure for assignment");
    }
    
    std::string varName = node->children[0]->value;  // e.g., "a" or "b"
    
    // If the variable is not yet declared, declare it in the data section
    if (declaredVars.find(varName) == declaredVars.end()) {
        dataSection += varName + ": dq 0\n";
        declaredVars.insert(varName);
    }
    
    // Evaluate the right-hand side expression
    visitNode(node->children[1]);
    
    // Generated code puts the result in RAX, now store it in the variable
    textSection += "mov qword [" + varName + "], rax\n";
}
void CodeGenerator::genFor(const std::shared_ptr<ASTNode>& node) {
    // Get the loop variable name
    std::string loopVar = node->children[0]->value;
    // If the variable is not yet declared, declare it
    if (declaredVars.find(loopVar) == declaredVars.end()) {
        dataSection += loopVar + ": dq 0\n";
        declaredVars.insert(loopVar);
    }
    
    // Generate unique labels for this loop
    static int loopCounter = 0;
    std::string loopId = std::to_string(loopCounter++);
    std::string startLabel = ".loop_start_" + loopId;
    std::string endLabel = ".loop_end_" + loopId;
    
    // Check if this is a range function call
    if (node->children[1]->type == "FunctionCall" && 
        node->children[1]->children[0]->value == "range") {
        
        // Get range parameter(s)
        auto paramList = node->children[1]->children[1];
        if (paramList->children.size() >= 1) {
            // Initialize loop variable to 0
            textSection += "; Initialize loop\n";
            textSection += "mov qword [" + loopVar + "], 0\n";
            
            // Get the end value of the range
            visitNode(paramList->children[0]);  // Visit the range end value (10 in your example)
            textSection += "push rax\n"; // Save the end value
            
            // Start of loop
            textSection += startLabel + ":\n";
            textSection += "; Check loop condition\n";
            textSection += "mov rax, qword [" + loopVar + "]\n";
            textSection += "pop rbx\n"; // Get the end value
            textSection += "push rbx\n"; // Save it again for next iteration
            textSection += "cmp rax, rbx\n";
            textSection += "jge " + endLabel + "\n";
            
            // Execute loop body
            textSection += "; Loop body\n";
            visitNode(node->children[2]); // Visit the ForBody
            
            // Increment loop variable
            textSection += "; Increment loop variable\n";
            textSection += "mov rax, qword [" + loopVar + "]\n";
            textSection += "inc rax\n";
            textSection += "mov qword [" + loopVar + "], rax\n";
            textSection += "jmp " + startLabel + "\n";
            
            // End of loop
            textSection += endLabel + ":\n";
            textSection += "pop rbx\n"; // Clean up the stack
        }
    } else {
        // Handle other types of for loops if necessary
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
    visitNode(node->children[0]);
    
    // Test if the condition is false (0)
    textSection += "cmp rax, 0\n";
    
    // If we have an else block (third child), jump to it if condition is false
    if (node->children.size() > 2 && node->children[2]) {
        textSection += "je " + elseLabel + "\n";
    } else {
        // Otherwise just skip the if block
        textSection += "je " + endLabel + "\n";
    }
    
    // Generate code for the "then" part (if body)
    textSection += "; If body\n";
    visitNode(node->children[1]);
    
    // Skip the else part if the "then" part was executed
    if (node->children.size() > 2 && node->children[2]) {
        textSection += "jmp " + endLabel + "\n";
        
        // Generate code for the "else" part
        textSection += elseLabel + ":\n";
        textSection += "; Else body\n";
        visitNode(node->children[2]);
    }
    
    // End of if statement
    textSection += endLabel + ":\n";
}
void CodeGenerator::genFunction(const std::shared_ptr<ASTNode>& node) {
    // Get function name
    std::string funcName = node->value;
    currentFunction = funcName;
    
    // Function label
    textSection += "\n" + funcName + ":\n";
    
    // Function prologue
    textSection += "    push rbp\n";
    textSection += "    mov rbp, rsp\n";
    
    // Handle parameters (first 6 in registers: rdi, rsi, rdx, rcx, r8, r9)
    auto paramList = node->children[0];
    for (size_t i = 0; i < paramList->children.size() && i < 6; i++) {
        std::string paramName = paramList->children[i]->value;
        if (declaredVars.find(paramName) == declaredVars.end()) {
            dataSection += paramName + ": dq 0\n";
            declaredVars.insert(paramName);
        }
        
        // Move parameter from register to memory
        std::string reg = (i == 0) ? "rdi" : (i == 1) ? "rsi" : (i == 2) ? "rdx" : 
                          (i == 3) ? "rcx" : (i == 4) ? "r8" : "r9";
        textSection += "    mov [" + paramName + "], " + reg + "\n";
    }
    
    // Generate function body
    visitNode(node->children[1]);
    
    // Function epilogue
    textSection += ".return_" + funcName + ":\n";
    textSection += "    mov rsp, rbp\n";
    textSection += "    pop rbp\n";
    textSection += "    ret\n";
}

void CodeGenerator::genFunctionCall(const std::shared_ptr<ASTNode>& node) {
    std::string funcName = node->children[0]->value;
    auto args = node->children[1];
    
    // Save caller-saved registers
    textSection += "    ; Save registers for function call\n";
    textSection += "    push rbx\n";
    textSection += "    push r12\n";
    textSection += "    push r13\n";
    
    // Align the stack to 16 bytes (critical for function calls)
    textSection += "    ; Align stack\n";
    textSection += "    mov rbx, rsp\n";
    textSection += "    and rsp, -16\n";
    textSection += "    push rbx\n";  // Save original stack pointer
    
    // Prepare arguments (first 6 in registers)
    std::string regs[] = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"};
    for (size_t i = 0; i < args->children.size() && i < 6; i++) {
        visitNode(args->children[i]);  // Result in RAX
        textSection += "    mov " + std::string(regs[i]) + ", rax\n";
    }
    
    // Call the function
    textSection += "    call " + funcName + "\n";
    
    // Restore stack alignment
    textSection += "    pop rsp\n";  // Restore original stack pointer
    
    // Restore saved registers in reverse order
    textSection += "    pop r13\n";
    textSection += "    pop r12\n";
    textSection += "    pop rbx\n";
    
    // Result is already in rax
}

void CodeGenerator::genReturn(const std::shared_ptr<ASTNode>& node) {
    // Evaluate return expression if any
    if (!node->children.empty()) {
        visitNode(node->children[0]);
    } else {
        textSection += "    xor rax, rax\n";  // Return 0 by default
    }
    
    // Jump to return label
    textSection += "    jmp .return_" + currentFunction + "\n";
}
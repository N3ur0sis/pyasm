#include "codeGenerator.h"
#include <fstream>
#include <stdexcept>
#include <cstdlib>
#include <sstream>

void CodeGenerator::generateCode(const std::shared_ptr<ASTNode>& root, const std::string& filename) {
    // Initialize our sections.
    dataSection = "";
    textSection = "";
    declaredVars.clear();
    
    // Generate the text (and data) from the AST.
    startAssembly();
    visitNode(root);
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
    
    // Text section.
    finalAsm << "section .text\n";
    finalAsm << "_start:\n";
    finalAsm << textSection;
    
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
    } else if (node->type == "For") {
            genFor(node);

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
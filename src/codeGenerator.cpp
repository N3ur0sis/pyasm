#include "codeGenerator.h"
#include <fstream>
#include <stdexcept>
#include <cstdlib>
#include <sstream>
#include <functional>
SymbolTable* currentSymbolTable = nullptr;

void CodeGenerator::generateCode(const std::shared_ptr<ASTNode>& root, const std::string& filename, SymbolTable* symTable) {
    symbolTable = symTable;
    currentSymbolTable = symbolTable;
    //Print for debugging
    

    std::cout << "Symbol Table: (generator)" << std::endl;
    symbolTable->print(std::cout, 0);
    // Initialize our sections.
    dataSection = "";
    textSection = "";
    std::string functionSection = ""; // Add a separate section for functions
    declaredVars.clear();
    dataSection = std::string("concat_buffer: times 2048 db 0\n") + "concat_offset: dq 0\n" + dataSection;
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
    finalAsm << "space: db 32\n";    
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
    } else if (node->type == "String") {
            // Generate a unique label for this string
            static int strCounter = 0;
            std::string strLabel = "str_" + std::to_string(strCounter++);
            
            // Add string to data section with null terminator
            dataSection += strLabel + ": db ";
            
            // Escape the string and add it to data section
            std::string strValue = node->value;
            dataSection += "\"" + strValue + "\", 0\n";
            
            // Load the address of the string into rax
            textSection += "mov rax, " + strLabel + "\n";
       
    } else if (node->type == "Print") {
        // Check if this is a print with multiple arguments (List)
        if (node->children.size() == 1 && node->children[0]->type == "List") {
            // Process each argument in the list
            for (size_t i = 0; i < node->children[0]->children.size(); i++) {
                const auto& item = node->children[0]->children[i];
                visitNode(item);  // Process one argument
                genPrint();       // Print that argument
                
                // Add a space between items (but not after the last one)
                if (i < node->children[0]->children.size() - 1) {
                    // Print a space between items
                    textSection += "mov rax, 1\n";         // write syscall
                    textSection += "mov rdi, 1\n";         // stdout
                    textSection += "push rax\n";           // save registers
                    textSection += "push rdi\n";
                    textSection += "mov rsi, space\n";     // point to space character
                    textSection += "mov rdx, 1\n";         // length 1
                    textSection += "syscall\n";
                    textSection += "pop rdi\n";            // restore registers
                    textSection += "pop rax\n";
                }
            }
        } else {
            // Regular print with single expression
            for (const auto& child : node->children) {
                visitNode(child);
            }
            genPrint();
        }
        
        // Always print newline at the end
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
            // Check if this is a string concatenation
            bool isStringOperation = false;
            if (node->children[0]->type == "String" && node->children[1]->type == "String") {
                isStringOperation = true;
            }
            else if (node->children[0]->type == "Identifier" && isStringVariable(node->children[0]->value)
                    && node->children[1]->type == "Identifier" && isStringVariable(node->children[1]->value)) {
            isStringOperation = true;
            }

            
            if (isStringOperation) {
                // Debug:
                textSection += "; String concatenation\n";
                
                // String concatenation
                visitNode(node->children[0]);
                textSection += "push rax\n";
                visitNode(node->children[1]);
                textSection += "mov rsi, rax\n";  // second string in RSI
                textSection += "pop rdi\n";       // first string in RDI
                // DEBUG: Print registers before concat
                textSection += "; rdi = first string, rsi = second string\n";
                textSection += "call str_concat\n";
            } else {
                // Integer addition
                visitNode(node->children[0]);
                textSection += "push rax\n";
                visitNode(node->children[1]);
                textSection += "pop rbx\n";
                textSection += "add rax, rbx\n";
            }
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
    // --- Program Exit ---
    textSection += "\n; Program exit\n";
    textSection += "mov rax, 60      ; syscall: exit\n";
    textSection += "xor rdi, rdi     ; exit code 0\n";
    textSection += "syscall\n\n";

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
    
    // --- str_concat avec gestion d’un buffer et offset ---
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
    // Create unique label names using a counter
    static int printCounter = 0;
    std::string printId = std::to_string(printCounter++);
    
    std::string strlenLoopLabel = ".print_strlen_loop_" + printId;
    std::string strlenDoneLabel = ".print_strlen_done_" + printId;
    std::string printNumLabel = ".print_num_" + printId;
    std::string printExitLabel = ".print_exit_" + printId;
    
    // Check if we're printing a string (address > 10000) or number
    textSection += "cmp rax, 10000\n";
    textSection += "jl " + printNumLabel + "\n";
    
    // Print string
    textSection += "mov rsi, rax\n";   // String address already in rax
    textSection += "mov rdx, 0\n";     // Calculate string length
    textSection += strlenLoopLabel + ":\n";
    textSection += "cmp byte [rsi+rdx], 0\n";
    textSection += "je " + strlenDoneLabel + "\n";
    textSection += "inc rdx\n";
    textSection += "jmp " + strlenLoopLabel + "\n";
    textSection += strlenDoneLabel + ":\n";
    textSection += "mov rax, 1\n";     // write syscall
    textSection += "mov rdi, 1\n";     // stdout
    textSection += "syscall\n";
    textSection += "jmp " + printExitLabel + "\n";
    
    // Print number
    textSection += printNumLabel + ":\n";
    textSection += "call print_number\n";
    
    textSection += printExitLabel + ":\n";
}
void CodeGenerator::genAffect(const std::shared_ptr<ASTNode>& node) {
    if (node->children.size() < 2) {
        throw std::runtime_error("Invalid ASTNode structure for assignment");
    }
    
    std::string varName = node->children[0]->value;  // e.g., "a" or "b"
    auto rightValue = node->children[1];
    
    // If the variable is not yet declared, declare it in the data section
    if (declaredVars.find(varName) == declaredVars.end()) {
        dataSection += varName + ": dq 0\n";
        declaredVars.insert(varName);
    }
    
    // Déterminer le type en fonction du nœud de droite
    std::string valueType;
    
    if (rightValue->type == "String") {
        valueType = "String";
    } 
    else if (rightValue->type == "Integer") {
        valueType = "int";
    }
    else if (rightValue->type == "Boolean") {
        valueType = "bool";
    }
    else if (rightValue->type == "ArithOp") {
        // On doit analyser l'opération pour déterminer le type résultant
        if (rightValue->value == "+" && 
            (rightValue->children[0]->type == "String" || rightValue->children[1]->type == "String" || 
             isStringVariable(rightValue->children[0]->value) || isStringVariable(rightValue->children[1]->value))) {
            valueType = "String";
        }
        else {
            valueType = "int";
        }
    }
    else if (rightValue->type == "Compare") {
        valueType = "bool";
    }
    else if (rightValue->type == "Identifier") {
        // Pour une variable, utiliser son type actuel
        if (isStringVariable(rightValue->value)) {
            valueType = "String";
        } else {
            valueType = "int"; // Par défaut
        }
    }
    else {
        // Type par défaut
        valueType = "int";
    }
    
    // Mettre à jour le type dans la table des symboles
    if (symbolTable) {
        updateSymbolType(varName, valueType);
    }
    
    // Evaluate the right-hand side expression
    visitNode(rightValue);
    
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
    std::string funcName = node->value;
    currentFunction = funcName;
    
    if (symbolTable) {
        for (const auto& child : symbolTable->children) {
            if (child->scopeName == "function " + funcName) {
                SymbolTable* previousTable = currentSymbolTable;
                currentSymbolTable = child.get();
                
                
                // Function header
                textSection += "\n" + funcName + ":\n";
                textSection += "    push rbp\n";
                textSection += "    mov rbp, rsp\n";
                
                // Handle parameters from stack
                auto paramList = node->children[0];
                for (size_t i = 0; i < paramList->children.size(); i++) {
                    std::string paramName = paramList->children[i]->value;
                    if (declaredVars.find(paramName) == declaredVars.end()) {
                        dataSection += paramName + ": dq 0\n";
                        declaredVars.insert(paramName);
                    }
                    
                    int offset = 16 + (i * 8);
                    textSection += "    mov rax, [rbp+" + std::to_string(offset) + "]\n";
                    textSection += "    mov [" + paramName + "], rax\n";
                }
                
                // Generate function body
                visitNode(node->children[1]);
                
                // Function epilogue
                textSection += ".return_" + funcName + ":\n";
                textSection += "    mov rsp, rbp\n";
                textSection += "    pop rbp\n";
                textSection += "    ret\n";
                
                currentSymbolTable = previousTable;
                return;
            }
        }
    }

}

void CodeGenerator::genFunctionCall(const std::shared_ptr<ASTNode>& node) {
    std::string funcName = node->children[0]->value;
    auto args = node->children[1];
    
    // Save caller-saved registers
    textSection += "; Save registers for function call\n";
    textSection += "push rbx\n";
    textSection += "push r12\n";
    textSection += "push r13\n";
    textSection += "push r14\n";
    textSection += "push r15\n";
    
    // Align the stack to 16 bytes before pushing arguments
    textSection += "; Align stack\n";
    textSection += "mov rbx, rsp\n";
    textSection += "and rsp, -16\n";
    textSection += "push rbx\n";  // Save original stack pointer
    
    // Push arguments in reverse order (right to left)
    for (int i = args->children.size() - 1; i >= 0; i--) {
        auto& arg = args->children[i];
        visitNode(arg);
        textSection += "push rax\n";
    }
    
     // Call the function
     textSection += "call " + funcName + "\n";
    
     // Cleanup: remove arguments from stack
     if (args->children.size() > 0) {
         textSection += "add rsp, " + std::to_string(args->children.size() * 8) + "\n";
     }
     
     // Restore stack alignment
     textSection += "pop rsp\n";  // Restore original stack pointer
     
     // Restore saved registers in reverse order
     textSection += "pop r15\n";
     textSection += "pop r14\n";
     textSection += "pop r13\n";
     textSection += "pop r12\n";
     textSection += "pop rbx\n";
     
     // Réinitialiser les types des variables de la fonction
     resetFunctionVarTypes(funcName);
     
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

bool CodeGenerator::isStringVariable(const std::string& name) {
    if (!symbolTable) return false;
    
    std::function<bool(SymbolTable*)> searchInTableAndParents = [&](SymbolTable* table) {
        while (table) {
            for (const auto& sym : table->symbols) {
                if (sym->name == name && sym->symCat == "variable") {
                    if (auto varSym = dynamic_cast<VariableSymbol*>(sym.get())) {
                        return varSym->type == "String";
                    }
                }
            }
            table = table->parent;
        }
        return false;
    };

    if (currentSymbolTable && searchInTableAndParents(currentSymbolTable)) {
        return true;
    }
    
    return searchInTableAndParents(symbolTable);
}
void CodeGenerator::updateSymbolType(const std::string& name, const std::string& type) {
    if (!symbolTable) return;
    
    // Fonction helper qui met à jour dans une table et ses parents
    std::function<bool(SymbolTable*)> updateInTableAndParents = [&](SymbolTable* table) {
        while (table) {
            // Chercher dans la table courante
            for (auto& sym : table->symbols) {
                if (sym->name == name && sym->symCat == "variable") {
                    if (auto varSym = dynamic_cast<VariableSymbol*>(sym.get())) {
                        varSym->type = type;
                        return true;
                    }
                }
            }
            // Passer à la table parente
            table = table->parent;
        }
        return false;
    };
    
    // Ordre de priorité:
    // 1. Mettre à jour d'abord dans la table courante et ses parents
    if (currentSymbolTable && updateInTableAndParents(currentSymbolTable)) {
        return;
    }
    
    // 2. Si pas trouvé, mettre à jour dans la table globale
    updateInTableAndParents(symbolTable);
}

void CodeGenerator::resetFunctionVarTypes(const std::string& funcName) {
    if (!symbolTable) return;
    
    // Trouver la table de symboles de la fonction
    for (const auto& child : symbolTable->children) {
        if (child->scopeName == "function " + funcName) {
            // Parcourir tous les symboles et réinitialiser les types
            for (auto& sym : child->symbols) {
                if (sym->symCat == "variable") {
                    if (auto varSym = dynamic_cast<VariableSymbol*>(sym.get())) {
                        varSym->type = "auto";
                    }
                }
            }
            break;
        }
    }
}
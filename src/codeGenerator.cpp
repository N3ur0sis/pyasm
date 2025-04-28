#include "codeGenerator.h"
#include <fstream>
#include <stdexcept>
#include <cstdlib>
#include <sstream>
#include <functional>
SymbolTable* currentSymbolTable = nullptr;

void CodeGenerator::generateCode(const std::shared_ptr<ASTNode>& root, const std::string& filename, SymbolTable* symTable) {
    //m_errorManager.addError(Error{"Expected Test", "", "Semantic", 0});
    symbolTable = symTable;
    currentSymbolTable = symbolTable;

    // Initialize our sections.
    dataSection = "";
    textSection = "";
    std::string functionSection = ""; // Add a separate section for functions
    declaredVars.clear();
    dataSection = std::string("concat_buffer: times 2048 db 0\n") + 
                "concat_offset: dq 0\n" + 
                "div_zero_msg: db 'Error: Division by zero', 10, 0\n" +
                "div_zero_len: equ $ - div_zero_msg\n" + 
                dataSection;

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
            static int strCounter = 0;
            std::string strLabel = "str_" + std::to_string(strCounter++);
            dataSection += strLabel + ": db ";
            
            std::string strValue = node->value;
            dataSection += "\"" + strValue + "\", 0\n";
            textSection += "mov rax, " + strLabel + "\n";
       
    } else if (node->type == "Print") {
        // Check if this is a print with multiple arguments (List)
        if (node->children.size() == 1 && node->children[0]->type == "List") {
            // Process each argument in the list
            for (size_t i = 0; i < node->children[0]->children.size(); i++) {
                const auto& item = node->children[0]->children[i];
                visitNode(item);  // Process one argument
                genPrint();       // Print that argument
                
                if (i < node->children[0]->children.size() - 1) {
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
        // Left
        visitNode(node->children[0]); 
        textSection += "push rax\n";  

        // Right
        visitNode(node->children[1]); 
        textSection += "mov rbx, rax\n"; 
        textSection += "pop rax\n";
        
        //Compare the two values
        textSection += "cmp rax, rbx\n";
        
        // Set rax to 1 (true) or 0 (false) based on the comparison
        if (node->value == "==") {
            textSection += "mov rax, 0\n";
            textSection += "sete al\n";  
        } else if (node->value == "!=") {
            textSection += "mov rax, 0\n";
            textSection += "setne al\n"; 
        } else if (node->value == "<") {
            textSection += "mov rax, 0\n";
            textSection += "setl al\n"; 
        } else if (node->value == ">") {
            textSection += "mov rax, 0\n";
            textSection += "setg al\n";  
        } else if (node->value == "<=") {
            textSection += "mov rax, 0\n";
            textSection += "setle al\n"; 
        } else if (node->value == ">=") {
            textSection += "mov rax, 0\n";
            textSection += "setge al\n"; 
        }
    } else if (node->type == "UnaryOp") {
        if (node->children[0]->type == "Integer" || (node->children[0]->type == "Identifier" && isIntVariable(node->children[0]->value.c_str()))) {
            visitNode(node->children[0]);
            textSection += "neg rax\n";
        }
        else{
            m_errorManager.addError(Error{"Expected Int for an Unary Operation ; ", "Got " + std::string(node->children[0]->type.c_str()), "Semantic", 0});
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
            }
            auto type1 = node->children[1]->type;
            if (node->children[1]->type == "Identifier") {
                type1 = getIdentifierType(node->children[1]->value);
            }
            if (type0 != type1) {
                m_errorManager.addError(Error{"Expected same type for an Arith Operation ; ", "Got " + std::string(type0.c_str()) + " and " + std::string(type1.c_str()), "Semantic", 0});
            }
            if (type0 != "Integer" && type0 != "String" && type0 != "List") {
                m_errorManager.addError(Error{"Expected Int or String for an Arith Operation ; ", "Got " + std::string(type0.c_str()), "Semantic", 0});
            }
            if (type1 != "Integer" && type1 != "String" && type1 != "List") {
                m_errorManager.addError(Error{"Expected Int or String or List for an Arith Operation ; ", "Got " + std::string(type0.c_str()), "Semantic", 0});
            }

            
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
            // Evaluate the left child
            if (node->children[0]->type == "Integer" || (node->children[0]->type == "Identifier" && isIntVariable(node->children[0]->value.c_str()))) {
                visitNode(node->children[0]);
                textSection += "push rax\n";
            }
            else{
                m_errorManager.addError(Error{"Expected Int for Sub Operation ; ", "Got " + std::string(node->children[0]->type.c_str()), "Semantic", 0});
            }
            // Evaluate the right child
            if (node->children[1]->type == "Integer" || (node->children[1]->type == "Identifier" && isIntVariable(node->children[1]->value.c_str()))) {
                visitNode(node->children[1]);
                textSection += "mov rbx, rax\n";
                textSection += "pop rax\n";
                textSection += "sub rax, rbx\n";
            }
            else{
                m_errorManager.addError(Error{"Expected Int for Sub Operation ; ", "Got " + std::string(node->children[1]->type.c_str()), "Semantic", 0});
            }
            
            
        }
    
    } else if (node->type == "TermOp"){
        if (node->value == "*") {
            // Evaluate the left child
            if (node->children[0]->type == "Integer" || (node->children[0]->type == "Identifier" && isIntVariable(node->children[0]->value.c_str()))) {
                visitNode(node->children[0]);
                textSection += "push rax\n";
            }
            else{
                m_errorManager.addError(Error{"Expected Int for Mul Operation ; ", "Got " + std::string(node->children[0]->type.c_str()), "Semantic", 0});
            }
            // Evaluate the right child
            if (node->children[1]->type == "Integer" || (node->children[1]->type == "Identifier" && isIntVariable(node->children[1]->value.c_str()))) {
                visitNode(node->children[1]);
                textSection += "mov rbx, rax\n";
                textSection += "pop rax\n";
                textSection += "imul rax, rbx\n";
            }
            else{
                m_errorManager.addError(Error{"Expected Int for Mul Operation ; ", "Got " + std::string(node->children[1]->type.c_str()), "Semantic", 0});
            }
        }
        else if (node->value == "//") {
            // Evaluate the left child
            if (node->children[0]->type == "Integer" || (node->children[0]->type == "Identifier" && isIntVariable(node->children[0]->value.c_str()))) {
                visitNode(node->children[0]);
                textSection += "push rax\n";
            }
            else{
                m_errorManager.addError(Error{"Expected Int for Integer Division Operation ; ", "Got " + std::string(node->children[0]->type.c_str()), "Semantic", 0});
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
            }
            else{
                m_errorManager.addError(Error{"Expected Int for Integer Division Operation ; ", "Got " + std::string(node->children[1]->type.c_str()), "Semantic", 0});
            }    
        }
        else if (node->value == "%") {
            // Evaluate the left child
            if (node->children[0]->type == "Integer" || (node->children[0]->type == "Identifier" && isIntVariable(node->children[0]->value.c_str()))) {
                visitNode(node->children[0]);
                textSection += "push rax\n";
            }
            else{
                m_errorManager.addError(Error{"Expected Int for Modulo Operation ; ", "Got " + std::string(node->children[0]->type.c_str()), "Semantic", 0});
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
                m_errorManager.addError(Error{"Expected Int for Modulo Operation ; ", "Got " + std::string(node->children[1]->type.c_str()), "Semantic", 0});
            }
        }
    } else if (node->type == "Identifier") {
        textSection += "mov rax, qword [" + node->value + "]\n";
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
    static int printCounter = 0;
    std::string printId = std::to_string(printCounter++);
    
    std::string strlenLoopLabel = ".print_strlen_loop_" + printId;
    std::string strlenDoneLabel = ".print_strlen_done_" + printId;
    std::string printNumLabel = ".print_num_" + printId;
    std::string printExitLabel = ".print_exit_" + printId;
    
    textSection += "cmp rax, 10000\n";
    textSection += "jl " + printNumLabel + "\n";
    
    // Print string
    textSection += "mov rsi, rax\n";  
    textSection += "mov rdx, 0\n";     
    textSection += strlenLoopLabel + ":\n";
    textSection += "cmp byte [rsi+rdx], 0\n";
    textSection += "je " + strlenDoneLabel + "\n";
    textSection += "inc rdx\n";
    textSection += "jmp " + strlenLoopLabel + "\n";
    textSection += strlenDoneLabel + ":\n";
    textSection += "mov rax, 1\n";     
    textSection += "mov rdi, 1\n";     
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
    
    std::string varName = node->children[0]->value; 
    auto rightValue = node->children[1];
    
    // If the variable is not yet declared, declare it in the data section
    if (declaredVars.find(varName) == declaredVars.end()) {
        dataSection += varName + ": dq 0\n";
        declaredVars.insert(varName);
    }
    
    // Check Type
    std::string valueType;
    
    if (rightValue->type == "String") {
        valueType = "String";
    } 
    else if (rightValue->type == "Integer") {
        valueType = "Integer";
    }
    else if (rightValue->type == "Boolean") {
        valueType = "bool";
    }
    else if (rightValue->type == "ArithOp") {
        if (rightValue->value == "+" && 
            (rightValue->children[0]->type == "String" || rightValue->children[1]->type == "String" || 
             isStringVariable(rightValue->children[0]->value) || isStringVariable(rightValue->children[1]->value))) {
            valueType = "String";
        }
        else {
            valueType = "Integer";
        }
    }
    else if (rightValue->type == "Compare") {
        valueType = "bool";
    }
    else if (rightValue->type == "Identifier") {
        if (isStringVariable(rightValue->value)) {
            valueType = "String";
        } else {
            valueType = "Integer";
        }
    }
    else {
        valueType = "Integer";
    }
    
    if (symbolTable) {
        updateSymbolType(varName, valueType);
    }
    
    visitNode(rightValue);
    
    textSection += "mov qword [" + varName + "], rax\n";
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
        
        // TODO : Check Possibilité de range(debut ? fin ? pas ?)
        auto paramList = node->children[1]->children[1];
        if (paramList->children.size() >= 1) {
            textSection += "; Initialize loop\n";
            textSection += "mov qword [" + loopVar + "], 0\n";
            
            // Get the end value of the range
            visitNode(paramList->children[0]);  
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
    visitNode(node->children[0]);
    
    // Test if the condition is false (0)
    textSection += "cmp rax, 0\n";
    
    if (node->children.size() > 2 && node->children[2]) {
        textSection += "je " + elseLabel + "\n";
    } else {
        textSection += "je " + endLabel + "\n";
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
    currentFunction = funcName;
    if (funcName == "range" || funcName == "len" || funcName == "print" || funcName == "range") {
        m_errorManager.addError(Error{"Function Name Error : " , funcName + " is a reserved function name", "Semantic", 0});
    }
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

    // Mise à jour des types de paramètres dans la table des symboles
    updateFunctionParamTypes(funcName, args);

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
        textSection += "    xor rax, rax\n";  
    }
    
    // Jump to return label
    textSection += "    jmp .return_" + currentFunction + "\n";
}


// Fonction pour obtenir le type d'un identifiant à partir de la table des symboles
std::string CodeGenerator::getIdentifierType(const std::string& name) {
    if (!symbolTable) return "auto";
    
    // Recherche dans la table courante et ses parents d'abord
    std::function<std::string(SymbolTable*)> searchInTableHierarchy = [&](SymbolTable* table) -> std::string {
        SymbolTable* current = table;
        while (current) {
            // Chercher dans les symboles de la table courante
            for (const auto& sym : current->symbols) {
                if (sym->name == name) {
                    if (sym->symCat == "variable" || sym->symCat == "parameter") {
                        if (auto varSym = dynamic_cast<VariableSymbol*>(sym.get())) {
                            return varSym->type;
                        }
                    } else if (sym->symCat == "function") {
                        if (auto funcSym = dynamic_cast<FunctionSymbol*>(sym.get())) {
                            return funcSym->returnType;
                        }
                    } else if (sym->symCat == "array") {
                        return "array";
                    }
                    // Si on trouve le nom mais pas le type
                    return "auto";
                }
            }
            // Si pas trouvé, chercher dans la table parente
            current = current->parent;
        }
        return "";  // Non trouvé dans cette hiérarchie
    };
    
    // Chercher d'abord dans la table courante (par exemple, fonction)
    if (currentSymbolTable) {
        std::string type = searchInTableHierarchy(currentSymbolTable);
        if (!type.empty()) {
            return type;
        }
    }
    
    // Si pas trouvé dans la table courante, chercher dans la table globale
    std::string type = searchInTableHierarchy(symbolTable);
    if (!type.empty()) {
        return type;
    }
    
    // Si le type n'est pas trouvé, retourner une valeur par défaut
    return "auto";
}

bool CodeGenerator::isStringVariable(const std::string& name) {
    return getIdentifierType(name) == "String";
}

bool CodeGenerator::isIntVariable(const std::string& name) {
    std::string type = getIdentifierType(name);
    return type == "Integer";
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

void CodeGenerator::updateFunctionParamTypes(const std::string& funcName, const std::shared_ptr<ASTNode>& args) {
    if (!symbolTable) return;

    // Rechercher la table des symboles de la fonction
    for (const auto& child : symbolTable->children) {
        if (child->scopeName == "function " + funcName) {
            
            textSection += "; Updating parameter types for function " + funcName + "\n";
            
            // Parcourir les paramètres et déterminer leurs types
            std::vector<std::string> paramTypes;
            for (size_t i = 0; i < args->children.size(); i++) {
                auto& arg = args->children[i];
                std::string paramType = "Integer"; // Par défaut
                
                // Déterminer le type en fonction du nœud
                if (arg->type == "String") {
                    paramType = "String";
                } 
                else if (arg->type == "Identifier" && isStringVariable(arg->value)) {
                    paramType = "String";
                }
                else if (arg->type == "ArithOp" && arg->value == "+") {
                    // Vérifier s'il s'agit d'une concaténation de chaînes
                    if ((arg->children[0]->type == "String") || 
                        (arg->children[1]->type == "String") ||
                        (arg->children[0]->type == "Identifier" && isStringVariable(arg->children[0]->value)) ||
                        (arg->children[1]->type == "Identifier" && isStringVariable(arg->children[1]->value))) {
                        paramType = "String";
                    }
                }
                
                paramTypes.push_back(paramType);
                textSection += "; Parameter " + std::to_string(i+1) + " determined as " + paramType + "\n";
            }
            
            // Mettre à jour les types dans la table des symboles
            size_t paramIdx = 0;
            for (auto& sym : child->symbols) {
                if ((sym->symCat == "parameter" || sym->symCat == "variable") && paramIdx < paramTypes.size()) {
                    if (auto varSym = dynamic_cast<VariableSymbol*>(sym.get())) {
                        varSym->type = paramTypes[paramIdx];
                        textSection += "; Set parameter " + sym->name + " to type " + paramTypes[paramIdx] + "\n";
                        paramIdx++;
                    }
                }
            }
            break;
        }
    }
}
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

    dataSection = "";
    textSection = "";
    std::string functionSection = "";
    declaredVars.clear();
    dataSection = std::string("concat_buffer: times 2048 db 0\n") + 
                "concat_offset: dq 0\n" + 
                "div_zero_msg: db 'Error: Division by zero', 10, 0\n" +
                "div_zero_len: equ $ - div_zero_msg\n" + 
                dataSection;
    dataSection += "list_buffer: times 8192 dq 0\n";
    dataSection += "list_offset: dq 0\n";  
    dataSection += "open_bracket: db '['\n";
    dataSection += "close_bracket: db ']'\n";
    dataSection += "comma_space: db ',', 32\n";

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
    } else if (node->type == "And") {
        visitNode(node->children[0]);
        textSection += "push rax\n"; 
        
        visitNode(node->children[1]);
        textSection += "pop rbx\n";   
        textSection += "and rax, rbx\n"; 
    } else if (node->type == "Not") {
        textSection += "not rax\n";  
    } else if (node->type == "Or") {
        visitNode(node->children[0]);
        textSection += "push rax\n"; 
        
        visitNode(node->children[1]);
        textSection += "pop rbx\n";   
        textSection += "or rax, rbx\n"; 
    } else if (node->type == "True") {
        textSection += "mov rax, 1\n";  
    } else if (node->type == "False") {
        textSection += "mov rax, 0\n";  
    } else if (node->type == "List") {
        genList(node);
    } else if (node->type == "String") {
            static int strCounter = 0;
            std::string strLabel = "str_" + std::to_string(strCounter++);
            dataSection += strLabel + ": db ";
            
            std::string strValue = node->value;
            dataSection += "\"" + strValue + "\", 0\n";
            textSection += "mov rax, " + strLabel + "\n";
       
    } else if (node->type == "Print") {
        if (node->children.size() == 1 && node->children[0]->type == "List") {
            for (size_t i = 0; i < node->children[0]->children.size(); i++) {
                const auto& item = node->children[0]->children[i];
                visitNode(item);  
                genPrint();      

                if (i < node->children[0]->children.size() - 1) {
                    textSection += "mov rax, 1\n";        
                    textSection += "mov rdi, 1\n";         
                    textSection += "push rax\n";          
                    textSection += "push rdi\n";
                    textSection += "mov rsi, space\n";     
                    textSection += "mov rdx, 1\n";        
                    textSection += "syscall\n";
                    textSection += "pop rdi\n";         
                    textSection += "pop rax\n";
                }
            }
        } else {
            for (const auto& child : node->children) {
                visitNode(child);
            }
            genPrint();
        }
        
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
            m_errorManager.addError(Error{
                "Expected Int for an Unary Operation ; ", 
                "Got " + std::string(node->children[0]->type.c_str()), 
                "Semantics", 
                std::stoi(node->line)
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
            
            // Détermination du type du premier opérande (type0)
            auto type0 = node->children[0]->type;
            if (type0 == "Identifier") {
                type0 = getIdentifierType(node->children[0]->value);
            } else if (type0 == "FunctionCall") {
                std::string funcName = node->children[0]->children[0]->value;
                type0 = getFunctionReturnType(funcName);
            }
            auto type1 = node->children[1]->type;
            if (node->children[1]->type == "Identifier") {
                type1 = getIdentifierType(node->children[1]->value);
            }
            if (type0 == "auto") {
                m_errorManager.addError(Error{
                    "Undefined Variable; ", 
                    "Used " + std::string(node->children[0]->value.c_str()) + " before assignment",
                    "Semantics", 
                    0
                });
                return;
            }
            if (type1 == "auto") {
                m_errorManager.addError(Error{
                    "Undefined Variable; ", 
                    "Used " + std::string(node->children[1]->value.c_str())+ " before assignment",
                    "Semantics", 
                    0
                });
                return;
            }
            if (type0 != type1) {
                m_errorManager.addError(Error{
                    "Expected same type for an Arith Operation ; ", 
                    "Got " + std::string(type0.c_str()) + " and " + std::string(type1.c_str()), 
                    "Semantics", 
                    std::stoi(node->line)
                });
            }
            if (type0 != "Integer" && type0 != "String" && type0 != "List" && type0 != "auto") {
                m_errorManager.addError(Error{
                    "Expected Int or String or List for an Arith Operation ; ", 
                    "Got " + std::string(type0.c_str()), 
                    "Semantics", 
                    std::stoi(node->line)
                });
            }
            if (type1 != "Integer" && type1 != "String" && type1 != "List" && type1 != "auto") {
                m_errorManager.addError(Error{
                    "Expected Int or String or List for an Arith Operation ; ", 
                    "Got " + std::string(type1.c_str()), 
                    "Semantics", 
                    std::stoi(node->line)
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

            // Si on arrive ici, c'est forcément des entiers
            textSection += intOpLabel + ":\n";
            textSection += "add rax, rbx\n";
            textSection += "jmp " + endOpLabel + "\n";


            
            textSection += endOpLabel + ":\n";
        }
        else if (node->value == "-") {
            // Evaluate the left child
            if (node->children[0]->type == "Integer" || (node->children[0]->type == "Identifier" && isIntVariable(node->children[0]->value.c_str()))) {
                visitNode(node->children[0]);
                textSection += "push rax\n";
            }
            else{
                m_errorManager.addError(Error{
                    "Expected Int for Sub Operation ; ", 
                    "Got " + std::string(node->children[0]->type.c_str()), 
                    "Semantics", 
                    std::stoi(node->line)
                });
            }
            // Evaluate the right child
            if (node->children[1]->type == "Integer" || (node->children[1]->type == "Identifier" && isIntVariable(node->children[1]->value.c_str()))) {
                visitNode(node->children[1]);
                textSection += "mov rbx, rax\n";
                textSection += "pop rax\n";
                textSection += "sub rax, rbx\n";
            }
            else{
                m_errorManager.addError(Error{
                    "Expected Int for Sub Operation ; ", 
                    "Got " + std::string(node->children[1]->type.c_str()), 
                    "Semantics", 
                    std::stoi(node->line)
                });
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
                m_errorManager.addError(Error{
                    "Expected Int for Mul Operation ; ", 
                    "Got " + std::string(node->children[0]->type.c_str()), 
                    "Semantics", 
                    std::stoi(node->line)
                });
            }
            // Evaluate the right child
            if (node->children[1]->type == "Integer" || (node->children[1]->type == "Identifier" && isIntVariable(node->children[1]->value.c_str()))) {
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
                    std::stoi(node->line)
                });
            }
        }
        else if (node->value == "//") {
            // Evaluate the left child
            if (node->children[0]->type == "Integer" || (node->children[0]->type == "Identifier" && isIntVariable(node->children[0]->value.c_str()))) {
                visitNode(node->children[0]);
                textSection += "push rax\n";
            }
            else{
                m_errorManager.addError(Error{
                    "Expected Int for Integer Division Operation ; ", 
                    "Got " + std::string(node->children[0]->type.c_str()), 
                    "Semantics", 
                    std::stoi(node->line)
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
            }
            else{
                m_errorManager.addError(Error{
                    "Expected Int for Integer Division Operation ; ", 
                    "Got " + std::string(node->children[1]->type.c_str()), 
                    "Semantics", 
                    std::stoi(node->line)
                });
            }    
        }
        else if (node->value == "%") {
            // Evaluate the left child
            if (node->children[0]->type == "Integer" || (node->children[0]->type == "Identifier" && isIntVariable(node->children[0]->value.c_str()))) {
                visitNode(node->children[0]);
                textSection += "push rax\n";
            }
            else{
                m_errorManager.addError(Error{
                    "Expected Int for Modulo Operation ; ", 
                    "Got " + std::string(node->children[0]->type.c_str()), 
                    "Semantics", 
                    std::stoi(node->line)
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
                    std::stoi(node->line)
                });
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
    

    // --- Ajouter la fonction list_concat ---
    textSection += "\n; Function to concatenate two lists\n";
    textSection += "list_concat:\n";
    textSection += "    push rbp\n";
    textSection += "    mov rbp, rsp\n";
    textSection += "    push r12\n";
    textSection += "    push r13\n";
    textSection += "    push r14\n";
    textSection += "    push rbx\n";
    
    // Sauvegarder les listes d'entrée
    textSection += "    mov r12, rdi        ; r12 = liste1\n";
    textSection += "    mov r13, rsi        ; r13 = liste2\n";
    
    // Obtenir un nouvel emplacement pour la liste résultat
    textSection += "    mov r14, [list_offset]\n";
    textSection += "    mov rax, list_buffer\n";
    textSection += "    add rax, r14        ; rax = adresse de la nouvelle liste\n";
    textSection += "    push rax            ; sauvegarder l'adresse de la nouvelle liste\n";
    
    // Copier la première liste
    textSection += "    mov rsi, r12        ; source = liste1\n";
    textSection += "    mov rbx, r14        ; destination offset = list_offset actuel\n";
    
    // Boucle de copie de la première liste
    textSection += ".list_copy1_loop:\n";
    textSection += "    mov rdx, [rsi]      ; charger élément de liste1\n";
    textSection += "    cmp rdx, 0          ; vérifier si fin de liste\n";
    textSection += "    je .list_copy1_done\n";
    textSection += "    mov [list_buffer + rbx], rdx  ; copier l'élément\n";
    textSection += "    add rsi, 8          ; avancer dans liste1\n";
    textSection += "    add rbx, 8          ; avancer dans nouvelle liste\n";
    textSection += "    jmp .list_copy1_loop\n";
    textSection += ".list_copy1_done:\n";
    
    // Copier la deuxième liste
    textSection += "    mov rsi, r13        ; source = liste2\n";
    
    // Boucle de copie de la deuxième liste
    textSection += ".list_copy2_loop:\n";
    textSection += "    mov rdx, [rsi]      ; charger élément de liste2\n";
    textSection += "    cmp rdx, 0          ; vérifier si fin de liste\n";
    textSection += "    je .list_copy2_done\n";
    textSection += "    mov [list_buffer + rbx], rdx  ; copier l'élément\n";
    textSection += "    add rsi, 8          ; avancer dans liste2\n";
    textSection += "    add rbx, 8          ; avancer dans nouvelle liste\n";
    textSection += "    jmp .list_copy2_loop\n";
    textSection += ".list_copy2_done:\n";
    
    // Ajouter marqueur de fin (0) à la nouvelle liste
    textSection += "    mov qword [list_buffer + rbx], 0  ; marquer la fin\n";
    textSection += "    add rbx, 8          ; inclure le marqueur dans la taille\n";
    
    // Mettre à jour list_offset
    textSection += "    mov [list_offset], rbx\n";
    
    // Retourner l'adresse de la nouvelle liste
    textSection += "    pop rax             ; récupérer l'adresse de la liste résultat\n";
    
    // Nettoyage
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
    std::string printListLabel = ".print_list_" + printId;
    std::string printExitLabel = ".print_exit_" + printId;
    
    // Vérifier d'abord si c'est une liste (en comparant avec l'adresse du list_buffer)
    textSection += "mov rcx, list_buffer\n";
    textSection += "cmp rax, rcx\n";
    textSection += "jge " + printListLabel + "\n";
    
    // Si c'est < 10000, c'est un nombre
    textSection += "cmp rax, 10000\n";
    textSection += "jl " + printNumLabel + "\n";
    
    // Sinon c'est une chaîne
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
    
    // Code pour imprimer une liste
    textSection += printListLabel + ":\n";
    textSection += "mov rbx, rax\n";        // rbx = adresse de la liste
    
    // Imprimer le crochet ouvrant '['
    textSection += "push rbx\n";
    textSection += "mov rax, 1\n";
    textSection += "mov rdi, 1\n";
    textSection += "mov rsi, open_bracket\n";
    textSection += "mov rdx, 1\n";
    textSection += "syscall\n";
    textSection += "pop rbx\n";
    
    textSection += "cmp qword [rbx], 0\n";
    textSection += "je .print_list_end_" + printId + "\n";

    
    textSection += ".print_list_loop_" + printId + ":\n";
    textSection += "mov rax, [rbx]\n";      // Charger l'élément courant
    textSection += "cmp rax, 0\n";          // Vérifier si c'est la fin (marqueur 0)
    textSection += "je .print_list_end_" + printId + "\n";  // Si c'est 0, finir
    
    // Sauvegarde du pointeur de liste
    textSection += "push rbx\n";
    
    // Note : on ne peut pas faire un appel récursif direct, donc on utilise les appels séparés
    textSection += "cmp rax, 10000\n";
    textSection += "jl .print_elem_num_" + printId + "\n";
    
    // Si c'est une chaîne
    textSection += "mov rsi, rax\n";
    textSection += "mov rdx, 0\n";
    textSection += ".print_elem_strlen_" + printId + ":\n";
    textSection += "cmp byte [rsi+rdx], 0\n";
    textSection += "je .print_elem_done_" + printId + "\n";
    textSection += "inc rdx\n";
    textSection += "jmp .print_elem_strlen_" + printId + "\n";
    textSection += ".print_elem_done_" + printId + ":\n";
    textSection += "mov rax, 1\n";
    textSection += "mov rdi, 1\n";
    textSection += "syscall\n";
    textSection += "jmp .continue_list_" + printId + "\n";
    
    // Si c'est un nombre
    textSection += ".print_elem_num_" + printId + ":\n";
    textSection += "call print_number\n";
    
    // Continuer avec le prochain élément
    textSection += ".continue_list_" + printId + ":\n";
    textSection += "pop rbx\n";               // Restaurer le pointeur de liste
    textSection += "add rbx, 8\n";            // Passer à l'élément suivant
    
    // Imprimer une virgule et un espace si ce n'est pas le dernier élément
    textSection += "cmp qword [rbx], 0\n";    // Vérifier si l'élément suivant est la fin
    textSection += "je .print_list_loop_" + printId + "\n";  // Si fin, ne pas imprimer de virgule
    
    textSection += "push rbx\n";
    textSection += "mov rax, 1\n";
    textSection += "mov rdi, 1\n";
    textSection += "mov rsi, comma_space\n";
    textSection += "mov rdx, 2\n";
    textSection += "syscall\n";
    textSection += "pop rbx\n";
    
    textSection += "jmp .print_list_loop_" + printId + "\n";
    
    // Fin de la liste - imprimer le crochet fermant ']'
    textSection += ".print_list_end_" + printId + ":\n";
    textSection += "mov rax, 1\n";
    textSection += "mov rdi, 1\n";
    textSection += "mov rsi, close_bracket\n";
    textSection += "mov rdx, 1\n";
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
    if (rightValue->type == "True" || rightValue->type == "False") {
        valueType = "Boolean";
    }
    else if (rightValue->type == "String") {
        valueType = "String";
    } 
    else if (rightValue->type == "Integer") {
        valueType = "Integer";
    }
    else if (rightValue->type == "Boolean") {
        valueType = "Boolean";
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
        valueType = "Boolean";
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
        
        auto paramList = node->children[1]->children[1];
        if (paramList->children.size() != 1){
            m_errorManager.addError(Error{
                    "Expected one parameter for range ; ", 
                    "Got " + std::to_string(paramList->children.size()), 
                    "Semantics", 
                    std::stoi(node->line)
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
                // Vérifier que les parametres sont distincts 2 à 2
                for (size_t i = 0; i < paramList->children.size(); ++i) {
                    for (size_t j = i + 1; j < paramList->children.size(); ++j) {
                        if (paramList->children[i]->value == paramList->children[j]->value) {
                            m_errorManager.addError(Error{
                                "Params Error : " , 
                                "Duplicate parameter " + paramList->children[i]->value + ". Expected distinct parameters name.", 
                                "Semantics", 
                                std::stoi(node->line)
                            });
                            return;
                        }
                    }
                }
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
void CodeGenerator::genList(const std::shared_ptr<ASTNode>& node) {
    int listSize = node->children.size();

    textSection += "mov rbx, [list_offset]\n";
    textSection += "mov rax, list_buffer\n";
    textSection += "add rax, rbx\n";  
    textSection += "push rax\n";     

    if ((listSize == 1) && node->children[0] == nullptr) {
        textSection += "mov rcx, [list_offset]\n";
        textSection += "mov qword [list_buffer + rcx], 0\n";  // marqueur de fin
        textSection += "add rcx, 8\n";
        textSection += "mov [list_offset], rcx\n";
    } 
    else {
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
                    0
                });
                return;
            }
            textSection += "mov rcx, [list_offset]\n";
            textSection += "mov [list_buffer + rcx], rax\n";
            textSection += "add rcx, 8\n";
            textSection += "mov [list_offset], rcx\n";
        }
        textSection += "mov rcx, [list_offset]\n";
        textSection += "mov qword [list_buffer + rcx], 0\n";  // marqueur de fin
        textSection += "add rcx, 8\n";
        textSection += "mov [list_offset], rcx\n";
    }   

    textSection += "pop rax\n"; 
}
void CodeGenerator::genFunctionCall(const std::shared_ptr<ASTNode>& node) {
    std::string funcName = node->children[0]->value;
    auto args = node->children[1];

    updateFunctionParamTypes(funcName, args);
    
    for (const auto& child : symbolTable->children) {
        if (child->scopeName == "function " + funcName) {
            currentSymbolTable = child.get();
            break;
        }
    }

    textSection += "; Save registers for function call\n";
    textSection += "push rbx\n";
    textSection += "push r12\n";
    textSection += "push r13\n";
    textSection += "push r14\n";
    textSection += "push r15\n";
    
    textSection += "; Align stack\n";
    textSection += "mov rbx, rsp\n";
    textSection += "and rsp, -16\n";
    textSection += "push rbx\n";
    
    for (int i = args->children.size() - 1; i >= 0; i--) {
        auto& arg = args->children[i];
        visitNode(arg);
        textSection += "push rax\n";
    }
    
    textSection += "call " + funcName + "\n";
    
    if (args->children.size() > 0) {
        textSection += "add rsp, " + std::to_string(args->children.size() * 8) + "\n";
    }
     
    textSection += "pop rsp\n"; 
     
    textSection += "pop r15\n";
    textSection += "pop r14\n";
    textSection += "pop r13\n";
    textSection += "pop r12\n";
    textSection += "pop rbx\n";
     

    resetFunctionVarTypes(funcName);
}
void CodeGenerator::genReturn(const std::shared_ptr<ASTNode>& node) {
    if (!node->children.empty()) {
        visitNode(node->children[0]);
    } else {
        textSection += "    xor rax, rax\n";  
    }
    
    textSection += "    jmp .return_" + currentFunction + "\n";
}


std::string CodeGenerator::getIdentifierType(const std::string& name) {
    if (!symbolTable) return "auto";
    
    std::function<std::string(SymbolTable*)> searchInTableHierarchy = [&](SymbolTable* table) -> std::string {
        SymbolTable* current = table;
        while (current) {
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
                    return "auto";
                }
            }
            current = current->parent;
        }
        return ""; 
    };
    
    if (currentSymbolTable) {
        std::string type = searchInTableHierarchy(currentSymbolTable);
        if (!type.empty()) {
            return type;
        }
    }
    
    std::string type = searchInTableHierarchy(symbolTable);
    if (!type.empty()) {
        return type;
    }
    
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
    
    std::function<bool(SymbolTable*)> updateInTableAndParents = [&](SymbolTable* table) {
        while (table) {
            for (auto& sym : table->symbols) {
                if (sym->name == name && sym->symCat == "variable") {
                    if (auto varSym = dynamic_cast<VariableSymbol*>(sym.get())) {
                        varSym->type = type;
                        return true;
                    }
                }
            }
            table = table->parent;
        }
        return false;
    };

    if (currentSymbolTable && updateInTableAndParents(currentSymbolTable)) {
        return;
    }
    
    updateInTableAndParents(symbolTable);
}

void CodeGenerator::resetFunctionVarTypes(const std::string& funcName) {
    if (!symbolTable) return;
    
    for (const auto& child : symbolTable->children) {
        if (child->scopeName == "function " + funcName) {
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

    for (const auto& child : symbolTable->children) {
        if (child->scopeName == "function " + funcName) {
            
            textSection += "; Updating parameter types for function " + funcName + "\n";
            
            std::vector<std::string> paramTypes;
            for (size_t i = 0; i < args->children.size(); i++) {
                auto& arg = args->children[i];
                std::string paramType = "Integer"; // Default
                
                if (arg->type == "String") {
                    paramType = "String";
                } 
                else if (arg->type == "Identifier" && isStringVariable(arg->value)) {
                    paramType = "String";
                }
                else if (arg->type == "ArithOp" && arg->value == "+") {
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

void CodeGenerator::updateFunctionReturnType(const std::string& funcName, const std::string& returnType) {
    if (!symbolTable) return;
    
    for (const auto& child : symbolTable->children) {
        if (child->scopeName == "function " + funcName) {
            for (auto& sym : symbolTable->symbols) {
                if (sym->name == funcName && sym->symCat == "function") {
                    if (auto funcSym = dynamic_cast<FunctionSymbol*>(sym.get())) {
                        funcSym->returnType = returnType;
                        return;
                    }
                }
            }
            break;
        }
    }
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
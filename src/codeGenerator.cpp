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
    rootNode = root;
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
    dataSection += "index_error_msg: db 'Error: Index out of bounds', 10, 0\n";
    dataSection += "index_error_len: equ $ - index_error_msg\n";
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
                auto type0 = item->type;
                if (type0 == "Identifier") {
                    type0 = getIdentifierType(item->value);
                } else if (type0 == "FunctionCall") {
                    type0 = inferFunctionReturnType(this->rootNode, item->value);
                    
                }
                genPrint(type0);      

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
            auto type0 = node->children[0]->type;
            if (type0 == "Identifier") {
                type0 = getIdentifierType(node->children[0]->value);
            } else if (type0 == "FunctionCall") {
                type0 = inferFunctionReturnType(this->rootNode, node->children[0]->value);
                
            }
            genPrint(type0);      
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
            
            auto type0 = node->children[0]->type;
            if (type0 == "Identifier") {
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
                    std::stoi(node->line)
                });
                 return;

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
                    std::stoi(node->line)
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
                    std::stoi(node->line)
                });
            }
            textSection += "mov rbx, rax\n";
                textSection += "pop rax\n";
                textSection += "sub rax, rbx\n";
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

    // Dans la fonction endAssembly(), remplacer la partie print_not_string
textSection += "print_not_string:\n";
textSection += "    push rbp\n";
textSection += "    mov rbp, rsp\n";
textSection += "    push r12\n";
textSection += "    push r13\n";
        
textSection += "    ; Check if list (>= list_buffer)\n";
textSection += "    mov rcx, list_buffer\n";
textSection += "    cmp rax, rcx\n";
textSection += "    jl .print_as_number\n";
        
textSection += "    ; Print as list\n";
textSection += "    mov rbx, rax\n";
textSection += "    mov r12, [rbx]      ; r12 = taille de la liste\n";
textSection += "    add rbx, 8          ; passer à l'élément suivant\n";
        
textSection += "    ; Print opening bracket\n";
textSection += "    push rbx\n";
textSection += "    push r12\n";
textSection += "    mov rax, 1\n";
textSection += "    mov rdi, 1\n";
textSection += "    mov rsi, open_bracket\n";
textSection += "    mov rdx, 1\n";
textSection += "    syscall\n";
textSection += "    pop r12\n";
textSection += "    pop rbx\n";
        
textSection += "    ; Check if empty list\n";
textSection += "    cmp r12, 0\n";
textSection += "    je .print_list_end\n";
        
textSection += "    ; Initialize element counter\n";
textSection += "    mov r13, 0          ; r13 = compteur d'éléments\n";
        
textSection += ".print_list_loop:\n";
textSection += "    ; Get current element\n";
textSection += "    mov rax, [rbx]\n";
        
textSection += "    ; Save list pointer and counter\n";
textSection += "    push rbx\n";
textSection += "    push r12\n";
textSection += "    push r13\n";
        
textSection += "    ; Print element\n";
textSection += "    cmp rax, 10000\n";
textSection += "    jge .print_element_as_string\n";
textSection += "    cmp rax, list_buffer\n";
textSection += "    jge .print_element_as_list\n";
textSection += "    call print_number\n";
textSection += "    jmp .print_element_done\n";
        
textSection += ".print_element_as_string:\n";
textSection += "    call print_string\n";
textSection += "    jmp .print_element_done\n";
        
textSection += ".print_element_as_list:\n";
textSection += "    call print_not_string\n";
        
textSection += ".print_element_done:\n";
textSection += "    ; Restore list pointer and counter\n";
textSection += "    pop r13\n";
textSection += "    pop r12\n";
textSection += "    pop rbx\n";
        
textSection += "    ; Move to next element\n";
textSection += "    add rbx, 8\n";
textSection += "    inc r13\n";
        
textSection += "    ; Check if we printed all elements\n";
textSection += "    cmp r13, r12\n";
textSection += "    jge .print_list_end\n";
        
textSection += "    ; Print comma and space\n";
textSection += "    push rbx\n";
textSection += "    push r12\n";
textSection += "    push r13\n";
textSection += "    mov rax, 1\n";
textSection += "    mov rdi, 1\n";
textSection += "    mov rsi, comma_space\n";
textSection += "    mov rdx, 2\n";
textSection += "    syscall\n";
textSection += "    pop r13\n";
textSection += "    pop r12\n";
textSection += "    pop rbx\n";
        
textSection += "    jmp .print_list_loop\n";
        
textSection += ".print_list_end:\n";
textSection += "    ; Print closing bracket\n";
textSection += "    mov rax, 1\n";
textSection += "    mov rdi, 1\n";
textSection += "    mov rsi, close_bracket\n";
textSection += "    mov rdx, 1\n";
textSection += "    syscall\n";
textSection += "    jmp .print_not_string_end\n";
        
textSection += ".print_as_number:\n";
textSection += "    call print_number\n";
        
textSection += ".print_not_string_end:\n";
textSection += "    pop r13\n";
textSection += "    pop r12\n";
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
    if (leftNode->type == "ListCall") {
        std::string listName = leftNode->children[0]->value;
        auto indexNode = leftNode->children[1];
        textSection += "; List element assignment\n";
        textSection += "mov rbx, qword [" + listName + "]\n";  

        // Vérifier si l'index est valide
        textSection += "; Calculate index\n";
        visitNode(indexNode);   

        textSection += "; Check if index is valid\n";
        textSection += "cmp rax, 0\n";
        static int errorCount = 0;
        std::string errorLabel = ".index_error_" + std::to_string(errorCount++);
        textSection += "jl " + errorLabel + "\n";
        textSection += "cmp rax, [rbx]\n";  // Comparer avec la taille stockée
        textSection += "jge " + errorLabel + "\n";
        
        textSection += "; Calculate element address\n";
        textSection += "add rbx, 8\n"; 
        textSection += "shl rax, 3\n"; 
        textSection += "add rbx, rax\n";
        
        textSection += "; Evaluate right value\n";
        visitNode(rightValue);  
        
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
    if (declaredVars.find(varName) == declaredVars.end()) {
        dataSection += varName + ": dq 0\n";
        declaredVars.insert(varName);
    }
    
    // Check Type
    std::string valueType;

    if (rightValue->type == "True" || rightValue->type == "False") {
        valueType = "Boolean";
    }
    else if (rightValue->type == "FunctionCall") {
        valueType = inferFunctionReturnType(this->rootNode, node->children[0]->value);   
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
    else if (rightValue->type == "List") {
        valueType = "List";
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

    textSection += "pop rax\n"; 
}
void CodeGenerator::genFunctionCall(const std::shared_ptr<ASTNode>& node) {
    std::string funcName = node->children[0]->value;
    auto args = node->children[1];
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
                m_errorManager.addError(Error{
                    "Undefined Variable; ", 
                    "Used " + std::string(param->value.c_str())+ " before assignment",
                    "Semantics", 
                    std::stoi(node->line)
                });
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
    }
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


std::string CodeGenerator::inferFunctionReturnType(const std::shared_ptr<ASTNode>& root, const std::string& funcName) {
    // Cette fonction analyserait l'AST de la fonction pour déterminer son type de retour
    // En se basant sur les instructions `return`
    for (const auto& child : root->children) {
        if (child->type == "Definitions") {
            for (const auto& def : child->children) {
                if (def->type == "FunctionDefinition" && def->value == funcName) {
                    // Trouver tous les noeuds Return dans le corps de la fonction
                    std::string returnType = "Integer"; // Type par défaut
                    
                    std::function<void(const std::shared_ptr<ASTNode>&)> findReturns;
                    findReturns = [&](const std::shared_ptr<ASTNode>& node) {
                        if (!node) return;
                        
                        if (node->type == "Return" && !node->children.empty()) {
                            auto returnExpr = node->children[0];
                            if (returnExpr->type == "String") {
                                returnType = "String";
                            } else if (returnExpr->type == "List") {
                                returnType = "List";
                            } else if (returnExpr->type == "FunctionCall") {
                                // Si c'est un appel récursif, on utilise le type actuel
                                if (returnExpr->children[0]->value == funcName) {
                                    // Type récursif, rien à faire
                                } else {
                                    returnType = getFunctionReturnType(returnExpr->children[0]->value);
                                }
                            } else if (returnExpr->type == "Identifier") {
                                 returnType = getIdentifierType(returnExpr->value);
                            }
                            // Pour les autres expressions, on conserve le type par défaut (Integer)
                        }
                        
                        for (const auto& child : node->children) {
                            findReturns(child);
                        }
                    };
                    
                    if (def->children.size() > 1) {
                        findReturns(def->children[1]); // Le corps est le 2ème enfant
                    }
                    
                    return returnType;
                }
            }
        }
    }
    
    return "Integer"; // Type par défaut
}
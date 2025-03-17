bits 64
default rel

section .data
msg_newline db 0xA, 0
msg_buf     db 128, 0  ; buffer for print

section .bss
; example: resb 4 for uninitialized data

section .text
global _start

align 16
_start:

; -- FunctionDefinition --
add:
    ; TODO: real function body
    ret
    ; [BOOTSTRAP] identifier x
    mov rax, 42
    ; [BOOTSTRAP] identifier y
    mov rax, 42
    ; [BOOTSTRAP] identifier x
    mov rax, 42
    ; [BOOTSTRAP] identifier y
    mov rax, 42

; -- Affect --
    mov rax, 5
    ; Stockage de RAX dans la variable a
    ; [BOOTSTRAP] skip memory store
    ; [BOOTSTRAP] identifier a
    mov rax, 42
    mov rax, 5

; -- Affect --
    mov rax, 10
    ; Stockage de RAX dans la variable b
    ; [BOOTSTRAP] skip memory store
    ; [BOOTSTRAP] identifier b
    mov rax, 42
    mov rax, 10

; -- Affect --

; -- FunctionCall --
    ; [BOOTSTRAP] call function add
    call add
    ; [BOOTSTRAP] identifier add
    mov rax, 42
    ; [BOOTSTRAP] identifier a
    mov rax, 42
    ; [BOOTSTRAP] identifier b
    mov rax, 42
    ; Stockage de RAX dans la variable c
    ; [BOOTSTRAP] skip memory store
    ; [BOOTSTRAP] identifier c
    mov rax, 42

; -- FunctionCall --
    ; [BOOTSTRAP] call function add
    call add
    ; [BOOTSTRAP] identifier add
    mov rax, 42
    ; [BOOTSTRAP] identifier a
    mov rax, 42
    ; [BOOTSTRAP] identifier b
    mov rax, 42

; -- Print --
    ; [BOOTSTRAP] identifier c
    mov rax, 42
    push rax

    ; Convert RAX to decimal ASCII in msg_buf
    pop rax
    mov rbx, 10                ; base = 10
    lea rcx, [rel msg_buf + 127]
    mov byte [rcx], 0          ; terminate string

convert_loop:
    xor rdx, rdx
    div rbx                     ; divide rax by 10 => rax=quotient, rdx=remainder
    add rdx, '0'               ; remainder -> ASCII
    dec rcx
    mov [rcx], dl
    cmp rax, 0
    jne convert_loop

    ; sys_write(stdout)
    mov rax, 1                 ; syscall: write
    mov rdi, 1                 ; fd = stdout
    mov rsi, rcx               ; address of string
    lea rdx, [rel msg_buf + 128]
    sub rdx, rcx               ; length = (msg_buf + 128) - rcx
    syscall

    ; print newline
    mov rax, 1
    mov rdi, 1
    lea rsi, [rel msg_newline] ; address of newline
    mov rdx, 2
    syscall

    ; [BOOTSTRAP] identifier c
    mov rax, 42

    ; exit(0)
    mov rax, 60                ; syscall: exit
    mov rdi, 20                 ; exit code 0
    syscall
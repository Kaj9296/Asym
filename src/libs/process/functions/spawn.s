[bits 64]

section .text

global spawn
spawn:
    mov rax, 1
    int 0x80
    ret
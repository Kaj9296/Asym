[bits 64]

%include "lib-system.inc"

section .text

global sys_spawn
sys_spawn:
    mov rax, SYS_SPAWN
    int SYSCALL_VECTOR
    ret
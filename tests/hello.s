    .file   "test.c"
    .globl  main

    .section .rodata
.SL0:
    .string "Hello "
.SL1:
    .string "world!\n"

    .text
main:
    push        %rbp
    mov         %rsp, %rbp
    leaq        .SL0(%rip), %rax
    movq        %rax, %rdi
    movb        $0, %al
    callq       printf@PLT
    callq       func
    movq        $0, %rax            # Exit code
    leaveq
    retq

func:
    push        %rbp
    mov         %rsp, %rbp
    leaq        .SL1(%rip), %rax
    movq        %rax, %rdi
    movb        $0, %al
    callq       printf@PLT
    leaveq
    retq

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "lexer.h"
#include "instr.h"
#include "parser.h"
#include "relocations.h"
#include "symbols.h"
#include "utils.h"

#define END -1

void _assert_instructions(Instructions* instr, va_list ap) {
    if (!instr) panic("Assert instruction on a NULL");

    int pos = 0;

    while (1) {
        int expected = va_arg(ap, int);

        if (expected == -1) {
            if (pos != instr->size) {
                dump_instructions(instr);
                panic("Unexpected instructions at position %d", pos);
            }

            return; // Success
        }

        if (pos == instr->size) {
            dump_instructions(instr);
            panic("Expected extra data at position %d: %#02x", pos, expected & 0xff);
        }

        if ((expected & 0xff) != instr->data[pos]) {
            dump_instructions(instr);
            panic("Mismatch at position %d: expected %#02x, got %#02x", pos, (uint8_t) expected & 0xff, instr->data[pos]);
        }

        pos++;
    }
}

void assert_instructions(Instructions* instr, ...) {
    va_list ap;
    va_start(ap, instr);
    _assert_instructions(instr, ap);
}

void test_assembly(char *input, ...) {
    va_list ap;
    va_start(ap, input);

    printf("%-60s", input);
    init_lexer_from_string(input);
    Instructions instr = parse_instruction_statement();
    _assert_instructions(&instr, ap);

    printf("pass\n");
}

int main() {
    init_opcodes();
    init_symbols();
    init_relocations();

    test_assembly("add      %al,                        %al",  0x00, 0xc0, END);
    test_assembly("add      %al,                        %cl",  0x00, 0xc1, END);
    test_assembly("add      %al,                        %dl",  0x00, 0xc2, END);
    test_assembly("add      %al,                        %bl",  0x00, 0xc3, END);
    test_assembly("add      %al,                        %ah",  0x00, 0xc4, END);
    test_assembly("add      %al,                        %ch",  0x00, 0xc5, END);
    test_assembly("add      %al,                        %dh",  0x00, 0xc6, END);
    test_assembly("add      %al,                        %bh",  0x00, 0xc7, END);
    test_assembly("add      %al,                        %r8b", 0x41, 0x00, 0xc0, END);
    test_assembly("add      %al,                        %r9b", 0x41, 0x00, 0xc1, END);
    test_assembly("add      %bl,                        %r8b", 0x41, 0x00, 0xd8, END);
    test_assembly("add      %r8b,                       %bl",  0x44, 0x00, 0xc3, END);
    test_assembly("add      %r8b,                       %r9b", 0x45, 0x00, 0xc1, END);
    test_assembly("add      %bx,                        %cx",  0x66, 0x01, 0xd9, END);
    test_assembly("add      %bx,                        %r8w", 0x66, 0x41, 0x01, 0xd8, END);
    test_assembly("add      %r8w,                       %bx",  0x66, 0x44, 0x01, 0xc3, END);
    test_assembly("add      %r8w,                       %r9w", 0x66, 0x45, 0x01, 0xc1, END);
    test_assembly("add      %ebx,                       %ecx", 0x01, 0xd9, END);
    test_assembly("add      %ebx,                       %r14d",0x41, 0x01, 0xde, END);
    test_assembly("add      %r14d,                      %ebx", 0x44, 0x01, 0xf3, END);
    test_assembly("add      %r15d,                      %r14d",0x45, 0x01, 0xfe, END);
    test_assembly("add      %rbx,                       %rcx", 0x48, 0x01, 0xd9, END);
    test_assembly("add      %rbx,                       %r14", 0x49, 0x01, 0xde, END);
    test_assembly("add      %r14,                       %rbx", 0x4c, 0x01, 0xf3, END);
    test_assembly("add      %r15,                       %r14", 0x4d, 0x01, 0xfe, END);
    test_assembly("add      $0x42,                      %al",  0x04, 0x42, END);
    test_assembly("add      $0x42,                      %bl",  0x80, 0xc3, 0x42, END);
    test_assembly("add      $0x42,                      %bx",  0x66, 0x83, 0xc3, 0x42, END);
    test_assembly("add      $0x42,                      %ebx", 0x83, 0xc3, 0x42, END);
    test_assembly("add      $0x42,                      %rbx", 0x48, 0x83, 0xc3, 0x42, END);
    test_assembly("add      $0x4243,                    %bx",  0x66, 0x81, 0xc3, 0x43, 0x42, END);
    test_assembly("add      $0x4243,                    %ebx", 0x81, 0xc3, 0x43, 0x42, 0x00, 0x00, END);
    test_assembly("add      $0x4243,                    %rbx", 0x48, 0x81, 0xc3, 0x43, 0x42, 0x00, 0x00, END);
    test_assembly("add      $0x42434445,                %ebx", 0x81, 0xc3, 0x45, 0x44, 0x43, 0x42, END);
    test_assembly("add      $0x42434445,                %rbx", 0x48, 0x81, 0xc3, 0x45, 0x44, 0x43, 0x42, END);

    // Check code generation with size in the mnemnonic
    test_assembly("addb     $42,                        %al",  0x04, 0x2a, END);
    test_assembly("addw     $42,                        %ax",  0x66, 0x5, 0x2a, 0x00, END);
    test_assembly("addl     $42,                        %eax", 0x83, 0xc0, 0x2a, END);
    test_assembly("addq     $42,                        %rax", 0x48, 0x83, 0xc0, 0x2a, END);

    test_assembly("not      %al",                0xf6, 0xd0, END);
    test_assembly("not      %ax",    0x66,       0xf7, 0xd0, END);
    test_assembly("not      %eax",               0xf7, 0xd0, END);
    test_assembly("not      %rax",   0x48,       0xf7, 0xd0, END);
    test_assembly("not      %r15b",  0x41,       0xf6, 0xd7, END);
    test_assembly("not      %r15d",  0x41,       0xf7, 0xd7, END);
    test_assembly("not      %r15w",  0x66, 0x41, 0xf7, 0xd7, END);
    test_assembly("not      %r15",   0x49,       0xf7, 0xd7, END);

    test_assembly("mov      %al,                        %r9b", 0x41, 0x88, 0xc1, END);
    test_assembly("mov      %bl,                        %r8b", 0x41, 0x88, 0xd8, END);
    test_assembly("mov      %r8b,                       %bl",  0x44, 0x88, 0xc3, END);
    test_assembly("mov      %r8b,                       %r9b", 0x45, 0x88, 0xc1, END);
    test_assembly("mov      %bx,                        %cx",  0x66, 0x89, 0xd9, END);
    test_assembly("mov      %bx,                        %r8w", 0x66, 0x41, 0x89, 0xd8, END);
    test_assembly("mov      %r8w,                       %bx",  0x66, 0x44, 0x89, 0xc3, END);
    test_assembly("mov      %r8w,                       %r9w", 0x66, 0x45, 0x89, 0xc1, END);
    test_assembly("mov      %ebx,                       %ecx", 0x89, 0xd9, END);
    test_assembly("mov      %ebx,                       %r14d",0x41, 0x89, 0xde, END);
    test_assembly("mov      %r14d,                      %ebx", 0x44, 0x89, 0xf3, END);
    test_assembly("mov      %r15d,                      %r14d",0x45, 0x89, 0xfe, END);
    test_assembly("mov      %rbx,                       %rcx", 0x48, 0x89, 0xd9, END);
    test_assembly("mov      %rbx,                       %r14", 0x49, 0x89, 0xde, END);
    test_assembly("mov      %r14,                       %rbx", 0x4c, 0x89, 0xf3, END);
    test_assembly("mov      %r15,                       %r14", 0x4d, 0x89, 0xfe, END);
    test_assembly("mov      $0xff,                      %al",  0xb0, 0xff, END);
    test_assembly("mov      $0x42,                      %al",  0xb0, 0x42, END);
    test_assembly("mov      $0x42,                      %ax",  0x66, 0xb8, 0x42, 0x00, END);
    test_assembly("mov      $0xff,                      %ax",  0x66, 0xb8, 0xff, 0x00, END);
    test_assembly("mov      $0x42,                      %eax",  0xb8, 0x42, 0x00, 0x00, 0x00, END);
    test_assembly("mov      $0x42,                      %rax",  0x48, 0xc7, 0xc0, 0x42, 0x00, 0x00, 0x00, END);
    test_assembly("mov      $0x42,                      %bl",  0xb3, 0x42, END);
    test_assembly("mov      $0x42,                      %r15b",0x41, 0xb7, 0x42, END);
    test_assembly("mov      $0x42,                      %bx",  0x66, 0xbb, 0x42, 0x00, END);
    test_assembly("mov      $0x42,                      %r15w",0x66, 0x41, 0xbf, 0x42, 0x00, END);
    test_assembly("mov      $0x42,                      %ebx", 0xbb, 0x42, 0x00, 0x00, 0x00, END);
    test_assembly("mov      $0x42,                      %r15d",0x41, 0xbf, 0x42, 0x00, 0x00, 0x00, END);
    test_assembly("mov      $0x42,                      %rbx", 0x48, 0xc7, 0xc3, 0x42, 0x00, 0x00, 0x00, END);
    test_assembly("mov      $0x42,                      %r15", 0x49, 0xc7, 0xc7, 0x42, 0x00, 0x00, 0x00, END);
    test_assembly("mov      $0x4243,                    %bx",  0x66, 0xbb, 0x43, 0x42, END);
    test_assembly("mov      $0x4243,                    %r15w",0x66, 0x41, 0xbf, 0x43, 0x42, END);
    test_assembly("mov      $0x4243,                    %ebx", 0xbb, 0x43, 0x42, 0x00, 0x00, END);
    test_assembly("mov      $0x4243,                    %r15d",0x41, 0xbf, 0x43, 0x42, 0x00, 0x00, END);
    test_assembly("mov      $0x4243,                    %rbx", 0x48, 0xc7, 0xc3, 0x43, 0x42, 0x00, 0x00, END);
    test_assembly("mov      $0x4243,                    %r15", 0x49, 0xc7, 0xc7, 0x43, 0x42, 0x00, 0x00, END);
    test_assembly("mov      $0x42434445,                %ebx", 0xbb, 0x45, 0x44, 0x43, 0x42, END);
    test_assembly("mov      $0x42434445,                %r15d",0x41, 0xbf, 0x45, 0x44, 0x43, 0x42, END);
    test_assembly("mov      $0x42434445,                %rbx", 0x48, 0xc7, 0xc3, 0x45, 0x44, 0x43, 0x42, END);
    test_assembly("mov      $0x42434445,                %r15", 0x49, 0xc7, 0xc7, 0x45, 0x44, 0x43, 0x42, END);
    test_assembly("mov      $0x82434445,                %rax", 0x48 ,0xb8 ,0x45 ,0x44 ,0x43 ,0x82 ,0x00, 0x00 ,0x00 ,0x00, END);
    test_assembly("mov      $0x4243444546474849,        %rbx", 0x48, 0xbb, 0x49, 0x48, 0x47, 0x46, 0x45, 0x44, 0x43, 0x42, END);
    test_assembly("mov      $0x4243444546474849,        %r15", 0x49, 0xbf, 0x49, 0x48, 0x47, 0x46, 0x45, 0x44, 0x43, 0x42, END);

    // Check avodance of instructions that would sign extend
    test_assembly("mov      $0x7f,                      %al",        0xb0,       0x7f, END);
    test_assembly("mov      $0x80,                      %al",        0xb0,       0x80, END);
    test_assembly("mov      $0x7f,                      %ax",  0x66, 0xb8,       0x7f, 0x00, END);
    test_assembly("mov      $0x80,                      %ax",  0x66, 0xb8,       0x80, 0x00, END);
    test_assembly("mov      $0x7f,                      %eax",       0xb8,       0x7f, 0x00, 0x00, 0x00, END);
    test_assembly("mov      $0x80,                      %eax",       0xb8,       0x80, 0x00, 0x00, 0x00, END);
    test_assembly("mov      $0x7f,                      %rax", 0x48, 0xc7,       0xc0, 0x7f, 0x00, 0x00, 0x00, END);
    test_assembly("mov      $0x80,                      %rax", 0x48, 0xc7,       0xc0, 0x80, 0x00, 0x00, 0x00, END);
    test_assembly("mov      $0x7fff,                    %ax",  0x66, 0xb8,       0xff, 0x7f, END);
    test_assembly("mov      $0x8000,                    %ax",  0x66, 0xb8,       0x00, 0x80, END);
    test_assembly("mov      $0x7fff,                    %eax",       0xb8,       0xff, 0x7f, 0x00, 0x00, END);
    test_assembly("mov      $0x8000,                    %eax",       0xb8,       0x00, 0x80, 0x00, 0x00, END);
    test_assembly("mov      $0x7fff,                    %rax", 0x48, 0xc7, 0xc0, 0xff, 0x7f, 0x00, 0x00, END);
    test_assembly("mov      $0x8000,                    %rax", 0x48, 0xc7, 0xc0, 0x00, 0x80, 0x00, 0x00, END);
    test_assembly("mov      $0x7fffffff,                %eax",       0xb8,       0xff, 0xff, 0xff, 0x7f, END);
    test_assembly("mov      $0x80000000,                %eax",       0xb8,       0x00, 0x00, 0x00, 0x80, END);
    test_assembly("mov      $0x7fffffff,                %rax", 0x48, 0xc7,       0xc0, 0xff, 0xff, 0xff, 0x7f, END);
    test_assembly("mov      $0x80000000,                %rax", 0x48, 0xb8, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, END);

    test_assembly("mov      %r15,                       (%rax)", 0x4c, 0x89, 0x38, END);
    test_assembly("mov      %r15,                       (%rcx)", 0x4c, 0x89, 0x39, END);
    test_assembly("mov      %r15,                       (%rdx)", 0x4c, 0x89, 0x3a, END);
    test_assembly("mov      %r15,                       (%rbx)", 0x4c, 0x89, 0x3b, END);
    test_assembly("mov      %r15,                       (%rsp)", 0x4c, 0x89, 0x3c, 0x24, END);      // SIB
    test_assembly("mov      %r15,                       (%rbp)", 0x4c, 0x89, 0x7d, 0x00, END);      // disp8
    test_assembly("mov      %r15,                       (%rsi)", 0x4c, 0x89, 0x3e, END);
    test_assembly("mov      %r15,                       (%rdi)", 0x4c, 0x89, 0x3f, END);
    test_assembly("mov      %r15,                       (%r8)",  0x4d, 0x89, 0x38, END);
    test_assembly("mov      %r15,                       (%r9)",  0x4d, 0x89, 0x39, END);
    test_assembly("mov      %r15,                       (%r10)", 0x4d, 0x89, 0x3a, END);
    test_assembly("mov      %r15,                       (%r11)", 0x4d, 0x89, 0x3b, END);
    test_assembly("mov      %r15,                       (%r12)", 0x4d, 0x89, 0x3c, 0x24, END);       // SIB
    test_assembly("mov      %r15,                       (%r13)", 0x4d, 0x89, 0x7d, 0x00, END);       // disp8
    test_assembly("mov      %r15,                       (%r14)", 0x4d, 0x89, 0x3e, END);
    test_assembly("mov      %r15,                       (%r15)", 0x4d, 0x89, 0x3f, END);
    test_assembly("mov      (%r14),                     %r15",   0x4d, 0x8b, 0x3e, END);

    test_assembly("movb     %bl,                        (%rax)",       0x88, 0x18, END);
    test_assembly("movw     %bx,                        (%rax)", 0x66, 0x89, 0x18, END);
    test_assembly("movl     %ebx,                       (%rax)",       0x89, 0x18, END);
    test_assembly("movq     %rbx,                       (%rax)", 0x48, 0x89, 0x18, END);

    test_assembly("mov      %r15,                       0x42(%rax)", 0x4c, 0x89, 0x78,       0x42, END);
    test_assembly("mov      %r15,                       0x42(%rcx)", 0x4c, 0x89, 0x79,       0x42, END);
    test_assembly("mov      %r15,                       0x42(%rdx)", 0x4c, 0x89, 0x7a,       0x42, END);
    test_assembly("mov      %r15,                       0x42(%rbx)", 0x4c, 0x89, 0x7b,       0x42, END);
    test_assembly("mov      %r15,                       0x42(%rsp)", 0x4c, 0x89, 0x7c, 0x24, 0x42, END); // SIB
    test_assembly("mov      %r15,                       0x42(%rbp)", 0x4c, 0x89, 0x7d,       0x42, END);
    test_assembly("mov      %r15,                       0x42(%rsi)", 0x4c, 0x89, 0x7e,       0x42, END);
    test_assembly("mov      %r15,                       0x42(%rdi)", 0x4c, 0x89, 0x7f,       0x42, END);
    test_assembly("mov      %r15,                       0x42(%r8)",  0x4d, 0x89, 0x78,       0x42, END);
    test_assembly("mov      %r15,                       0x42(%r9)",  0x4d, 0x89, 0x79,       0x42, END);
    test_assembly("mov      %r15,                       0x42(%r10)", 0x4d, 0x89, 0x7a,       0x42, END);
    test_assembly("mov      %r15,                       0x42(%r11)", 0x4d, 0x89, 0x7b,       0x42, END);
    test_assembly("mov      %r15,                       0x42(%r12)", 0x4d, 0x89, 0x7c, 0x24, 0x42, END); // SIB
    test_assembly("mov      %r15,                       0x42(%r13)", 0x4d, 0x89, 0x7d,       0x42, END);
    test_assembly("mov      %r15,                       0x42(%r14)", 0x4d, 0x89, 0x7e,       0x42, END);
    test_assembly("mov      %r15,                       0x42(%r15)", 0x4d, 0x89, 0x7f,       0x42, END);

    test_assembly("mov      %r15,                       0x42434546(%rax)",  0x4c, 0x89, 0xb8,       0x46, 0x45, 0x43, 0x42, END);
    test_assembly("mov      %r15,                       0x42434546(%rcx)",  0x4c, 0x89, 0xb9,       0x46, 0x45, 0x43, 0x42, END);
    test_assembly("mov      %r15,                       0x42434546(%rdx)",  0x4c, 0x89, 0xba,       0x46, 0x45, 0x43, 0x42, END);
    test_assembly("mov      %r15,                       0x42434546(%rbx)",  0x4c, 0x89, 0xbb,       0x46, 0x45, 0x43, 0x42, END);
    test_assembly("mov      %r15,                       0x42434546(%rsp)",  0x4c, 0x89, 0xbc, 0x24, 0x46, 0x45, 0x43, 0x42, END);
    test_assembly("mov      %r15,                       0x42434546(%rbp)",  0x4c, 0x89, 0xbd,       0x46, 0x45, 0x43, 0x42, END);
    test_assembly("mov      %r15,                       0x42434546(%rsi)",  0x4c, 0x89, 0xbe,       0x46, 0x45, 0x43, 0x42, END);
    test_assembly("mov      %r15,                       0x42434546(%rdi)",  0x4c, 0x89, 0xbf,       0x46, 0x45, 0x43, 0x42, END);
    test_assembly("mov      %r15,                       0x42434546(%r8)",   0x4d, 0x89, 0xb8,       0x46, 0x45, 0x43, 0x42, END);
    test_assembly("mov      %r15,                       0x42434546(%r9)",   0x4d, 0x89, 0xb9,       0x46, 0x45, 0x43, 0x42, END);
    test_assembly("mov      %r15,                       0x42434546(%r10)",  0x4d, 0x89, 0xba,       0x46, 0x45, 0x43, 0x42, END);
    test_assembly("mov      %r15,                       0x42434546(%r11)",  0x4d, 0x89, 0xbb,       0x46, 0x45, 0x43, 0x42, END);
    test_assembly("mov      %r15,                       0x42434546(%r12)",  0x4d, 0x89, 0xbc, 0x24, 0x46, 0x45, 0x43, 0x42, END);
    test_assembly("mov      %r15,                       0x42434546(%r13)",  0x4d, 0x89, 0xbd,       0x46, 0x45, 0x43, 0x42, END);
    test_assembly("mov      %r15,                       0x42434546(%r14)",  0x4d, 0x89, 0xbe,       0x46, 0x45, 0x43, 0x42, END);
    test_assembly("mov      %r15,                       0x42434546(%r15)",  0x4d, 0x89, 0xbf,       0x46, 0x45, 0x43, 0x42, END);

    test_assembly("mov      (%rax,%rbx,1),              %rcx", 0x48, 0x8b, 0x0c, 0x18, END);
    test_assembly("mov      (%r15,%rbx,1),              %rcx", 0x49, 0x8b, 0x0c, 0x1f, END);
    test_assembly("mov      (%rax,%r15,1),              %rcx", 0x4a, 0x8b, 0x0c, 0x38, END);
    test_assembly("mov      (%rax,%rbx,1),              %r15", 0x4c, 0x8b, 0x3c, 0x18, END);
    test_assembly("mov      (%rax,%rbx,2),              %rcx", 0x48, 0x8b, 0x0c, 0x58, END);
    test_assembly("mov      (%r15,%rbx,2),              %rcx", 0x49, 0x8b, 0x0c, 0x5f, END);
    test_assembly("mov      (%rax,%r15,2),              %rcx", 0x4a, 0x8b, 0x0c, 0x78, END);
    test_assembly("mov      (%rax,%rbx,2),              %r15", 0x4c, 0x8b, 0x3c, 0x58, END);
    test_assembly("mov      (%rax,%rbx,4),              %rcx", 0x48, 0x8b, 0x0c, 0x98, END);
    test_assembly("mov      (%r15,%rbx,4),              %rcx", 0x49, 0x8b, 0x0c, 0x9f, END);
    test_assembly("mov      (%rax,%r15,4),              %rcx", 0x4a, 0x8b, 0x0c, 0xb8, END);
    test_assembly("mov      (%rax,%rbx,4),              %r15", 0x4c, 0x8b, 0x3c, 0x98, END);
    test_assembly("mov      (%rax,%rbx,8),              %rcx", 0x48, 0x8b, 0x0c, 0xd8, END);
    test_assembly("mov      (%r15,%rbx,8),              %rcx", 0x49, 0x8b, 0x0c, 0xdf, END);
    test_assembly("mov      (%rax,%r15,8),              %rcx", 0x4a, 0x8b, 0x0c, 0xf8, END);
    test_assembly("mov      (%rax,%rbx,8),              %r15", 0x4c, 0x8b, 0x3c, 0xd8, END);
    test_assembly("mov      (%r11,%r14,8),              %r14", 0x4f, 0x8b, 0x34, 0xf3, END);

    test_assembly("mov      (%rax,%rbx,1),              %rcx", 0x48, 0x8b, 0x0c, 0x18, END);
    test_assembly("mov      (%rcx,%rbx,1),              %rcx", 0x48, 0x8b, 0x0c, 0x19, END);
    test_assembly("mov      (%rdx,%rbx,1),              %rcx", 0x48, 0x8b, 0x0c, 0x1a, END);
    test_assembly("mov      (%rbx,%rbx,1),              %rcx", 0x48, 0x8b, 0x0c, 0x1b, END);
    test_assembly("mov      (%rsp,%rbx,1),              %rcx", 0x48, 0x8b, 0x0c, 0x1c, END);
    test_assembly("mov      (%rbp,%rbx,1),              %rcx", 0x48, 0x8b, 0x4c, 0x1d, 0x00, END);  // With displacement
    test_assembly("mov      (%rsi,%rbx,1),              %rcx", 0x48, 0x8b, 0x0c, 0x1e, END);
    test_assembly("mov      (%rdi,%rbx,1),              %rcx", 0x48, 0x8b, 0x0c, 0x1f, END);
    test_assembly("mov      (%r8,%rbx,1),               %rcx", 0x49, 0x8b, 0x0c, 0x18, END);
    test_assembly("mov      (%r9,%rbx,1),               %rcx", 0x49, 0x8b, 0x0c, 0x19, END);
    test_assembly("mov      (%r10,%rbx,1),              %rcx", 0x49, 0x8b, 0x0c, 0x1a, END);
    test_assembly("mov      (%r11,%rbx,1),              %rcx", 0x49, 0x8b, 0x0c, 0x1b, END);
    test_assembly("mov      (%r12,%rbx,1),              %rcx", 0x49, 0x8b, 0x0c, 0x1c, END);
    test_assembly("mov      (%r13,%rbx,1),              %rcx", 0x49, 0x8b, 0x4c, 0x1d, 0x00, END);  // With displacement
    test_assembly("mov      (%r14,%rbx,1),              %rcx", 0x49, 0x8b, 0x0c, 0x1e, END);
    test_assembly("mov      (%r15,%rbx,1),              %rcx", 0x49, 0x8b, 0x0c, 0x1f, END);

    test_assembly("mov      0x42(%rax,%rbx,1),          %rcx", 0x48, 0x8b, 0x4c, 0x18, 0x42, END);
    test_assembly("mov      0x42(%r15,%rbx,1),          %rcx", 0x49, 0x8b, 0x4c, 0x1f, 0x42, END);
    test_assembly("mov      0x42(%rax,%r15,1),          %rcx", 0x4a, 0x8b, 0x4c, 0x38, 0x42, END);
    test_assembly("mov      0x42(%rax,%rbx,1),          %r15", 0x4c, 0x8b, 0x7c, 0x18, 0x42, END);
    test_assembly("mov      0x42(%rax,%rbx,2),          %rcx", 0x48, 0x8b, 0x4c, 0x58, 0x42, END);
    test_assembly("mov      0x42(%r15,%rbx,2),          %rcx", 0x49, 0x8b, 0x4c, 0x5f, 0x42, END);
    test_assembly("mov      0x42(%rax,%r15,2),          %rcx", 0x4a, 0x8b, 0x4c, 0x78, 0x42, END);
    test_assembly("mov      0x42(%rax,%rbx,2),          %r15", 0x4c, 0x8b, 0x7c, 0x58, 0x42, END);
    test_assembly("mov      0x42(%rax,%rbx,4),          %rcx", 0x48, 0x8b, 0x4c, 0x98, 0x42, END);
    test_assembly("mov      0x42(%r15,%rbx,4),          %rcx", 0x49, 0x8b, 0x4c, 0x9f, 0x42, END);
    test_assembly("mov      0x42(%rax,%r15,4),          %rcx", 0x4a, 0x8b, 0x4c, 0xb8, 0x42, END);
    test_assembly("mov      0x42(%rax,%rbx,4),          %r15", 0x4c, 0x8b, 0x7c, 0x98, 0x42, END);
    test_assembly("mov      0x42(%rax,%rbx,8),          %rcx", 0x48, 0x8b, 0x4c, 0xd8, 0x42, END);
    test_assembly("mov      0x42(%r15,%rbx,8),          %rcx", 0x49, 0x8b, 0x4c, 0xdf, 0x42, END);
    test_assembly("mov      0x42(%rax,%r15,8),          %rcx", 0x4a, 0x8b, 0x4c, 0xf8, 0x42, END);
    test_assembly("mov      0x42(%rax,%rbx,8),          %r15", 0x4c, 0x8b, 0x7c, 0xd8, 0x42, END);
    test_assembly("mov      0x42(%r11,%r14,8),          %r14", 0x4f, 0x8b, 0x74, 0xf3, 0x42, END);

    test_assembly("mov      0x42434445(%rax,%rbx,1),    %rcx", 0x48, 0x8b, 0x8c, 0x18, 0x45, 0x44, 0x43, 0x42, END);
    test_assembly("mov      0x42434445(%r15,%rbx,1),    %rcx", 0x49, 0x8b, 0x8c, 0x1f, 0x45, 0x44, 0x43, 0x42, END);
    test_assembly("mov      0x42434445(%rax,%r15,1),    %rcx", 0x4a, 0x8b, 0x8c, 0x38, 0x45, 0x44, 0x43, 0x42, END);
    test_assembly("mov      0x42434445(%rax,%rbx,1),    %r15", 0x4c, 0x8b, 0xbc, 0x18, 0x45, 0x44, 0x43, 0x42, END);
    test_assembly("mov      0x42434445(%rax,%rbx,2),    %rcx", 0x48, 0x8b, 0x8c, 0x58, 0x45, 0x44, 0x43, 0x42, END);
    test_assembly("mov      0x42434445(%r15,%rbx,2),    %rcx", 0x49, 0x8b, 0x8c, 0x5f, 0x45, 0x44, 0x43, 0x42, END);
    test_assembly("mov      0x42434445(%rax,%r15,2),    %rcx", 0x4a, 0x8b, 0x8c, 0x78, 0x45, 0x44, 0x43, 0x42, END);
    test_assembly("mov      0x42434445(%rax,%rbx,2),    %r15", 0x4c, 0x8b, 0xbc, 0x58, 0x45, 0x44, 0x43, 0x42, END);
    test_assembly("mov      0x42434445(%rax,%rbx,4),    %rcx", 0x48, 0x8b, 0x8c, 0x98, 0x45, 0x44, 0x43, 0x42, END);
    test_assembly("mov      0x42434445(%r15,%rbx,4),    %rcx", 0x49, 0x8b, 0x8c, 0x9f, 0x45, 0x44, 0x43, 0x42, END);
    test_assembly("mov      0x42434445(%rax,%r15,4),    %rcx", 0x4a, 0x8b, 0x8c, 0xb8, 0x45, 0x44, 0x43, 0x42, END);
    test_assembly("mov      0x42434445(%rax,%rbx,4),    %r15", 0x4c, 0x8b, 0xbc, 0x98, 0x45, 0x44, 0x43, 0x42, END);
    test_assembly("mov      0x42434445(%rax,%rbx,8),    %rcx", 0x48, 0x8b, 0x8c, 0xd8, 0x45, 0x44, 0x43, 0x42, END);
    test_assembly("mov      0x42434445(%r15,%rbx,8),    %rcx", 0x49, 0x8b, 0x8c, 0xdf, 0x45, 0x44, 0x43, 0x42, END);
    test_assembly("mov      0x42434445(%rax,%r15,8),    %rcx", 0x4a, 0x8b, 0x8c, 0xf8, 0x45, 0x44, 0x43, 0x42, END);
    test_assembly("mov      0x42434445(%rax,%rbx,8),    %r15", 0x4c, 0x8b, 0xbc, 0xd8, 0x45, 0x44, 0x43, 0x42, END);
    test_assembly("mov      0x42434445(%r11,%r14,8),    %r14", 0x4f, 0x8b, 0xb4, 0xf3, 0x45, 0x44, 0x43, 0x42, END);

    test_assembly("mov      %rbx,                       0x0(%rip)",   0x48, 0x89, 0x1d, 0x00, 0x00, 0x00, 0x00, END);
    test_assembly("mov      %rbx,                       0x100(%rip)", 0x48, 0x89, 0x1d, 0x00, 0x01, 0x00, 0x00, END);
    test_assembly("mov      %r15,                       0x0(%rip)",   0x4c, 0x89, 0x3d, 0x00, 0x00, 0x00, 0x00, END);
    test_assembly("mov      %r15,                       0x100(%rip)", 0x4c, 0x89, 0x3d, 0x00, 0x01, 0x00, 0x00, END);

    test_assembly("leaq     0(%rip),                    %r15", 0x4c, 0x8d, 0x3d, 0x00, 0x00, 0x00, 0x00, END);
    test_assembly("leaq     5(%rip),                    %r15", 0x4c, 0x8d, 0x3d, 0x05, 0x00, 0x00, 0x00, END);
    test_assembly("leaq     foo(%rip),                  %r15", 0x4c, 0x8d, 0x3d, 0x00, 0x00, 0x00, 0x00, END);

    test_assembly("movq     foo(%rax),                  %rbx", 0x48, 0x8b, 0x98, 0x00, 0x00, 0x00, 0x00, END);

    test_assembly("add      %cl,                        (%rax)",                 0x00, 0x08, END);
    test_assembly("add      %cl,                        (%rbx)",                 0x00, 0x0b, END);
    test_assembly("add      %r15w,                      (%r14)",           0x66, 0x45, 0x01, 0x3e, END);
    test_assembly("addq     $0x42,                      5(%rax)",          0x48, 0x83, 0x40, 0x05, 0x42, END);
    test_assembly("add      %bl,                        5(%rbx)",          0x00, 0x5b, 0x05, END);
    test_assembly("addq     $0x42,                      5(%rbx)",          0x48, 0x83, 0x43, 0x05, 0x42, END);
    test_assembly("addq     $0x4243,                    (%rbx)",           0x48, 0x81, 0x03, 0x43, 0x42, 0x00, 0x00, END);
    test_assembly("addq     $0x4243,                    5(%rbx)",          0x48, 0x81, 0x43, 0x05,       0x43, 0x42, 0x00, 0x00, END);
    test_assembly("addq     $0x4243,                    5(%rbx,%rcx,1)"  , 0x48, 0x81, 0x44, 0x0b, 0x05, 0x43, 0x42, 0x00, 0x00, END);
    test_assembly("addq     $0x4243,                    (%rbx,%rcx,1)",    0x48, 0x81, 0x04, 0x0b,       0x43, 0x42, 0x00, 0x00, END);

    test_assembly("test     %al,                        %bl",        0x84, 0xc3, END);
    test_assembly("test     %bl,                        %al",        0x84, 0xd8, END);
    test_assembly("test     %bx,                        %ax",  0x66, 0x85, 0xd8, END);
    test_assembly("test     %ebx,                       %eax",       0x85, 0xd8, END);
    test_assembly("test     %rbx,                       %rax", 0x48, 0x85, 0xd8, END);

    test_assembly("testb    $0x42,                      (%rax)", 0xf6, 0x00, 0x42, END);
    test_assembly("testw    $0x4243,                    (%rax)", 0x66, 0xf7, 0x00, 0x43, 0x42, END);
    test_assembly("testl    $0x42434445,                (%rax)", 0xf7, 0x00, 0x45, 0x44, 0x43, 0x42, END);
    test_assembly("testq    $0x42,                      (%rax)", 0x48, 0xf7, 0x00, 0x42, 0x00, 0x00, 0x00, END);
    test_assembly("testq    $0x4243,                    (%rax)", 0x48, 0xf7, 0x00, 0x43, 0x42, 0x00, 0x00, END);
    test_assembly("testq    $0x42434445,                (%rax)", 0x48, 0xf7, 0x00, 0x45, 0x44, 0x43, 0x42, END);

    test_assembly("test     $0x42,                      %al",        0xa8, 0x42, END);
    test_assembly("test     $0x42,                      %ax",  0x66, 0xa9, 0x42, 0x00, END);
    test_assembly("test     $0x42,                      %eax",       0xa9, 0x42, 0x00, 0x00, 0x00, END);
    test_assembly("test     $0x42,                      %rax", 0x48, 0xa9, 0x42, 0x00, 0x00, 0x00, END);

    test_assembly("jb       foo", 0x0f, 0x82, 0x00, 0x00, 0x00, 0x00, END);
    test_assembly("jb       foo", 0x0f, 0x82, 0x00, 0x00, 0x00, 0x00, END);
    test_assembly("jb       foo", 0x0f, 0x82, 0x00, 0x00, 0x00, 0x00, END);
    test_assembly("jae      foo", 0x0f, 0x83, 0x00, 0x00, 0x00, 0x00, END);
    test_assembly("jae      foo", 0x0f, 0x83, 0x00, 0x00, 0x00, 0x00, END);
    test_assembly("jae      foo", 0x0f, 0x83, 0x00, 0x00, 0x00, 0x00, END);
    test_assembly("je       foo", 0x0f, 0x84, 0x00, 0x00, 0x00, 0x00, END);
    test_assembly("je       foo", 0x0f, 0x84, 0x00, 0x00, 0x00, 0x00, END);
    test_assembly("jne      foo", 0x0f, 0x85, 0x00, 0x00, 0x00, 0x00, END);
    test_assembly("jne      foo", 0x0f, 0x85, 0x00, 0x00, 0x00, 0x00, END);
    test_assembly("jbe      foo", 0x0f, 0x86, 0x00, 0x00, 0x00, 0x00, END);
    test_assembly("jbe      foo", 0x0f, 0x86, 0x00, 0x00, 0x00, 0x00, END);
    test_assembly("ja       foo", 0x0f, 0x87, 0x00, 0x00, 0x00, 0x00, END);
    test_assembly("ja       foo", 0x0f, 0x87, 0x00, 0x00, 0x00, 0x00, END);
    test_assembly("jo       foo", 0x0f, 0x80, 0x00, 0x00, 0x00, 0x00, END);
    test_assembly("jno      foo", 0x0f, 0x81, 0x00, 0x00, 0x00, 0x00, END);
    test_assembly("js       foo", 0x0f, 0x88, 0x00, 0x00, 0x00, 0x00, END);
    test_assembly("jns      foo", 0x0f, 0x89, 0x00, 0x00, 0x00, 0x00, END);
    test_assembly("jp       foo", 0x0f, 0x8a, 0x00, 0x00, 0x00, 0x00, END);
    test_assembly("jnp      foo", 0x0f, 0x8b, 0x00, 0x00, 0x00, 0x00, END);
    test_assembly("jl       foo", 0x0f, 0x8c, 0x00, 0x00, 0x00, 0x00, END);
    test_assembly("jl       foo", 0x0f, 0x8c, 0x00, 0x00, 0x00, 0x00, END);
    test_assembly("jge      foo", 0x0f, 0x8d, 0x00, 0x00, 0x00, 0x00, END);
    test_assembly("jge      foo", 0x0f, 0x8d, 0x00, 0x00, 0x00, 0x00, END);
    test_assembly("jle      foo", 0x0f, 0x8e, 0x00, 0x00, 0x00, 0x00, END);
    test_assembly("jle      foo", 0x0f, 0x8e, 0x00, 0x00, 0x00, 0x00, END);
    test_assembly("jg       foo", 0x0f, 0x8f, 0x00, 0x00, 0x00, 0x00, END);
    test_assembly("jg       foo", 0x0f, 0x8f, 0x00, 0x00, 0x00, 0x00, END);

    test_assembly("movsbl   %bl,                        %eax",       0x0f, 0xbe, 0xc3, END);
    test_assembly("movsbw   %bl,                        %ax",  0x66, 0x0f, 0xbe, 0xc3, END);
    test_assembly("movsbq   %bl,                        %rax", 0x48, 0x0f, 0xbe, 0xc3, END);
    test_assembly("movswl   %bx,                        %eax",       0x0f, 0xbf, 0xc3, END);
    test_assembly("movswq   %bx,                        %rax", 0x48, 0x0f, 0xbf, 0xc3, END);
    test_assembly("movslq   %eax,                       %rax", 0x48,       0x63, 0xc0, END);
    test_assembly("movzbl   %bl,                        %eax",       0x0f, 0xb6, 0xc3, END);
    test_assembly("movzbq   %bl,                        %rax", 0x48, 0x0f, 0xb6, 0xc3, END);
    test_assembly("movzwl   %bx,                        %eax",       0x0f, 0xb7, 0xc3, END);
    test_assembly("movzwq   %bx,                        %rax", 0x48, 0x0f, 0xb7, 0xc3, END);

    test_assembly("cmovne   %ax,                        %bx",  0x66, 0x0f, 0x45, 0xd8, END);
    test_assembly("cmovne   %cx,                        %dx",  0x66, 0x0f, 0x45, 0xd1, END);
    test_assembly("cmovne   %cx,                        %dx",  0x66, 0x0f, 0x45, 0xd1, END);
    test_assembly("cmovne   %ecx,                       %edx",       0x0f, 0x45, 0xd1, END);
    test_assembly("cmovne   %rcx,                       %rdx", 0x48, 0x0f, 0x45, 0xd1, END);

    test_assembly("cmovo    %cx,                        %dx",  0x66, 0x0f, 0x40, 0xd1, END);
    test_assembly("cmovno   %cx,                        %dx",  0x66, 0x0f, 0x41, 0xd1, END);
    test_assembly("cmovb    %cx,                        %dx",  0x66, 0x0f, 0x42, 0xd1, END);
    test_assembly("cmovb    %cx,                        %dx",  0x66, 0x0f, 0x42, 0xd1, END);
    test_assembly("cmovb    %cx,                        %dx",  0x66, 0x0f, 0x42, 0xd1, END);
    test_assembly("cmovae   %cx,                        %dx",  0x66, 0x0f, 0x43, 0xd1, END);
    test_assembly("cmovae   %cx,                        %dx",  0x66, 0x0f, 0x43, 0xd1, END);
    test_assembly("cmovae   %cx,                        %dx",  0x66, 0x0f, 0x43, 0xd1, END);
    test_assembly("cmove    %cx,                        %dx",  0x66, 0x0f, 0x44, 0xd1, END);
    test_assembly("cmove    %cx,                        %dx",  0x66, 0x0f, 0x44, 0xd1, END);
    test_assembly("cmovne   %cx,                        %dx",  0x66, 0x0f, 0x45, 0xd1, END);
    test_assembly("cmovne   %cx,                        %dx",  0x66, 0x0f, 0x45, 0xd1, END);
    test_assembly("cmovbe   %cx,                        %dx",  0x66, 0x0f, 0x46, 0xd1, END);
    test_assembly("cmovbe   %cx,                        %dx",  0x66, 0x0f, 0x46, 0xd1, END);
    test_assembly("cmova    %cx,                        %dx",  0x66, 0x0f, 0x47, 0xd1, END);
    test_assembly("cmova    %cx,                        %dx",  0x66, 0x0f, 0x47, 0xd1, END);
    test_assembly("cmovs    %cx,                        %dx",  0x66, 0x0f, 0x48, 0xd1, END);
    test_assembly("cmovns   %cx,                        %dx",  0x66, 0x0f, 0x49, 0xd1, END);
    test_assembly("cmovp    %cx,                        %dx",  0x66, 0x0f, 0x4a, 0xd1, END);
    test_assembly("cmovp    %cx,                        %dx",  0x66, 0x0f, 0x4a, 0xd1, END);
    test_assembly("cmovnp   %cx,                        %dx",  0x66, 0x0f, 0x4b, 0xd1, END);
    test_assembly("cmovnp   %cx,                        %dx",  0x66, 0x0f, 0x4b, 0xd1, END);
    test_assembly("cmovl    %cx,                        %dx",  0x66, 0x0f, 0x4c, 0xd1, END);
    test_assembly("cmovl    %cx,                        %dx",  0x66, 0x0f, 0x4c, 0xd1, END);
    test_assembly("cmovge   %cx,                        %dx",  0x66, 0x0f, 0x4d, 0xd1, END);
    test_assembly("cmovge   %cx,                        %dx",  0x66, 0x0f, 0x4d, 0xd1, END);
    test_assembly("cmovle   %cx,                        %dx",  0x66, 0x0f, 0x4e, 0xd1, END);
    test_assembly("cmovle   %cx,                        %dx",  0x66, 0x0f, 0x4e, 0xd1, END);
    test_assembly("cmovg    %cx,                        %dx",  0x66, 0x0f, 0x4f, 0xd1, END);
    test_assembly("cmovg    %cx,                        %dx",  0x66, 0x0f, 0x4f, 0xd1, END);

    test_assembly("cwtd", 0x66, 0x99, END);
    test_assembly("cltd",       0x99, END);
    test_assembly("cqto", 0x48, 0x99, END);
    test_assembly("cwtd", 0x66, 0x99, END);
    test_assembly("cltd",       0x99, END);
    test_assembly("cqto", 0x48, 0x99, END);

    test_assembly("movss  %xmm2,                        %xmm3",       0xf3,       0x0f, 0x10, 0xda, END);
    test_assembly("movsd  %xmm2,                        %xmm3",       0xf2,       0x0f, 0x10, 0xda, END);
    test_assembly("movss  %xmm14,                       %xmm15",      0xf3, 0x45, 0x0f, 0x10, 0xfe, END);
    test_assembly("movsd  %xmm14,                       %xmm15",      0xf2, 0x45, 0x0f, 0x10, 0xfe, END);
    test_assembly("movss  %xmm14,                       (%rax)",      0xf3, 0x44, 0x0f, 0x11, 0x30, END);
    test_assembly("movss  (%rax),                       %xmm14",      0xf3, 0x44, 0x0f, 0x10, 0x30, END);
    test_assembly("movsd  %xmm14,                       (%rax)",      0xf2, 0x44, 0x0f, 0x11, 0x30, END);
    test_assembly("movsd  (%rax),                       %xmm14",      0xf2, 0x44, 0x0f, 0x10, 0x30, END);
    test_assembly("movsd  %xmm14,                       0x5(%rax)",   0xf2, 0x44, 0x0f, 0x11, 0x70, 0x05, END);
    test_assembly("movsd  %xmm14,                       0x100(%rax)", 0xf2, 0x44, 0x0f, 0x11, 0xb0, 0x00, 0x01, 0x00, 0x00, END);

    test_assembly("movss  %xmm1,                        0x42434445",  0xf3,       0x0f, 0x11, 0x0c, 0x25, 0x45, 0x44, 0x43, 0x42, END);
    test_assembly("movss  %xmm14,                       0x42434445",  0xf3, 0x44, 0x0f, 0x11, 0x34, 0x25, 0x45, 0x44, 0x43, 0x42, END);
    test_assembly("movsd  %xmm1,                        0x42434445",  0xf2,       0x0f, 0x11, 0x0c, 0x25, 0x45, 0x44, 0x43, 0x42, END);
    test_assembly("movsd  %xmm14,                       0x42434445",  0xf2, 0x44, 0x0f, 0x11, 0x34, 0x25, 0x45, 0x44, 0x43, 0x42, END);

    test_assembly("movss  %xmm1,                        0x42434445",  0xf3,       0x0f, 0x11, 0x0c, 0x25, 0x45, 0x44, 0x43, 0x42, END);
    test_assembly("movss  %xmm14,                       0x42434445",  0xf3, 0x44, 0x0f, 0x11, 0x34, 0x25, 0x45, 0x44, 0x43, 0x42, END);
    test_assembly("movsd  %xmm1,                        0x42434445",  0xf2,       0x0f, 0x11, 0x0c, 0x25, 0x45, 0x44, 0x43, 0x42, END);
    test_assembly("movsd  %xmm14,                       0x42434445",  0xf2, 0x44, 0x0f, 0x11, 0x34, 0x25, 0x45, 0x44, 0x43, 0x42, END);
    test_assembly("movss  0x42434445,                   %xmm1",       0xf3,       0x0f, 0x10, 0x0c, 0x25, 0x45, 0x44, 0x43, 0x42, END);
    test_assembly("movss  0x42434445,                   %xmm14",      0xf3, 0x44, 0x0f, 0x10, 0x34, 0x25, 0x45, 0x44, 0x43, 0x42, END);
    test_assembly("movsd  0x42434445,                   %xmm1",       0xf2,       0x0f, 0x10, 0x0c, 0x25, 0x45, 0x44, 0x43, 0x42, END);
    test_assembly("movsd  0x42434445,                   %xmm14",      0xf2, 0x44, 0x0f, 0x10, 0x34, 0x25, 0x45, 0x44, 0x43, 0x42, END);
    test_assembly("movsd  foo,                          %xmm14",      0xf2, 0x44, 0x0f, 0x10, 0x34, 0x25, 0x00, 0x00, 0x00, 0x00, END);
    test_assembly("movsd  %xmm14,                       foo",         0xf2, 0x44, 0x0f, 0x11, 0x34, 0x25, 0x00, 0x00, 0x00, 0x00, END);

    test_assembly("addss  %xmm14,                       %xmm15",      0xf3, 0x45, 0x0f, 0x58, 0xfe, END);
    test_assembly("addsd  %xmm14,                       %xmm15",      0xf2, 0x45, 0x0f, 0x58, 0xfe, END);
    test_assembly("subss  %xmm14,                       %xmm15",      0xf3, 0x45, 0x0f, 0x5c, 0xfe, END);
    test_assembly("subsd  %xmm14,                       %xmm15",      0xf2, 0x45, 0x0f, 0x5c, 0xfe, END);
    test_assembly("mulss  %xmm14,                       %xmm15",      0xf3, 0x45, 0x0f, 0x59, 0xfe, END);
    test_assembly("mulsd  %xmm14,                       %xmm15",      0xf2, 0x45, 0x0f, 0x59, 0xfe, END);
    test_assembly("divss  %xmm14,                       %xmm15",      0xf3, 0x45, 0x0f, 0x5e, 0xfe, END);
    test_assembly("divsd  %xmm14,                       %xmm15",      0xf2, 0x45, 0x0f, 0x5e, 0xfe, END);

    test_assembly("comiss  %xmm14,                      %xmm15",             0x45, 0x0f, 0x2f, 0xfe, END);
    test_assembly("comisd  %xmm14,                      %xmm15",       0x66, 0x45, 0x0f, 0x2f, 0xfe, END);
    test_assembly("ucomiss %xmm14,                      %xmm15",             0x45, 0x0f, 0x2e, 0xfe, END);
    test_assembly("ucomisd %xmm14,                      %xmm15",       0x66, 0x45, 0x0f, 0x2e, 0xfe, END);

    test_assembly("cvtsd2ss %xmm15,                     %xmm14", 0xf2, 0x45, 0x0f, 0x5a, 0xf7, END);
    test_assembly("cvtsd2ss (%rax),                     %xmm15", 0xf2, 0x44, 0x0f, 0x5a, 0x38, END);
    test_assembly("cvtss2si  %xmm15,                    %eax",   0xf3, 0x41, 0x0f, 0x2d, 0xc7, END);
    test_assembly("cvtss2si  %xmm15,                    %rax",   0xf3, 0x49, 0x0f, 0x2d, 0xc7, END);
    test_assembly("cvtsd2si  %xmm15,                    %eax",   0xf2, 0x41, 0x0f, 0x2d, 0xc7, END);
    test_assembly("cvtsd2si  %xmm15,                    %rax",   0xf2, 0x49, 0x0f, 0x2d, 0xc7, END);

    test_assembly("cvttss2si %xmm14,                    %eax",   0xf3, 0x41, 0x0f, 0x2c, 0xc6, END);
    test_assembly("cvttss2si %xmm14,                    %rax",   0xf3, 0x49, 0x0f, 0x2c, 0xc6, END);
    test_assembly("cvttsd2si %xmm14,                    %eax",   0xf2, 0x41, 0x0f, 0x2c, 0xc6, END);
    test_assembly("cvttsd2si %xmm14,                    %rax",   0xf2, 0x49, 0x0f, 0x2c, 0xc6, END);

    test_assembly("faddp  %st,                          %st(1)", 0xde, 0xc1, END);
    test_assembly("fsubp  %st,                          %st(1)", 0xde, 0xe1, END);
    test_assembly("fmulp  %st,                          %st(1)", 0xde, 0xc9, END);
    test_assembly("fdivp  %st,                          %st(1)", 0xde, 0xf1, END);
    test_assembly("fsubrp %st,                          %st(1)", 0xde, 0xe9, END);
    test_assembly("fdivrp %st,                          %st(1)", 0xde, 0xf9, END);

    test_assembly("faddp  %st,                          %st(0)", 0xde, 0xc0, END);
    test_assembly("faddp  %st(0),                       %st(0)", 0xde, 0xc0, END);
    test_assembly("faddp  %st,                          %st(1)", 0xde, 0xc1, END);
    test_assembly("faddp  %st(0),                       %st(1)", 0xde, 0xc1, END);
    test_assembly("faddp  %st,                          %st(2)", 0xde, 0xc2, END);
    test_assembly("faddp  %st(0),                       %st(2)", 0xde, 0xc2, END);

    test_assembly("fxch   %st(1)", 0xd9, 0xc9, END);
    test_assembly("fxch   %st(2)", 0xd9, 0xca, END);

    test_assembly("ret", 0xc3, END);
    test_assembly("retq", 0xc3, END);
    test_assembly("leave", 0xc9, END);
    test_assembly("leaveq", 0xc9, END);

    test_assembly("push     %rax", 0x50, END);
    test_assembly("push     %rbx", 0x53, END);
    test_assembly("push     %r15", 0x41, 0x57, END);
    test_assembly("pop      %rax", 0x58, END);
    test_assembly("pop      %rbx", 0x5b, END);
    test_assembly("pop      %r15", 0x41, 0x5f, END);

    test_assembly("callq    foo@PLT", 0xe8, 00, 00, 00, 00, END);
    test_assembly("callq    1", 0xe8, 00, 00, 00, 00, END);
}

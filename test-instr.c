#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "elf.h"
#include "lexer.h"
#include "instr.h"
#include "parser.h"
#include "relocations.h"
#include "symbols.h"
#include "utils.h"
#include "test-utils.h"

void assert_instructions(Instructions* instr, va_list ap) {
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

void test_assembly(char *input, ...) {
    va_list ap;
    va_start(ap, input);

    printf("%-60s", input);
    init_lexer_from_string(input);
    init_parser();
    InstructionsSet *instr_set = parse_instruction_statement();
    Instructions *instr = instr_set->primary;
    assert_instructions(instr, ap);

    printf("pass\n");
}

void test_parse_instruction_statement() {
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
    test_assembly("mov      $0x0,                       %spl", 0x40, 0xb4, 0x00, END);
    test_assembly("mov      $0x0,                       %bpl", 0x40, 0xb5, 0x00, END);
    test_assembly("mov      $0x0,                       %sil", 0x40, 0xb6, 0x00, END);
    test_assembly("mov      $0x0,                       %dil", 0x40, 0xb7, 0x00, END);
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

    test_assembly("add      $0x7fff,                    %ax",  0x66, 0x05, 0xff, 0x7f, END);
    test_assembly("add      $0x8000,                    %ax",  0x66, 0x05, 0x00, 0x80, END);
    test_assembly("add      $0xffff,                    %ax",  0x66, 0x05, 0xff, 0xff, END); // Differs with gcc, same length
    test_assembly("add      $0xffff,                    %ax",  0x66, 0x05, 0xff, 0xff, END); // Differs with gcc, same length
    test_assembly("add      $0x7fffffff,                %eax", 0x05, 0xff, 0xff, 0xff, 0x7f, END);
    test_assembly("add      $0x80000000,                %eax", 0x05, 0x00, 0x00, 0x00, 0x80, END); // Differs with  gcc, can be shortened to 0x83, 0xc0, 0xff
    test_assembly("add      $0xffffffff,                %eax", 0x05, 0xff, 0xff, 0xff, 0xff, END);
    test_assembly("add      $-1,                        %eax", 0x83, 0xc0, 0xff, END);

    // Check code generation with size in the mnemnonic
    test_assembly("addb     $42,                        %al",  0x04, 0x2a, END);
    test_assembly("addw     $42,                        %ax",  0x66, 0x05, 0x2a, 0x00, END);
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
    test_assembly("mov      $0x42,                      %eax", 0xb8, 0x42, 0x00, 0x00, 0x00, END);
    test_assembly("mov      $0x42,                      %rax", 0x48, 0xc7, 0xc0, 0x42, 0x00, 0x00, 0x00, END);
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

    // Check avoidance of instructions that would sign extend
    test_assembly("mov      $0x7f,                      %al",        0xb0,       0x7f, END);
    test_assembly("mov      $0x80,                      %al",        0xb0,       0x80, END);
    test_assembly("mov      $-1,                        %al",        0xb0,       0xff, END);
    test_assembly("mov      $0x7f,                      %ax",  0x66, 0xb8,       0x7f, 0x00, END);
    test_assembly("mov      $0x80,                      %ax",  0x66, 0xb8,       0x80, 0x00, END);
    test_assembly("mov      $-1,                        %ax",  0x66, 0xb8,       0xff, 0xff, END);
    test_assembly("mov      $0x7f,                      %eax",       0xb8,       0x7f, 0x00, 0x00, 0x00, END);
    test_assembly("mov      $0x80,                      %eax",       0xb8,       0x80, 0x00, 0x00, 0x00, END);
    test_assembly("mov      $0x7f,                      %rax", 0x48, 0xc7,       0xc0, 0x7f, 0x00, 0x00, 0x00, END);
    test_assembly("mov      $0x80,                      %rax", 0x48, 0xc7,       0xc0, 0x80, 0x00, 0x00, 0x00, END);
    test_assembly("mov      $0x7fff,                    %ax",  0x66, 0xb8,       0xff, 0x7f, END);
    test_assembly("mov      $0x8000,                    %ax",  0x66, 0xb8,       0x00, 0x80, END);
    test_assembly("mov      $0x7fff,                    %eax",       0xb8,       0xff, 0x7f, 0x00, 0x00, END);
    test_assembly("mov      $0x8000,                    %eax",       0xb8,       0x00, 0x80, 0x00, 0x00, END);
    test_assembly("mov      $-1,                        %eax",       0xb8,       0xff, 0xff, 0xff, 0xff, END);
    test_assembly("mov      $0x7fff,                    %rax", 0x48, 0xc7, 0xc0, 0xff, 0x7f, 0x00, 0x00, END);
    test_assembly("mov      $0x8000,                    %rax", 0x48, 0xc7, 0xc0, 0x00, 0x80, 0x00, 0x00, END);
    test_assembly("mov      $0x7fffffff,                %eax",       0xb8,       0xff, 0xff, 0xff, 0x7f, END);
    test_assembly("mov      $0x80000000,                %eax",       0xb8,       0x00, 0x00, 0x00, 0x80, END);
    test_assembly("mov      $0x7fffffff,                %rax", 0x48, 0xc7, 0xc0, 0xff, 0xff, 0xff, 0x7f, END);
    test_assembly("mov      $-1,                        %rax", 0x48, 0xc7, 0xc0, 0xff, 0xff, 0xff, 0xff, END);
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

    test_assembly("mov      %r15,                       -0x80       (%rax)", 0x4c, 0x89, 0x78, 0x80,                   END);
    test_assembly("mov      %r15,                       -0x7f       (%rax)", 0x4c, 0x89, 0x78, 0x81,                   END);
    test_assembly("mov      %r15,                       -0x1(       %rax)",  0x4c, 0x89, 0x78, 0xff,                   END);
    test_assembly("mov      %r15,                        0x7f       (%rax)", 0x4c, 0x89, 0x78, 0x7f,                   END);
    test_assembly("mov      %r15,                       -0x80000000 (%rax)", 0x4c, 0x89, 0xb8, 0x00, 0x00, 0x00, 0x80, END);
    test_assembly("mov      %r15,                       -0x7fffffff (%rax)", 0x4c, 0x89, 0xb8, 0x01, 0x00, 0x00, 0x80, END);
    test_assembly("mov      %r15,                       -0x81       (%rax)", 0x4c, 0x89, 0xb8, 0x7f, 0xff, 0xff, 0xff, END);
    test_assembly("mov      %r15,                        0x80       (%rax)", 0x4c, 0x89, 0xb8, 0x80, 0x00, 0x00, 0x00, END);
    test_assembly("mov      %r15,                        0xff       (%rax)", 0x4c, 0x89, 0xb8, 0xff, 0x00, 0x00, 0x00, END);
    test_assembly("mov      %r15,                        0x7fffffff (%rax)", 0x4c, 0x89, 0xb8, 0xff, 0xff, 0xff, 0x7f, END);

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

    test_assembly("movb     %al,                        0x0(%rip)",       0x88, 0x05, 0x00, 0x00, 0x00, 0x00, END);
    test_assembly("movw     %ax,                        0x0(%rip)", 0x66, 0x89, 0x05, 0x00, 0x00, 0x00, 0x00, END);
    test_assembly("movl     %eax,                       0x0(%rip)",       0x89, 0x05, 0x00, 0x00, 0x00, 0x00, END);
    test_assembly("movq     %rbx,                       0x0(%rip)", 0x48, 0x89, 0x1d, 0x00, 0x00, 0x00, 0x00, END);
    test_assembly("movb     0x0(%rip),                  %al",             0x8a, 0x05, 0x00, 0x00, 0x00, 0x00, END);
    test_assembly("movw     0x0(%rip),                  %ax",       0x66, 0x8b, 0x05, 0x00, 0x00, 0x00, 0x00, END);
    test_assembly("movl     0x0(%rip),                  %eax",            0x8b, 0x05, 0x00, 0x00, 0x00, 0x00, END);
    test_assembly("movq     0x0(%rip),                  %rbx",      0x48, 0x8b, 0x1d, 0x00, 0x00, 0x00, 0x00, END);

    test_assembly("mov      %r15,                       foo - 0x42(%rip)", 0x4c, 0x89, 0x3d, 0x00, 0x00, 0x00, 0x00, END);
    test_assembly("mov      %r15,                       foo + 0x00(%rip)", 0x4c, 0x89, 0x3d, 0x00, 0x00, 0x00, 0x00, END);
    test_assembly("mov      %r15,                       foo + 0x42(%rip)", 0x4c, 0x89, 0x3d, 0x00, 0x00, 0x00, 0x00, END);

    test_assembly("movb     $1,                         (%rip)",       0xc6, 0x05, 0x00, 0x00, 0x00, 0x00, 0x01, END);
    test_assembly("movw     $1,                         (%rip)", 0x66, 0xc7, 0x05, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, END);
    test_assembly("movl     $1,                         (%rip)",       0xc7, 0x05, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, END);
    test_assembly("movq     $1,                         (%rip)", 0x48, 0xc7, 0x05, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, END);

    test_assembly("movb     $0x42,                      foo",          0xc6, 0x04, 0x25, 0x00, 0x00, 0x00, 0x00, 0x42, END);
    test_assembly("movw     $0x42,                      foo",    0x66, 0xc7, 0x04, 0x25, 0x00, 0x00, 0x00, 0x00, 0x42, 0x00, END);
    test_assembly("movl     $0x42,                      foo",          0xc7, 0x04, 0x25, 0x00, 0x00, 0x00, 0x00, 0x42, 0x00, 0x00, 0x00, END);
    test_assembly("movq     $0x42,                      foo",    0x48, 0xc7, 0x04, 0x25, 0x00, 0x00, 0x00, 0x00, 0x42, 0x00, 0x00, 0x00, END);

    test_assembly("movq     2 + 8(%rax),                %rbx",           0x48, 0x8b, 0x58, 0x0a, END);
    test_assembly("movq     %rbx,                       2 + 8(%rax)",    0x48, 0x89, 0x58, 0x0a, END);
    test_assembly("movq     %rbx,                       2 + 8 +2(%rax)", 0x48, 0x89, 0x58, 0x0c, END);

    test_assembly("leaq     0(%rip),                    %r15", 0x4c, 0x8d, 0x3d, 0x00, 0x00, 0x00, 0x00, END);
    test_assembly("leaq     5(%rip),                    %r15", 0x4c, 0x8d, 0x3d, 0x05, 0x00, 0x00, 0x00, END);
    test_assembly("leaq     -5(%rip),                   %r15", 0x4c, 0x8d, 0x3d, 0xfb, 0xff, 0xff, 0xff, END);
    test_assembly("leaq     foo(%rip),                  %r15", 0x4c, 0x8d, 0x3d, 0x00, 0x00, 0x00, 0x00, END);

    test_assembly("movq     foo(%rax),                  %rbx", 0x48, 0x8b, 0x98, 0x00, 0x00, 0x00, 0x00, END);

    test_assembly("add      %cl,                        (%rax)",                 0x00, 0x08, END);
    test_assembly("add      %cl,                        (%rbx)",                 0x00, 0x0b, END);
    test_assembly("add      %r15w,                      (%r14)",           0x66, 0x45, 0x01, 0x3e, END);
    test_assembly("add      $0x42,                      (%rbx)",                 0x83, 0x03, 0x42, END);
    test_assembly("addq     $0x42,                      5(%rax)",          0x48, 0x83, 0x40, 0x05, 0x42, END);
    test_assembly("add      %bl,                        5(%rbx)",          0x00, 0x5b, 0x05, END);
    test_assembly("addq     $0x42,                      5(%rbx)",          0x48, 0x83, 0x43, 0x05, 0x42, END);
    test_assembly("addq     $0x4243,                    (%rbx)",           0x48, 0x81, 0x03, 0x43, 0x42, 0x00, 0x00, END);
    test_assembly("addq     $0x4243,                    5(%rbx)",          0x48, 0x81, 0x43, 0x05,       0x43, 0x42, 0x00, 0x00, END);
    test_assembly("addq     $0x4243,                    5(%rbx,%rcx,1)"  , 0x48, 0x81, 0x44, 0x0b, 0x05, 0x43, 0x42, 0x00, 0x00, END);
    test_assembly("addq     $0x4243,                    (%rbx,%rcx,1)",    0x48, 0x81, 0x04, 0x0b,       0x43, 0x42, 0x00, 0x00, END);

    // Testing of 2 and 3 operand imulw with lots of signed edge cases
    test_assembly("imulw     $-0x80,                     %bx, %bx",                 0x66, 0x6b, 0xdb, 0x80,                               END);
    test_assembly("imulw     $-0x7f,                     %bx, %bx",                 0x66, 0x6b, 0xdb, 0x81,                               END);
    test_assembly("imulw     $0x7f,                      %bx, %bx",                 0x66, 0x6b, 0xdb, 0x7f,                               END);
    test_assembly("imulw     $0x80,                      %bx, %bx",                 0x66, 0x69, 0xdb, 0x80, 0x00,                         END);
    test_assembly("imulw     $0xff,                      %bx, %bx",                 0x66, 0x69, 0xdb, 0xff, 0x00,                         END);
    test_assembly("imull     $-0x80,                     %ebx, %ebx",               0x6b, 0xdb, 0x80,                                     END);
    test_assembly("imull     $-0x7f,                     %ebx, %ebx",               0x6b, 0xdb, 0x81,                                     END);
    test_assembly("imull     $0x7f,                      %ebx, %ebx",               0x6b, 0xdb, 0x7f,                                     END);
    test_assembly("imull     $0x80,                      %ebx, %ebx",               0x69, 0xdb, 0x80, 0x00, 0x00, 0x00,                   END);
    test_assembly("imull     $0xff,                      %ebx, %ebx",               0x69, 0xdb, 0xff, 0x00, 0x00, 0x00,                   END);
    test_assembly("imulq     $-0x80,                     %rbx, %rbx",               0x48, 0x6b, 0xdb, 0x80,                               END);
    test_assembly("imulq     $-0x7f,                     %rbx, %rbx",               0x48, 0x6b, 0xdb, 0x81,                               END);
    test_assembly("imulq     $0x7f,                      %rbx, %rbx",               0x48, 0x6b, 0xdb, 0x7f,                               END);
    test_assembly("imulq     $0x80,                      %rbx, %rbx",               0x48, 0x69, 0xdb, 0x80, 0x00, 0x00, 0x00,             END);
    test_assembly("imulq     $0xff,                      %rbx, %rbx",               0x48, 0x69, 0xdb, 0xff, 0x00, 0x00, 0x00,             END);
    test_assembly("imulw     $-0x8000,                   %bx, %bx",                 0x66, 0x69, 0xdb, 0x00, 0x80,                         END);
    test_assembly("imulw     $-0x7fff,                   %bx, %bx",                 0x66, 0x69, 0xdb, 0x01, 0x80,                         END);
    test_assembly("imulw     $0x7fff,                    %bx, %bx",                 0x66, 0x69, 0xdb, 0xff, 0x7f,                         END);
    test_assembly("imulw     $0x8000,                    %bx, %bx",                 0x66, 0x69, 0xdb, 0x00, 0x80,                         END);
    test_assembly("imulw     $0xffff,                    %bx, %bx",                 0x66, 0x69, 0xdb, 0xff, 0xff,                         END);
    test_assembly("imull     $-0x8000,                   %ebx, %ebx",               0x69, 0xdb, 0x00, 0x80, 0xff, 0xff,                   END);
    test_assembly("imull     $-0x7fff,                   %ebx, %ebx",               0x69, 0xdb, 0x01, 0x80, 0xff, 0xff,                   END);
    test_assembly("imull     $0x7fff,                    %ebx, %ebx",               0x69, 0xdb, 0xff, 0x7f, 0x00, 0x00,                   END);
    test_assembly("imull     $0x8000,                    %ebx, %ebx",               0x69, 0xdb, 0x00, 0x80, 0x00, 0x00,                   END);
    test_assembly("imull     $0xffff,                    %ebx, %ebx",               0x69, 0xdb, 0xff, 0xff, 0x00, 0x00,                   END);
    test_assembly("imulq     $-0x8000,                   %rbx, %rbx",               0x48, 0x69, 0xdb, 0x00, 0x80, 0xff, 0xff,             END);
    test_assembly("imulq     $-0x7fff,                   %rbx, %rbx",               0x48, 0x69, 0xdb, 0x01, 0x80, 0xff, 0xff,             END);
    test_assembly("imulq     $0x7fff,                    %rbx, %rbx",               0x48, 0x69, 0xdb, 0xff, 0x7f, 0x00, 0x00,             END);
    test_assembly("imulq     $0x8000,                    %rbx, %rbx",               0x48, 0x69, 0xdb, 0x00, 0x80, 0x00, 0x00,             END);
    test_assembly("imulq     $0xffff,                    %rbx, %rbx",               0x48, 0x69, 0xdb, 0xff, 0xff, 0x00, 0x00,             END);
    test_assembly("imull     $-0x80000000,               %ebx, %ebx",               0x69, 0xdb, 0x00, 0x00, 0x00, 0x80,                   END);
    test_assembly("imull     $-0x7fffffff,               %ebx, %ebx",               0x69, 0xdb, 0x01, 0x00, 0x00, 0x80,                   END);
    test_assembly("imull     $0x7fffffff,                %ebx, %ebx",               0x69, 0xdb, 0xff, 0xff, 0xff, 0x7f,                   END);
    test_assembly("imull     $0x80000000,                %ebx, %ebx",               0x69, 0xdb, 0x00, 0x00, 0x00, 0x80,                   END);
    test_assembly("imull     $0xffffffff,                %ebx, %ebx",               0x69, 0xdb, 0xff, 0xff, 0xff, 0xff,                   END);
    test_assembly("imulq     $-0x80000000,               %rbx, %rbx",               0x48, 0x69, 0xdb, 0x00, 0x00, 0x00, 0x80,             END);
    test_assembly("imulq     $-0x7fffffff,               %rbx, %rbx",               0x48, 0x69, 0xdb, 0x01, 0x00, 0x00, 0x80,             END);
    test_assembly("imulq     $0x7fffffff,                %rbx, %rbx",               0x48, 0x69, 0xdb, 0xff, 0xff, 0xff, 0x7f,             END);
    test_assembly("imulq     $0x7fffffff,                %rbx, %rcx",               0x48, 0x69, 0xcb, 0xff, 0xff, 0xff, 0x7f,             END);
    test_assembly("imulq     $0x7fffffff,                2(%rbx), %rcx",            0x48, 0x69, 0x4b, 0x02, 0xff, 0xff, 0xff, 0x7f,       END);
    test_assembly("imulq     $0x7fffffff,                2(%rbx,%rdx,4), %rcx",     0x48, 0x69, 0x4c, 0x93, 0x02, 0xff, 0xff, 0xff, 0x7f, END);
    test_assembly("imulq     $0x7fffffff,                %rbx, %rcx",               0x48, 0x69, 0xcb, 0xff, 0xff, 0xff, 0x7f,             END);
    test_assembly("imulq     $0x7fffffff,                %rbx, %rbx",               0x48, 0x69, 0xdb, 0xff, 0xff, 0xff, 0x7f,             END);


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
    test_assembly("movzbw   %bl,                        %ax",  0x66, 0x0f, 0xb6, 0xc3, END);
    test_assembly("movzbq   %bl,                        %rax", 0x48, 0x0f, 0xb6, 0xc3, END);
    test_assembly("movzwl   %bx,                        %eax",       0x0f, 0xb7, 0xc3, END);
    test_assembly("movzwq   %bx,                        %rax", 0x48, 0x0f, 0xb7, 0xc3, END);

    test_assembly("movsbw   (%rbp),                     %ax",  0x66, 0x0f, 0xbe, 0x45, 0x00, END);
    test_assembly("movsbl   (%rbp),                     %eax", 0x0f, 0xbe, 0x45, 0x00, END);
    test_assembly("movsbq   (%rbp),                     %rax", 0x48, 0x0f, 0xbe, 0x45, 0x00, END);
    test_assembly("movslq   (%rbp),                     %rax", 0x48, 0x63, 0x45, 0x00, END);
    test_assembly("movswl   (%rbp),                     %eax", 0x0f, 0xbf, 0x45, 0x00, END);
    test_assembly("movswq   (%rbp),                     %rax", 0x48, 0x0f, 0xbf, 0x45, 0x00, END);
    test_assembly("movzbl   (%rbp),                     %eax", 0x0f, 0xb6, 0x45, 0x00, END);
    test_assembly("movzbq   (%rbp),                     %rax", 0x48, 0x0f, 0xb6, 0x45, 0x00, END);
    test_assembly("movzwl   (%rbp),                     %eax", 0x0f, 0xb7, 0x45, 0x00, END);
    test_assembly("movzwq   (%rbp),                     %rax", 0x48, 0x0f, 0xb7, 0x45, 0x00, END);

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

    test_assembly("sarb   %cl,                          %r15b",             0x41, 0xd2, 0xff, END);
    test_assembly("sarw   %cl,                          %r15w",       0x66, 0x41, 0xd3, 0xff, END);
    test_assembly("sarl   %cl,                          %r15d",             0x41, 0xd3, 0xff, END);
    test_assembly("sarq   %cl,                          %r15",              0x49, 0xd3, 0xff, END);

    test_assembly("cmp    $0x42,                        %al",          0x3c, 0x42, END);

    test_assembly("comiss  %xmm14,                      %xmm15",             0x45, 0x0f, 0x2f, 0xfe, END);
    test_assembly("comisd  %xmm14,                      %xmm15",       0x66, 0x45, 0x0f, 0x2f, 0xfe, END);
    test_assembly("ucomiss %xmm14,                      %xmm15",             0x45, 0x0f, 0x2e, 0xfe, END);
    test_assembly("ucomisd %xmm14,                      %xmm15",       0x66, 0x45, 0x0f, 0x2e, 0xfe, END);

    test_assembly("cvtsd2ss %xmm15,                     %xmm14", 0xf2, 0x45, 0x0f, 0x5a, 0xf7, END);    // Convert Scalar Double-FP Value to Scalar Single-FP Value
    test_assembly("cvtsd2ss (%rax),                     %xmm15", 0xf2, 0x44, 0x0f, 0x5a, 0x38, END);

    test_assembly("cvtss2si  %xmm15,                    %eax",   0xf3, 0x41, 0x0f, 0x2d, 0xc7, END);    // Convert Scalar Single-FP Value to DW Integer
    test_assembly("cvtss2si  %xmm15,                    %rax",   0xf3, 0x49, 0x0f, 0x2d, 0xc7, END);
    test_assembly("cvtsd2si  %xmm15,                    %eax",   0xf2, 0x41, 0x0f, 0x2d, 0xc7, END);
    test_assembly("cvtsd2si  %xmm15,                    %rax",   0xf2, 0x49, 0x0f, 0x2d, 0xc7, END);

    test_assembly("cvttss2si %xmm14,                    %eax",   0xf3, 0x41, 0x0f, 0x2c, 0xc6, END);    // Convert with Trunc. Scalar Single-FP Value to DW Integer
    test_assembly("cvttss2si %xmm14,                    %rax",   0xf3, 0x49, 0x0f, 0x2c, 0xc6, END);
    test_assembly("cvttsd2si %xmm14,                    %eax",   0xf2, 0x41, 0x0f, 0x2c, 0xc6, END);
    test_assembly("cvttsd2si %xmm14,                    %rax",   0xf2, 0x49, 0x0f, 0x2c, 0xc6, END);

    test_assembly("cvtsi2ss  %ebx,                      %xmm0", 0xf3 ,0x0f ,0x2a ,0xc3, END);           // Convert DW Integer to Scalar Single-FP Value
    test_assembly("cvtsi2ssl %ebx,                      %xmm0", 0xf3 ,0x0f ,0x2a ,0xc3, END);           // Convert DW Integer to Scalar Single-FP Value
    test_assembly("cvtsi2ssq %rbx,                      %xmm0", 0xf3, 0x48, 0x0f, 0x2a, 0xc3, END);     // Convert DW Integer to Scalar Single-FP Value
    test_assembly("cvtsi2sd  %eax,                      %xmm0", 0xf2, 0x0f, 0x2a, 0xc0, END);           // Convert DW Integer to Scalar Double-FP Value
    test_assembly("cvtsi2sd  %rbx,                      %xmm0", 0xf2, 0x48, 0x0f, 0x2a, 0xc3, END);     // Convert DW Integer to Scalar Double-FP Value
    test_assembly("cvtsi2sdl %eax,                      %xmm0", 0xf2, 0x0f, 0x2a, 0xc0, END);
    test_assembly("cvtsi2sdq %rbx,                      %xmm0", 0xf2, 0x48, 0x0f, 0x2a, 0xc3, END);     // Convert DW Integer to Scalar Double-FP Value

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

    test_assembly("fild   (%rcx)",       0xdf, 0x01, END);
    test_assembly("fild   (%r15)", 0x41, 0xdf, 0x07, END);
    test_assembly("filds  (%rcx)",       0xdf, 0x01, END);
    test_assembly("filds  (%rdx)",       0xdf, 0x02, END);
    test_assembly("filds  (%r15)", 0x41, 0xdf, 0x07, END);
    test_assembly("fildl  (%rcx)",       0xdb, 0x01, END);
    test_assembly("fildl  (%r15)", 0x41, 0xdb, 0x07, END);
    test_assembly("fildq  (%rcx)",       0xdf, 0x29, END);
    test_assembly("fildq  (%r15)", 0x41, 0xdf, 0x2f, END);
    test_assembly("fildll (%rcx)",       0xdf, 0x29, END);
    test_assembly("fildll (%r15)", 0x41, 0xdf, 0x2f, END);

    test_assembly("fistp   (%rcx)",       0xdf, 0x19, END);
    test_assembly("fistp   (%r15)", 0x41, 0xdf, 0x1f, END);
    test_assembly("fistps  (%rcx)",       0xdf, 0x19, END);
    test_assembly("fistps  (%rdx)",       0xdf, 0x1a, END);
    test_assembly("fistps  (%r15)", 0x41, 0xdf, 0x1f, END);
    test_assembly("fistpl  (%rcx)",       0xdb, 0x19, END);
    test_assembly("fistpl  (%r15)", 0x41, 0xdb, 0x1f, END);
    test_assembly("fistpq  (%rcx)",       0xdf, 0x39, END);
    test_assembly("fistpq  (%r15)", 0x41, 0xdf, 0x3f, END);
    test_assembly("fistpll (%rcx)",       0xdf, 0x39, END);
    test_assembly("fistpll (%r15)", 0x41, 0xdf, 0x3f, END);

    test_assembly("fldz", 0xd9, 0xee, END);

    test_assembly("fadd  (%rax)",           0xd8, 0x00, END);
    test_assembly("fadd  (%rbx,%rcx,4)",    0xd8, 0x04, 0x8b, END);
    test_assembly("fadd  0x0",              0xd8, 0x04, 0x25, 0x00, 0x00, 0x00, 0x00, END);
    test_assembly("fadds  (%rax)",          0xd8, 0x00, END);
    test_assembly("fadds  (%rbx,%rcx,4)",   0xd8, 0x04, 0x8b, END);
    test_assembly("fadds  0x0",             0xd8, 0x04, 0x25, 0x00, 0x00, 0x00, 0x00, END);

    test_assembly("fld    (%r15)",       0x41, 0xd9, 0x07, END);
    test_assembly("flds   (%r15)",       0x41, 0xd9, 0x07, END);
    test_assembly("flds   (%r15)",       0x41, 0xd9, 0x07, END);
    test_assembly("flds   0x5(%r15)",    0x41, 0xd9, 0x47, 0x05, END);
    test_assembly("flds   0x5(%r15)",    0x41, 0xd9, 0x47, 0x05, END);
    test_assembly("fldt   0x5(%r15)",    0x41, 0xdb, 0x6f, 0x05, END);
    test_assembly("fldl   0x5(%r15)",    0x41, 0xdd, 0x47, 0x05, END);

    test_assembly("fstp    (%r15)",       0x41, 0xd9, 0x1f, END);
    test_assembly("fstps   (%r15)",       0x41, 0xd9, 0x1f, END);
    test_assembly("fstps   (%r15)",       0x41, 0xd9, 0x1f, END);
    test_assembly("fstps   0x5(%r15)",    0x41, 0xd9, 0x5f, 0x05, END);
    test_assembly("fstps   0x5(%r15)",    0x41, 0xd9, 0x5f, 0x05, END);
    test_assembly("fstpt   0x5(%r15)",    0x41, 0xdb, 0x7f, 0x05, END);
    test_assembly("fstpl   0x5(%r15)",    0x41, 0xdd, 0x5f, 0x05, END);

    test_assembly("fldcw  (%rax)",  0xd9, 0x28, END);
    test_assembly("fldcw  0x0",     0xd9, 0x2c, 0x25, 0x00, 0x00, 0x00, 0x00, END);
    test_assembly("fnstcw (%rax)",  0xd9, 0x38, END);
    test_assembly("fnstcw 0x0",     0xd9, 0x3c, 0x25, 0x00, 0x00, 0x00, 0x00, END);

    test_assembly("fcomip %st(1),%st", 0xdf, 0xf1, END);
    test_assembly("fucomip %st(1),%st", 0xdf, 0xe9, END);

    test_assembly("fcmovnbe %st(1),%st", 0xdb, 0xd1, END);

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
    test_assembly("callq    1",       0xe8, 00, 00, 00, 00, END);

    test_assembly("callq    *%rbx",         0xff, 0xd3, END);
    test_assembly("callq    *%r15",   0x41, 0xff, 0xd7, END);
}

void test_reduce_branch_instructions(void) {
    char *input =
        "top:\n"
        "    jz foo\n"
        "    jz bar\n"
        "    jz top\n"
        "    jz top\n"
        "foo:\n"
        "   nop\n"
        "bar:\n"
        "    nop\n";

    test_full_assembly("reduce_branch_instructions", input,
        0x74, 0x06,     // jz foo
        0x74, 0x05,     // jz bar
        0x74, 0xfa,     // jz top
        0x74, 0xf8,     // jz top
        0x90,           // nop
        0x90,           // nop
        END);
}

// foo is not defined, so must be added to the relocation table.
// A 32 bit RIP-relative relocation is added + different addends
void test_relocations_with_rip_and_undefined_symbol(void) {
    char *input =
        "mov %r15, foo - 0x42(%rip)\n"
        "mov %r15, foo + 0x00(%rip)\n"
        "mov %r15, foo + 0x42(%rip)\n";

    test_full_assembly("relocations_with_rip_and_undefined_symbol", input,
        0x4c, 0x89, 0x3d, 0x00, 0x00, 0x00, 0x00,       // offset 0x00
        0x4c, 0x89, 0x3d, 0x00, 0x00, 0x00, 0x00,       // offset 0x07
        0x4c, 0x89, 0x3d, 0x00, 0x00, 0x00, 0x00,       // offset 0x0e
        END);

    assert_relocations(&section_rela_text,
        R_X86_64_PC32, first_symbol_index, 0x00 + 0x03, -0x42 - 4,
        R_X86_64_PC32, first_symbol_index, 0x07 + 0x03,  0x00 - 4,
        R_X86_64_PC32, first_symbol_index, 0x0e + 0x03,  0x42 - 4,
        END
    );
}

void test_relocations_with_rip_and_defined_symbol(void) {
    char *input =
        "mov %r15, foo - 1(%rip)\n"
        "mov %r15, foo + 0(%rip)\n"
        "mov %r15, foo + 1(%rip)\n"
        "foo: nop\n";

    test_full_assembly("relocations_with_rip_and_defined_symbol", input,
        0x4c, 0x89, 0x3d, 0x0d, 0x00, 0x00, 0x00,       // offset 0x00
        0x4c, 0x89, 0x3d, 0x07, 0x00, 0x00, 0x00,       // offset 0x07
        0x4c, 0x89, 0x3d, 0x01, 0x00, 0x00, 0x00,       // offset 0x0e
        0x90,                                           // offset 0x00
        END);

    // There should be no relocations
    assert_relocations(&section_rela_text, END);
}

// Symbols that are defined and local get resolved without a relocation
void test_local_defined_symbol_relocation(void) {
    char *input =
        ".text\n"
        "callq bar\n"
        "callq bar\n"
        "bar: nop";

    test_full_assembly("test_local_defined_symbol_relocation", input,
        0xe8, 0x05, 0x00, 0x00, 0x00, // callq bar
        0xe8, 0x00, 0x00, 0x00, 0x00, // callq bar
        0x90,                         // nop
        END
    );

    assert_relocations(&section_rela_text, END); // No relocations
}

// Symbols that are defined and global get a relocation pointing at the symbol.
void test_global_defined_symbol_relocation(void) {
    char *input =
        ".text\n"
        ".globl bar\n"
        "callq bar\n"
        "bar: nop";

    test_full_assembly("test_global_defined_symbol_relocation", input,
        0xe8, 0x00, 0x00, 0x00, 0x00, // callq bar
        0x90,                         // nop
        END
    );

    assert_relocations(&section_rela_text,
        R_X86_64_PLT32, first_symbol_index, 0x01, -4,
        END
    );
}

// Test data referencing an undefined symbol
// An undefined symbol is added at position first_symbol_index. The relocation
// table points to it.
void test_data_with_undefined_symbol(void) {
    char *input;

    // Byte
    input = ".data\n.byte a\n.byte a + 1\n.byte a - 1\n.byte 1";

    test_full_assembly("data_with_undefined_symbol byte", input, END); // No code

    assert_relocations(&section_rela_data,
        R_X86_64_8, first_symbol_index, 0x00,  0,
        R_X86_64_8, first_symbol_index, 0x01,  1,
        R_X86_64_8, first_symbol_index, 0x02, -1,
        END
    );

    assert_section(&section_data,
        0x00, // Relocated
        0x00, // Relocated
        0x00, // Relocated
        0x01, // Direct
        END);

    // Word
    input = ".data\n.word a\n.word a + 1\n.word a - 1\n.word 1";

    test_full_assembly("data_with_undefined_symbol word", input, END); // No code

    assert_relocations(&section_rela_data,
        R_X86_64_16, first_symbol_index, 0x00,  0,
        R_X86_64_16, first_symbol_index, 0x02,  1,
        R_X86_64_16, first_symbol_index, 0x04, -1,
        END
    );

    assert_section(&section_data,
        0x00, 0x00, // Relocated
        0x00, 0x00, // Relocated
        0x00, 0x00, // Relocated
        0x01, 0x00, // Direct
        END);

    // Long
    input = ".data\n.long a\n.long a + 1\n.long a - 1\n.long 1";

    test_full_assembly("data_with_undefined_symbol long", input, END); // No code

    assert_relocations(&section_rela_data,
        R_X86_64_32, first_symbol_index, 0x00,  0,
        R_X86_64_32, first_symbol_index, 0x04,  1,
        R_X86_64_32, first_symbol_index, 0x08, -1,
        END
    );

    assert_section(&section_data,
        0x00, 0x00, 0x00, 0x00, // Relocated
        0x00, 0x00, 0x00, 0x00, // Relocated
        0x00, 0x00, 0x00, 0x00, // Relocated
        0x01, 0x00, 0x00, 0x00, // Direct
        END);

    // Quad
    input = ".data\n.quad a\n.quad a + 1\n.quad a - 1\n.quad 1";

    test_full_assembly("data_with_undefined_symbol quad", input, END); // No code

    assert_relocations(&section_rela_data,
        R_X86_64_64, first_symbol_index, 0x00,  0,
        R_X86_64_64, first_symbol_index, 0x08,  1,
        R_X86_64_64, first_symbol_index, 0x10, -1,
        END
    );

    assert_section(&section_data,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // Relocated
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // Relocated
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // Relocated
        0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // Direct
        END);

    // Quad in .rodata
    input = ".section .rodata\n.quad a\n.quad a + 1\n.quad a - 1\n.quad 1";

    test_full_assembly("data_with_undefined_symbol quad in .rodata", input, END); // No code

    assert_relocations(&section_rela_rodata,
        R_X86_64_64, first_symbol_index, 0x00,  0,
        R_X86_64_64, first_symbol_index, 0x08,  1,
        R_X86_64_64, first_symbol_index, 0x10, -1,
        END
    );

    assert_section(&section_rodata,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // Relocated
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // Relocated
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // Relocated
        0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // Direct
        END);

    // Quad in .text
    input = ".text\n.quad a\n.quad a + 1\n.quad a - 1\n.quad 1";

    test_full_assembly("data_with_undefined_symbol quad in .text", input,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // Relocated
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // Relocated
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // Relocated
        0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // Direct
        END); // No code

    assert_relocations(&section_rela_text,
        R_X86_64_64, first_symbol_index, 0x00,  0,
        R_X86_64_64, first_symbol_index, 0x08,  1,
        R_X86_64_64, first_symbol_index, 0x10, -1,
        END
    );
}

void test_data_with_defined_symbol(void) {
    char *input;

    // Byte
    input = ".data\na: .byte -1\n.byte a\n.byte a + 1\n.byte a - 1\n.byte 1";

    test_full_assembly("data_with_defined_symbol byte", input, END); // No code

    assert_relocations(&section_rela_data,
        R_X86_64_8, section_data.index, 0x01,  0,
        R_X86_64_8, section_data.index, 0x02,  1,
        R_X86_64_8, section_data.index, 0x03, -1,
        END
    );

    assert_section(&section_data,
        0xff, // a
        0x00, // Relocated
        0x00, // Relocated
        0x00, // Relocated
        0x01, // Direct
        END);

    // Word
    input = ".data\na: .word -1\n.word a\n.word a + 1\n.word a - 1\n.word 1";

    test_full_assembly("data_with_defined_symbol word", input, END); // No code

    assert_relocations(&section_rela_data,
        R_X86_64_16, section_data.index, 0x02,  0,
        R_X86_64_16, section_data.index, 0x04,  1,
        R_X86_64_16, section_data.index, 0x06, -1,
        END
    );

    assert_section(&section_data,
        0xff, 0xff, // a
        0x00, 0x00, // Relocated
        0x00, 0x00, // Relocated
        0x00, 0x00, // Relocated
        0x01, 0x00, // Direct
        END);

    // Long
    input = ".data\na: .long -1\n.long a\n.long a + 1\n.long a - 1\n.long 1";

    test_full_assembly("data_with_defined_symbol long", input, END); // No code

    assert_relocations(&section_rela_data,
        R_X86_64_32, section_data.index, 0x04,  0,
        R_X86_64_32, section_data.index, 0x08,  1,
        R_X86_64_32, section_data.index, 0x0c, -1,
        END
    );

    assert_section(&section_data,
        0xff, 0xff, 0xff, 0xff, // a
        0x00, 0x00, 0x00, 0x00, // Relocated
        0x00, 0x00, 0x00, 0x00, // Relocated
        0x00, 0x00, 0x00, 0x00, // Relocated
        0x01, 0x00, 0x00, 0x00, // Direct
        END);

    // Quad
    input = ".data\na: .quad -1\n.quad a\n.quad a + 1\n.quad a - 1\n.quad 1";

    test_full_assembly("data_with_defined_symbol quad", input, END); // No code

    assert_relocations(&section_rela_data,
        R_X86_64_64, section_data.index, 0x08,  0,
        R_X86_64_64, section_data.index, 0x10,  1,
        R_X86_64_64, section_data.index, 0x18, -1,
        END
    );

    assert_section(&section_data,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, // a
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // Relocated
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // Relocated
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // Relocated
        0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // Direct
        END);

    // Quad in .rodata
    input = ".section .rodata\na: .quad -1\n.quad a\n.quad a + 1\n.quad a - 1\n.quad 1";

    test_full_assembly("data_with_defined_symbol quad in .rodata", input, END); // No code

    assert_relocations(&section_rela_rodata,
        R_X86_64_64, section_rodata.index, 0x08,  0,
        R_X86_64_64, section_rodata.index, 0x10,  1,
        R_X86_64_64, section_rodata.index, 0x18, -1,
        END
    );

    assert_section(&section_rodata,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, // a
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // Relocated
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // Relocated
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // Relocated
        0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // Direct
        END);

    // Quad in .text
    input = ".text\na: .quad -1\n.quad a\n.quad a + 1\n.quad a - 1\n.quad 1";

    test_full_assembly("data_with_defined_symbol quad in .text", input,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, // a
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // Relocated
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // Relocated
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // Relocated
        0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // Direct
        END);

    assert_relocations(&section_rela_text,
        R_X86_64_64, section_text.index, 0x08,  0,
        R_X86_64_64, section_text.index, 0x10,  1,
        R_X86_64_64, section_text.index, 0x18, -1,
        END
    );
}

void test_symbol_types_and_binding(void) {
    int text_index = section_text.index;
    int data_index = section_data.index;

    test_full_assembly("default symbol type is NOTYPE", "foo: nop", 0x90, END);
    assert_symbols(0, 0, STT_NOTYPE, STB_LOCAL, text_index, "foo", END);

    test_full_assembly("default symbol with .L type is not in the symbol table", ".Lfoo: nop", 0x90, END);
    assert_symbols(END);

    test_full_assembly("declaring symbol as @object", ".data; .type data_sym, @object", END);
    assert_symbols(0, 0, STT_OBJECT, STB_GLOBAL, SHN_UNDEF, "data_sym", END);

    test_full_assembly("declaring symbol as @function", ".data; .type func_sym, @function", END);
    assert_symbols(0, 0, STT_FUNC, STB_GLOBAL, SHN_UNDEF, "func_sym", END);

    test_full_assembly("an undefined symbol is global", ".data; .quad undef", END);
    assert_symbols(0, 0, STT_NOTYPE, STB_GLOBAL, SHN_UNDEF, "undef", END);

    test_full_assembly("a defined symbol is local", ".data; .quad undef; undef: .byte 1", END);
    assert_symbols(8, 0, STT_NOTYPE, STB_LOCAL, data_index, "undef", END);

    test_full_assembly("defined and declared .globl", ".data; .quad def; def: .byte 1; .globl def", END);
    assert_symbols(8, 0, STT_NOTYPE, STB_GLOBAL, data_index, "def", END);

    test_full_assembly("an undefined symbol even with with .local is still global", ".data; .quad undef; .local undef", END);
    assert_symbols(0, 0, STT_NOTYPE, STB_GLOBAL, SHN_UNDEF, "undef", END);

    test_full_assembly("global symbols offset are ok",
        ".text\n"
        ".globl foo\n"
        ".globl bar\n"
        "foo: nop\n"
        "bar: nop\n",
        0x90, 0x90, END);
    assert_symbols(
        0, 0, STT_NOTYPE, STB_GLOBAL, text_index, "foo",
        1, 0, STT_NOTYPE, STB_GLOBAL, text_index, "bar",
        END);

    test_full_assembly("a .comm symbol", ".comm foo, 8, 16", END);
    assert_symbols(16, 8, STT_OBJECT, STB_GLOBAL, SHN_COMMON, "foo", END);
}

static void test_size_with_number(void) {
    test_full_assembly(".size 10", ".size foo, 10\n", END);
    assert_symbols(0, 10, STT_NOTYPE, STB_GLOBAL, SHN_UNDEF, "foo", END);
}

static void test_size_difference(void) {
    int text_index = section_text.index;

    test_full_assembly(".size obj, bar - foo",
        ".size obj, bar - foo\n"
        ".text\n"
        "foo: nop\n"
        "bar: nop\n",
        0x90, 0x90, END);

    assert_symbols(
        0, 0, STT_NOTYPE, STB_LOCAL,  text_index, "foo",
        1, 0, STT_NOTYPE, STB_LOCAL,  text_index, "bar",
        0, 1, STT_NOTYPE, STB_GLOBAL, SHN_UNDEF,  "obj", // Size of nop instruction
        END);
}

int main() {
    init_tests();

    test_parse_instruction_statement();
    test_reduce_branch_instructions();
    test_relocations_with_rip_and_undefined_symbol();
    test_relocations_with_rip_and_defined_symbol();
    test_local_defined_symbol_relocation();
    test_global_defined_symbol_relocation();
    test_data_with_undefined_symbol();
    test_data_with_defined_symbol();
    test_symbol_types_and_binding();
    test_size_with_number();
    test_size_difference();
}

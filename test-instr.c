#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "dwarf.h"
#include "elf.h"
#include "lexer.h"
#include "instr.h"
#include "parser.h"
#include "relocations.h"
#include "symbols.h"
#include "utils.h"
#include "test-utils.h"

#define DWARF_PROLOGUE 0x00, 0x09, 0x02, 0, 0, 0, 0, 0, 0, 0, 0
#define DWARF_EPILOGUE 0x00, 0x01, 0x01

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
    init_dwarf();
    Chunk *c = parse_instruction_statement();
    Instructions *instr = c->coc.primary;
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

    test_assembly("movb     %bl,                        (%rax)",        0x88, 0x18, END);
    test_assembly("movw     %bx,                        (%rax)",  0x66, 0x89, 0x18, END);
    test_assembly("movl     %ebx,                       (%rax)",        0x89, 0x18, END);
    test_assembly("movq     %rbx,                       0(%rax)", 0x48, 0x89, 0x18, END);

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

    test_assembly("shr    %bl" ,                                      0xd0, 0xeb,       END);
    test_assembly("shl    %bl" ,                                      0xd0, 0xe3,       END);
    test_assembly("shr    %bx" ,                                0x66, 0xd1, 0xeb,       END);
    test_assembly("shl    %bx" ,                                0x66, 0xd1, 0xe3,       END);
    test_assembly("shr    %ebx",                                      0xd1, 0xeb,       END);
    test_assembly("shl    %ebx",                                      0xd1, 0xe3,       END);
    test_assembly("shr    %rbx",                                0x48, 0xd1, 0xeb,       END);
    test_assembly("shl    %rbx",                                0x48, 0xd1, 0xe3,       END);
    test_assembly("shr    $0x2,                         %bl",         0xc0, 0xeb, 0x02, END);
    test_assembly("shl    $0x2,                         %bl",         0xc0, 0xe3, 0x02, END);
    test_assembly("shr    $0x2,                         %bx",   0x66, 0xc1, 0xeb, 0x02, END);
    test_assembly("shl    $0x2,                         %bx",   0x66, 0xc1, 0xe3, 0x02, END);
    test_assembly("shr    $0x2,                         %ebx",        0xc1, 0xeb, 0x02, END);
    test_assembly("shl    $0x2,                         %ebx",        0xc1, 0xe3, 0x02, END);
    test_assembly("shr    $0x2,                         %rbx",  0x48, 0xc1, 0xeb, 0x02, END);
    test_assembly("shl    $0x2,                         %rbx",  0x48, 0xc1, 0xe3, 0x02, END);

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
    test_assembly("cvtsi2sdl %eax,                      %xmm0", 0xf2, 0x0f, 0x2a, 0xc0, END);           // Convert DW Integer to Scalar Double-FP Value
    test_assembly("cvtsi2sdq %rbx,                      %xmm0", 0xf2, 0x48, 0x0f, 0x2a, 0xc3, END);     // Convert DW Integer to Scalar Double-FP Value

    test_assembly("movd     %r15d,                      %xmm0", 0x66, 0x41, 0x0f, 0x6e, 0xc7, END);     // Move Doubleword
    test_assembly("movq     %r15,                       %xmm0", 0x66, 0x49, 0x0f, 0x6e, 0xc7, END);     // Move Quadword
    test_assembly("movd     %xmm0,                      %r15d", 0x66, 0x41, 0x0f, 0x7e, 0xc7, END);     // Move Doubleword
    test_assembly("movq     %xmm0,                      %r15",  0x66, 0x49, 0x0f, 0x7e, 0xc7, END);     // Move Quadword

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

    test_assembly("push   $0x7f",       0x6a, 0x7f,                   END);
    test_assembly("push   $0x80",       0x68, 0x80, 0x00, 0x00, 0x00, END);
    test_assembly("push   $0x7fff",     0x68, 0xff, 0x7f, 0x00, 0x00, END);
    test_assembly("push   $0x8000",     0x68, 0x00, 0x80, 0x00, 0x00, END);
    test_assembly("push   $0x7fffffff", 0x68, 0xff, 0xff, 0xff, 0x7f, END);
    test_assembly("pushq  $0x7f",       0x6a, 0x7f,                   END);
    test_assembly("pushq  $0x80",       0x68, 0x80, 0x00, 0x00, 0x00, END);
    test_assembly("pushq  $0x7fff",     0x68, 0xff, 0x7f, 0x00, 0x00, END);
    test_assembly("pushq  $0x8000",     0x68, 0x00, 0x80, 0x00, 0x00, END);
    test_assembly("pushq  $0x7fffffff", 0x68, 0xff, 0xff, 0xff, 0x7f, END);

    test_assembly("push     %rax", 0x50, END);
    test_assembly("push     %rbx", 0x53, END);
    test_assembly("push     %r15", 0x41, 0x57, END);
    test_assembly("pop      %rax", 0x58, END);
    test_assembly("pop      %rbx", 0x5b, END);
    test_assembly("pop      %r15", 0x41, 0x5f, END);

    test_assembly("callq    foo@PLT",       0xe8, 00, 00, 00, 00, END);
    test_assembly("callq    foo@GOTPCREL",  0xe8, 00, 00, 00, 00, END);
    test_assembly("callq    1",             0xe8, 00, 00, 00, 00, END);

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

    // 121 zeroes: both branches get shortened
    input =
        "nop\n"
        "jne a\n"
        "nop\n"
        "nop\n"
        "nop\n"
        "jne b\n"
        "nop\n"
        ".zero 121\n"
        "a: nop\n"
        "b: nop\n";

    test_full_assembly("reduce_branch_instructions with 121 zeros", input,
        0x90,                   // nop
        0x75, 0x7f,             // jne a
        0x90, 0x90, 0x90,       // nop * 3
        0x75, 0x7b,             // jne b
        0x90,                   // nop
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 121 zeroes
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0,
        0x90,                   // a: nop
        0x90,                   // b: nop
        END);

    // 122 zeroes: only the second branch gets shortened
    input =
        "nop\n"
        "jne a\n"
        "nop\n"
        "nop\n"
        "nop\n"
        "jne b\n"
        "nop\n"
        ".zero 122\n"
        "a: nop\n"
        "b: nop\n";

    test_full_assembly("reduce_branch_instructions with 122 zeros", input,
        0x90,                                   // nop
        0x0f, 0x85, 0x80, 0x00, 0x00, 0x00,     // jne a
        0x90, 0x90, 0x90,                       // nop * 3
        0x75, 0x7c,                             // jne b
        0x90,                                   // nop
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 122 zeroes
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0,
        0x90,                   // a: nop
        0x90,                   // b: nop
        END);

    // 126 zeroes: no  branches are shortened
    input =
        "nop\n"
        "jne a\n"
        "nop\n"
        "nop\n"
        "nop\n"
        "jne b\n"
        "nop\n"
        ".zero 126\n"
        "a: nop\n"
        "b: nop\n";

    test_full_assembly("reduce_branch_instructions with 126 zeros", input,
        0x90,                                   // nop
        0x0f, 0x85, 0x88, 0x00, 0x00, 0x00,     // jne a
        0x90, 0x90, 0x90,                       // nop * 3
        0x0f, 0x85, 0x80, 0x00, 0x00, 0x00,     // jne a
        0x90,                                   // nop
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 126 zeroes
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0,
        0x90,                   // a: nop
        0x90,                   // b: nop
        END);
}

void test_relocations_with_imm_rip_and_undefined_symbol(void) {
    char *input = "movl $0x42, foo(%rip)";

    test_full_assembly("test_relocations_with_call", input,
        0xc7, 0x05,              // opcode and mod/rm
        0x00, 0x00, 0x00, 0x00,  // displacement
        0x42, 0x00, 0x00, 0x00,  // immediate
        END);

    // The offset is 2 and instruction size 10.
    // The relocation has to have an addend of -(10 - 2) = -8
    assert_relocations(".rela.text",
        R_X86_64_PC32, get_symbol_symtab_index("foo"), 0x02, 0x00 - 8,
        END
    );
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

    assert_relocations(".rela.text",
        R_X86_64_PC32, get_symbol_symtab_index("foo"), 0x00 + 0x03, -0x42 - 4,
        R_X86_64_PC32, get_symbol_symtab_index("foo"), 0x07 + 0x03,  0x00 - 4,
        R_X86_64_PC32, get_symbol_symtab_index("foo"), 0x0e + 0x03,  0x42 - 4,
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
    if (get_section(".rela.text")) panic("Unexpectedly got a .rela.text section");
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

    // There should be no relocations
    if (get_section(".rela.text")) panic("Unexpectedly got a .rela.text section");
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

    assert_relocations(".rela.text",
        R_X86_64_PLT32, get_symbol_symtab_index("bar"), 0x01, -4,
        END
    );
}

// Test data referencing an undefined symbol An undefined symbol is added. The
// relocation table points to it.
void test_data_with_undefined_symbol(void) {
    char *input;

    // Byte
    input = ".data\n.byte a\n.byte a + 1\n.byte a - 1\n.byte 1";

    test_full_assembly("data_with_undefined_symbol byte", input, END); // No code

    assert_relocations(".rela.data",
        R_X86_64_8, get_symbol_symtab_index("a"), 0x00,  0,
        R_X86_64_8, get_symbol_symtab_index("a"), 0x01,  1,
        R_X86_64_8, get_symbol_symtab_index("a"), 0x02, -1,
        END
    );

    assert_section_data(section_data,
        0x00, // Relocated
        0x00, // Relocated
        0x00, // Relocated
        0x01, // Direct
        END);

    // Word
    input = ".data\n.word a\n.word a + 1\n.word a - 1\n.word 1";

    test_full_assembly("data_with_undefined_symbol word", input, END); // No code

    assert_relocations(".rela.data",
        R_X86_64_16, get_symbol_symtab_index("a"), 0x00,  0,
        R_X86_64_16, get_symbol_symtab_index("a"), 0x02,  1,
        R_X86_64_16, get_symbol_symtab_index("a"), 0x04, -1,
        END
    );

    assert_section_data(section_data,
        0x00, 0x00, // Relocated
        0x00, 0x00, // Relocated
        0x00, 0x00, // Relocated
        0x01, 0x00, // Direct
        END);

    // Long
    input = ".data\n.long a\n.long a + 1\n.long a - 1\n.long 1";

    test_full_assembly("data_with_undefined_symbol long", input, END); // No code

    assert_relocations(".rela.data",
        R_X86_64_32, get_symbol_symtab_index("a"), 0x00,  0,
        R_X86_64_32, get_symbol_symtab_index("a"), 0x04,  1,
        R_X86_64_32, get_symbol_symtab_index("a"), 0x08, -1,
        END
    );

    assert_section_data(section_data,
        0x00, 0x00, 0x00, 0x00, // Relocated
        0x00, 0x00, 0x00, 0x00, // Relocated
        0x00, 0x00, 0x00, 0x00, // Relocated
        0x01, 0x00, 0x00, 0x00, // Direct
        END);

    // Quad
    input = ".data\n.quad a\n.quad a + 1\n.quad a - 1\n.quad 1";

    test_full_assembly("data_with_undefined_symbol quad", input, END); // No code

    assert_relocations(".rela.data",
        R_X86_64_64, get_symbol_symtab_index("a"), 0x00,  0,
        R_X86_64_64, get_symbol_symtab_index("a"), 0x08,  1,
        R_X86_64_64, get_symbol_symtab_index("a"), 0x10, -1,
        END
    );

    assert_section_data(section_data,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // Relocated
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // Relocated
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // Relocated
        0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // Direct
        END);

    // Quad in .rodata
    input = ".section .rodata\n.quad a\n.quad a + 1\n.quad a - 1\n.quad 1";

    test_full_assembly("data_with_undefined_symbol quad in .rodata", input, END); // No code

    assert_relocations(".rela.rodata",
        R_X86_64_64, get_symbol_symtab_index("a"), 0x00,  0,
        R_X86_64_64, get_symbol_symtab_index("a"), 0x08,  1,
        R_X86_64_64, get_symbol_symtab_index("a"), 0x10, -1,
        END
    );

    assert_section_data(section_rodata,
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

    assert_relocations(".rela.text",
        R_X86_64_64, get_symbol_symtab_index("a"), 0x00,  0,
        R_X86_64_64, get_symbol_symtab_index("a"), 0x08,  1,
        R_X86_64_64, get_symbol_symtab_index("a"), 0x10, -1,
        END
    );
}

void test_data_with_defined_symbol(void) {
    char *input;

    // Byte
    input = ".data\na: .byte -1\n.byte a\n.byte a + 1\n.byte a - 1\n.byte 1";

    test_full_assembly("data_with_defined_symbol byte", input, END); // No code

    assert_relocations(".rela.data",
        R_X86_64_8, section_data->symtab_index, 0x01,  0,
        R_X86_64_8, section_data->symtab_index, 0x02,  1,
        R_X86_64_8, section_data->symtab_index, 0x03, -1,
        END
    );

    assert_section_data(section_data,
        0xff, // a
        0x00, // Relocated
        0x00, // Relocated
        0x00, // Relocated
        0x01, // Direct
        END);

    // Word
    input = ".data\na: .word -1\n.word a\n.word a + 1\n.word a - 1\n.word 1";

    test_full_assembly("data_with_defined_symbol word", input, END); // No code

    assert_relocations(".rela.data",
        R_X86_64_16, section_data->symtab_index, 0x02,  0,
        R_X86_64_16, section_data->symtab_index, 0x04,  1,
        R_X86_64_16, section_data->symtab_index, 0x06, -1,
        END
    );

    assert_section_data(section_data,
        0xff, 0xff, // a
        0x00, 0x00, // Relocated
        0x00, 0x00, // Relocated
        0x00, 0x00, // Relocated
        0x01, 0x00, // Direct
        END);

    // Long
    input = ".data\na: .long -1\n.long a\n.long a + 1\n.long a - 1\n.long 1";

    test_full_assembly("data_with_defined_symbol long", input, END); // No code

    assert_relocations(".rela.data",
        R_X86_64_32, section_data->symtab_index, 0x04,  0,
        R_X86_64_32, section_data->symtab_index, 0x08,  1,
        R_X86_64_32, section_data->symtab_index, 0x0c, -1,
        END
    );

    assert_section_data(section_data,
        0xff, 0xff, 0xff, 0xff, // a
        0x00, 0x00, 0x00, 0x00, // Relocated
        0x00, 0x00, 0x00, 0x00, // Relocated
        0x00, 0x00, 0x00, 0x00, // Relocated
        0x01, 0x00, 0x00, 0x00, // Direct
        END);

    // Quad
    input = ".data\na: .quad -1\n.quad a\n.quad a + 1\n.quad a - 1\n.quad 1";

    test_full_assembly("data_with_defined_symbol quad", input, END); // No code

    assert_relocations(".rela.data",
        R_X86_64_64, section_data->symtab_index, 0x08,  0,
        R_X86_64_64, section_data->symtab_index, 0x10,  1,
        R_X86_64_64, section_data->symtab_index, 0x18, -1,
        END
    );

    assert_section_data(section_data,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, // a
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // Relocated
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // Relocated
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // Relocated
        0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // Direct
        END);

    // Quad in .rodata
    input = ".section .rodata\na: .quad -1\n.quad a\n.quad a + 1\n.quad a - 1\n.quad 1";

    test_full_assembly("data_with_defined_symbol quad in .rodata", input, END); // No code

    assert_relocations(".rela.rodata",
        R_X86_64_64, section_rodata->symtab_index, 0x08,  0,
        R_X86_64_64, section_rodata->symtab_index, 0x10,  1,
        R_X86_64_64, section_rodata->symtab_index, 0x18, -1,
        END
    );

    assert_section_data(section_rodata,
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

    assert_relocations(".rela.text",
        R_X86_64_64, section_text->symtab_index, 0x08,  0,
        R_X86_64_64, section_text->symtab_index, 0x10,  1,
        R_X86_64_64, section_text->symtab_index, 0x18, -1,
        END
    );
}

void test_GOTPCREL_relocations(void) {
    char *input;

    input = "movq foo@GOTPCREL(%rip), %rax"; // With foo undefined

    test_full_assembly("test_GOTPCREL_relocations movq foo@GOTPCREL(%rip)", input,
        0x48, 0x8b, 0x05, 0x00, 0x00, 0x00, 0x00, END);

    assert_relocations(".rela.text",
        R_X86_64_REX_GOTP, get_symbol_symtab_index("foo"), 0x03, -4,
        END
    );

    input = "foo: nop; movq foo@GOTPCREL(%rip), %rax"; // With foo defined
    test_full_assembly("test_GOTPCREL_relocations movq foo@GOTPCREL(%rip), %rax", input,
        0x90,
        0x48, 0x8b, 0x05, 0x00, 0x00, 0x00, 0x00,
        END);

    assert_relocations(".rela.text",
        R_X86_64_REX_GOTP, get_symbol_symtab_index("foo"), 0x04, -4,
        END
    );

    input = "callq foo@GOTPCREL"; // With foo undefined
    test_full_assembly("test_GOTPCREL_relocations callq foo@GOTPCREL", input,
        0xe8, 0x00, 0x00, 0x00, 0x00,
        END);

    assert_relocations(".rela.text",
        R_X86_64_REX_GOTP, get_symbol_symtab_index("foo"), 0x01, -4,
        END
    );

    input = "foo: nop; callq foo@GOTPCREL"; // With foo defined
    test_full_assembly("test_GOTPCREL_relocations callq foo@GOTPCREL", input,
        0x90,
        0xe8, 0x00, 0x00, 0x00, 0x00,
        END);

    assert_relocations(".rela.text",
        R_X86_64_REX_GOTP, get_symbol_symtab_index("foo"), 0x02, -4,
        END
    );
}

// Ensure data is placed together with surrounding instructions instead of at the start
// of the section.
void test_zero_in_text_section(void) {
    char *input;

    // Byte
    input =
        ".text\n"
        "nop\n"
        ".zero 4\n"
        ".byte 0x42";

    test_full_assembly("test_zero_in_text_section byte", input, 0x90, 0x00, 0x00, 0x00, 0x00, 0x42, END);
}

void test_symbol_types_and_binding(void) {
    int text_index = section_text->index;
    int data_index = section_data->index;
    int bss_index  = section_bss->index;

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

    // A .local symbol
    test_full_assembly("a .local symbol", ".local foo", END);
    assert_symbols(0, 0, STT_NOTYPE, STB_GLOBAL, SHN_UNDEF, "foo", END);

    // A .comm symbol
    test_full_assembly("a .comm symbol", ".comm foo, 8, 16", END);
    assert_symbols(16, 8, STT_OBJECT, STB_GLOBAL, SHN_COMMON, "foo", END);

    // A .comm symbol followed by a .local; the .local doesn't change anything
    test_full_assembly("a .comm symbol", ".comm foo, 8, 16; .local foo", END);
    assert_symbols(16, 8, STT_OBJECT, STB_GLOBAL, SHN_COMMON, "foo", END);

    // Two.local symbols followed by a .comm; they get allocated in the bss section
    test_full_assembly("three .local symbols followed by .comm",
        ".local foo1; .comm foo1, 8, 16;"
        ".local foo2; .comm foo2, 4, 8;"
        ".local foo3; .comm foo3, 4, 8",
        END);
    assert_symbols(
        8,  4, STT_OBJECT, STB_LOCAL, bss_index, "foo2",
        0,  8, STT_OBJECT, STB_LOCAL, bss_index, "foo1", // The odd order is due to the strmap_iterator
        12, 4, STT_OBJECT, STB_LOCAL, bss_index, "foo3",
        END);

    // A .local followed by a .globl
    test_full_assembly("a .local followed by a .globl", ".local foo; .globl foo", END);
    assert_symbols(0, 0, STT_NOTYPE, STB_GLOBAL, SHN_UNDEF, "foo", END);

    // A .globl followed by a .local
    test_full_assembly("a .globl followed by a .local", ".globl foo; .local foo", END);
    assert_symbols(0, 0, STT_NOTYPE, STB_GLOBAL, SHN_UNDEF, "foo", END);
}

static void test_size_with_number(void) {
    test_full_assembly(".size 10", ".size foo, 10\n", END);
    assert_symbols(0, 10, STT_NOTYPE, STB_GLOBAL, SHN_UNDEF, "foo", END);
}

static void test_size_difference(void) {
    int text_index = section_text->index;

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

    // Ensure the . symbol doesn't make it into the symbol table
    test_full_assembly("foo: nop; .size obj, . - foo", "foo: nop; .size obj, . - foo", 0x90, END);

    assert_symbols(
        0, 0, STT_NOTYPE, STB_LOCAL,  text_index, "foo",
        0, 1, STT_NOTYPE, STB_GLOBAL, SHN_UNDEF,  "obj", // Size of nop instruction
        END);
}

static void test_quad_label_difference(void) {
    test_full_assembly("test_quad_label_difference",
        ".section .data\n"
        "   .long   .Lend - .Lstart\n"
        ".Lstart:\n"
        "   .long -1\n"
        "   .quad 1\n"
        ".Lend:\n", END);

    assert_section_data(section_data,
        0x0c, 0x00, 0x00, 0x00,
        0xff, 0xff, 0xff, 0xff,
        0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        END);
}

// A convolutes test that checks all sections are laid out before
// code generation.
static void test_cross_section_quad_label_difference(void) {
    test_full_assembly("test_cross_section_quad_label_difference",
        ".text\n"
        "    .quad   .b - .a\n"
        ".data\n"
        ".a:\n"
        "    .quad -1\n"
        ".b:\n", 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, END);

    assert_section_data(section_data,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        END);
}

static void test_section_creation(void) {
    test_full_assembly(".section .foo", ".section .foo", END);
    assert_section(".foo", SHT_PROGBITS, 0);

    test_full_assembly(".section .foo, \"\"", ".section .foo, \"\"", END);
    assert_section(".foo", SHT_PROGBITS, 0);

    test_full_assembly(".section .foo, \"MS\"", ".section .foo, \"MS\"", END);
    assert_section(".foo", SHT_PROGBITS, SHF_MERGE | SHF_STRINGS);

    test_full_assembly(".section .foo, \"\", @progbits", ".section .foo, \"\", @progbits", END);
    assert_section(".foo", SHT_PROGBITS, 0);

    test_full_assembly(".section .foo, \"MS\", @progbits, 1", ".section .foo, \"MS\", @progbits, 1", END);
    assert_section(".foo", SHT_PROGBITS, SHF_MERGE | SHF_STRINGS);
}

static void test_align(void) {
    test_full_assembly(
        "ret; .align 2; ret", NULL,
        0xc3, 0x90,
        0xc3, END);

    test_full_assembly(
        "ret; .align 8; ret", NULL,
        0xc3,
        0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90,
        0xc3, END);

    test_full_assembly(
        "je foo; .align 256; foo: ret", NULL,
        0x0f, 0x84, 0xfa, 0x00, 0x00, 0x00,
        0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90,
        0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90,
        0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90,
        0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90,
        0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90,
        0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90,
        0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90,
        0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90,
        0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90,
        0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90,
        0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90,
        0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90,
        0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90,
        0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90,
        0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90,
        0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90,
        0xc3,
        END);
}

static void test_string_with_label(void) {
    int text_index = section_text->index;
    test_full_assembly("foo: .string \"foo\"", NULL, 0x66, 0x6f, 0x6f, 0x00, END);
    assert_symbols(0, 0, STT_NOTYPE, STB_LOCAL, text_index, "foo", END);
}

static void test_relocation_to_section_symbol(void) {
    // Test usage of a section symbol before and after the section has been defined
    char *input =
        ".data\n"
        "    .long .test\n"
        ".section .test\n"
        "    .long .test\n";

    test_full_assembly("test_relocation_to_section_symbol", input, END);

    assert_relocations(".rela.data", R_X86_64_32, get_symbol_symtab_index(".test"), 0, 0, END);
    assert_relocations(".rela.test", R_X86_64_32, get_symbol_symtab_index(".test"), 0, 0, END);
}

static void test_debug_line_files(void) {
    char *input;

    input =
        ".section .debug_info, \"\", @progbits\n"
        ".file       2 \"test2.c\"\n"
        ".file       3 \"a/test3.c\"\n"
        ".file       4 \"/a/test4.c\"\n"
        ".file       5 \"/a/b/test5.c\"\n"
        ".file       1 \"../a/test1.c\"\n"
        ".file       6 \"/test6.c\"\n"
        ".file       7 \"/a/test7.c\"\n";

    test_full_assembly("test_debug_line_dirs", input, ENDL);

    assert_dwarf_dirs("a", "/a", "/a/b", "../a", ENDL);

    assert_dwarf_files(
        4, "test1.c",
        0, "test2.c",
        1, "test3.c",
        2, "test4.c",
        3, "test5.c",
        0, "/test6.c",
        2, "test7.c",
        END);

    // Check entire section is OK
    assert_section_data(get_section(".debug_line"),
        // Header
        0x76, 0x00, 0x00, 0x00,             // unit_length
        0x03, 0x00,                         // DWARF version
        0x70, 0x00, 0x00, 0x00,             // Header length
        1,                                  // minimum_instruction_length
        1,                                  // default_is_stmt
        -5,                                 // line_base
        14,                                 // line_range
        13,                                 // opcode_base
        0, 1, 1, 1, 1, 0, 0, 0, 1, 0, 0, 1, // standard_opcode_lengths

        // Directory table
        0x61, 0x00,                     // a
        0x2f, 0x61, 0x00,               // /a
        0x2f, 0x61, 0x2f, 0x62, 0x00,   // /a/b
        0x2e, 0x2e, 0x2f, 0x61, 0x00,   // ../a
        0x00,                           // Terminator

        // File table
        0x74, 0x65, 0x73, 0x74, 0x31, 0x2e, 0x63, 0x00,       0x04, 0x00, 0x00, // 4, test1.c
        0x74, 0x65, 0x73, 0x74, 0x32, 0x2e, 0x63, 0x00,       0x00, 0x00, 0x00, // 0, test2.c
        0x74, 0x65, 0x73, 0x74, 0x33, 0x2e, 0x63, 0x00,       0x01, 0x00, 0x00, // 1, test3.c
        0x74, 0x65, 0x73, 0x74, 0x34, 0x2e, 0x63, 0x00,       0x02, 0x00, 0x00, // 2, test4.c
        0x74, 0x65, 0x73, 0x74, 0x35, 0x2e, 0x63, 0x00,       0x03, 0x00, 0x00, // 3, test5.c
        0x2f, 0x74, 0x65, 0x73, 0x74, 0x36, 0x2e, 0x63, 0x00, 0x00, 0x00, 0x00, // 0, /test6.c
        0x74, 0x65, 0x73, 0x74, 0x37, 0x2e, 0x63, 0x00,       0x02, 0x00, 0x00, // 2, test7.c
        0x00,                                                                   // Terminator
        END);

}

static void test_debug_line_program(void) {
    char *input;

    input =
        ".section .debug_info\n.text\n"
        ".loc 1 4\n"
        "nop\n";
    test_full_assembly("test_debug_line_dirs loc+3", input, 0x90, END);
    assert_dwarf_line_program(START,
        DWARF_PROLOGUE,
        0x15,               // Special opcode 8: advance Address by 0 to 0x0 and Line by 3 to 4
        0x20,               // Special opcode 19: advance Address by 1 to 0x1 and Line by 0 to 4
        DWARF_EPILOGUE,     // Extended opcode 1: End of Sequence
        END);

    input =
        ".section .debug_info\n.text\n"
        ".loc 1 4\n"
        "nop\n"
        ".loc 1 10\n"
        "nop\n";
    test_full_assembly("test_debug_line_dirs loc+4, loc+6", input, 0x90, 0x90, END);
    assert_dwarf_line_program(START,
        DWARF_PROLOGUE,
        0x15,               // Special opcode 8: advance Address by 0 to 0x0 and Line by 3 to 4
        0x26,               // Special opcode 25: advance Address by 1 to 0x1 and Line by 6 to 10
        0x20,               // Special opcode 19: advance Address by 1 to 0x2 and Line by 0 to 10
        DWARF_EPILOGUE,     // Extended opcode 1: End of Sequence
        END);

    // The max line increment goes from -5 to 8 (inclusive)
    input =
        ".section .debug_info\n.text\n"
        ".loc 1 100\n"
        "nop\n"
        ".loc 1 94\n"
        "nop\n";
    test_full_assembly("test_debug_line_dirs loc+99, loc-6", input, 0x90, 0x90, END);
    assert_dwarf_line_program(START,
        DWARF_PROLOGUE,
        0x03, 0xe3, 0x00,   // Advance Line by 99 to 100
        0x03, 0x7a,         // Advance Line by -6 to 94
        0x02, 0x01,         // Advance PC by 1 to 0x1
        0x20,               // Special opcode 19: advance Address by 1 to 0x2 and Line by 0 to 10
        DWARF_EPILOGUE,     // Extended opcode 1: End of Sequence
        END);

    input =
        ".section .debug_info\n.text\n"
        ".loc 1 100\n"
        "nop\n"
        ".loc 1 95\n"
        "nop\n";
    test_full_assembly("test_debug_line_dirs loc+99, loc-5", input, 0x90, 0x90, END);
    assert_dwarf_line_program(START,
        DWARF_PROLOGUE,
        0x03, 0xe3, 0x00,   // Advance Line by 99 to 100
        0x1b,               // Special opcode 14: advance Address by 1 to 0x1 and Line by -5 to 95
        0x20,               // Special opcode 19: advance Address by 1 to 0x2 and Line by 0 to 10
        DWARF_EPILOGUE,     // Extended opcode 1: End of Sequence
        END);

    input =
        ".section .debug_info\n.text\n"
        ".loc 1 100\n"
        "nop\n"
        ".loc 1 108\n"
        "nop\n";
    test_full_assembly("test_debug_line_dirs loc+99, loc+8", input, 0x90, 0x90, END);
    assert_dwarf_line_program(START,
        DWARF_PROLOGUE,
        0x03, 0xe3, 0x00,   // Advance Line by 99 to 100
        0x28,               // Special opcode 27: advance Address by 1 to 0x1 and Line by 8 to 108
        0x20,               // Special opcode 19: advance Address by 1 to 0x2 and Line by 0 to 10
        DWARF_EPILOGUE,     // Extended opcode 1: End of Sequence
        END);

    input =
        ".section .debug_info\n.text\n"
        ".loc 1 100\n"
        "nop\n"
        ".loc 1 109\n"
        "nop\n";
    test_full_assembly("test_debug_line_dirs loc+99, loc+9", input, 0x90, 0x90, END);
    assert_dwarf_line_program(START,
        DWARF_PROLOGUE,
        0x03, 0xe3, 0x00,   // Advance Line by 99 to 100
        0x03, 0x09,         // Advance Line by 9 to 109
        0x02, 0x01,         // Advance PC by 1 to 0x1
        0x20,               // Special opcode 19: advance Address by 1 to 0x2 and Line by 0 to 109
        DWARF_EPILOGUE,     // Extended opcode 1: End of Sequence
        END);

    // Test locs such that opcode 0xff is emitted
    // Test opcode=255 -> adjusted_opcode=242
    // address increment = (adjusted_opcode / line_range) = 17
    // line increment = line_base + (adjusted_opcode % line_range) = -5 + (242 % 14) = -1
    input =
        ".section .debug_info\n.text\n"
        ".loc 1 100\n"
        ".zero 17\n"
        ".loc 1 99\n"
        "nop\n";
    test_full_assembly("test_debug_line_dirs loc+99, loc-9/addr+17", input,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x90, END);
    assert_dwarf_line_program(START,
        DWARF_PROLOGUE,
        0x03, 0xe3, 0x00,   // Advance Line by 99 to 100
        0xff,               // Special opcode 242: advance Address by 17 to 0x11 and Line by -1 to 99
        0x20,               // Special opcode 19: advance Address by 1 to 0x12 and Line by 0 to 99
        DWARF_EPILOGUE,     // Extended opcode 1: End of Sequence
        END);

    // Test use of DW_LNS_const_add_pc for address deltas up to 2x the address amount of opcode 255
    // See page 101 of https://dwarfstd.org/doc/Dwarf3.pdf
    input =
        ".section .debug_info\n.text\n"
        ".loc 1 100\n"
        ".zero 30\n"
        ".loc 1 101\n"
        "nop\n";
    test_full_assembly("test_debug_line_dirs loc+99, loc+1/addr+30", input,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0x90, END);
    assert_dwarf_line_program(START,
        DWARF_PROLOGUE,
        0x03, 0xe3, 0x00,   // Advance Line by 99 to 100
        0x08,               // Advance PC by constant 17 to 0x11
        0xc9,               // Special opcode 188: advance Address by 13 to 0x1e and Line by 1 to 101
        0x20,               // Special opcode 19: advance Address by 1 to 0x1f and Line by 0 to 101
        DWARF_EPILOGUE,     // Extended opcode 1: End of Sequence
        END);

    // Test potential address double with DW_LNS_const_add_pc exceeding the opcode range
    // This would trigger an opcode 256, which isn't possible
    input =
        ".section .debug_info\n.text\n"
        ".zero 34\n"
        ".loc 1 1\n"
        "nop\n";
    test_full_assembly("test_debug_line_dirs loc+99, loc+0/addr+34", input,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0,
        0x90, END);
    assert_dwarf_line_program(START,
        DWARF_PROLOGUE,
        0x02, 0x22,         // Advance PC by 34 to 0x22
        0x20,               // Special opcode 19: advance Address by 1 to 0x1f and Line by 0 to 101
        DWARF_EPILOGUE,     // Extended opcode 1: End of Sequence
        END);

    // Test file index changing
    input =
        ".section .debug_info\n.text\n"
        ".file 1 \"test1.c\"\n"
        ".file 2 \"test2.c\"\n"
        ".loc 2 100\n"
        "nop\n"
        ".loc 1 2\n"
        "nop\n";
    test_full_assembly("test_debug_line_dirs loc+99, loc+0/addr+34", input, 0x90, 0x90, END);
    assert_dwarf_line_program(START,
        DWARF_PROLOGUE,
        0x04, 0x02,         // Set File Name to entry 2 in the File Name Table
        0x03, 0xe3, 0x00,   // Advance Line by 99 to 100
        0x04, 0x01,         // Set File Name to entry 1 in the File Name Table
        0x03, 0x9e, 0x7f,   // Advance Line by -98 to 2
        0x02, 0x01,         // Advance PC by 1 to 0x1
        0x20,               // Special opcode 19: advance Address by 1 to 0x2 and Line by 0 to 2
        DWARF_EPILOGUE,     // Extended opcode 1: End of Sequence
        END);
}

int main() {
    init_tests();

    test_parse_instruction_statement();
    test_reduce_branch_instructions();
    test_relocations_with_imm_rip_and_undefined_symbol();
    test_relocations_with_rip_and_undefined_symbol();
    test_relocations_with_rip_and_defined_symbol();
    test_local_defined_symbol_relocation();
    test_global_defined_symbol_relocation();
    test_data_with_undefined_symbol();
    test_data_with_defined_symbol();
    test_GOTPCREL_relocations();
    test_zero_in_text_section();
    test_symbol_types_and_binding();
    test_size_with_number();
    test_size_difference();
    test_quad_label_difference();
    test_cross_section_quad_label_difference();
    test_section_creation();
    test_align();
    test_string_with_label();
    test_relocation_to_section_symbol();
    test_debug_line_files();
    test_debug_line_program();
}

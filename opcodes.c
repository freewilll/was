#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "list.h"
#include "opcodes.h"
#include "strmap.h"
#include "utils.h"

// Uncomment to enable debug output
// #define DEBUG

static char *am_strings[] = { " ", "C", "D", "E", "ES", "EST", "G", "I", "J", "H", "M", "O", "R", "S", "ST", "T", "V", "W", "Z" };

static char *type_strings[] = { "  ", "b", "bs", "bss", "d", "di", "dr", "dqp", "er", "q", "qi", "sr", "ss", "sd", "v", "vds", "vq", "vqp", "vs", "w", "wi"};

StrMap *opcode_alias_map;

void print_opcode(Opcode *opcode) {
    char opcd_ext = opcode->needs_mod_rm
        ? 'r'
        : opcode->opcd_ext == -1 ? ' ' : '0' + opcode->opcd_ext;

    char direction = opcode->direction == -1 ? ' ' : opcode->direction ? 'D' : 'd';
    char op_size = opcode->op_size == -1 ? ' ' : opcode->op_size ? 'W' : 's';

    char *prefix;
    if (opcode->prefix) {
        prefix = malloc(5);
        sprintf(prefix, "0x%02x", opcode->prefix);
    }
    else
        prefix = "    ";

    char *ohf_prefix;
    if (opcode->ohf_prefix) {
        ohf_prefix = malloc(5);
        sprintf(ohf_prefix, "0x%02x", opcode->ohf_prefix);
    }
    else
        ohf_prefix = "    ";

    char *sec_opcd;
    if (opcode->sec_opcd) {
        sec_opcd = malloc(5);
        sprintf(sec_opcd, "0x%02x", opcode->sec_opcd);
    }
    else
        sec_opcd = "    ";

    printf("  %-10s  %s %s 0x%02x %s %c %c %c %s %c %s%s  %s%s  %s%s\n",
        opcode->mnem,
        prefix,
        ohf_prefix,
        opcode->primary_opcode,
        sec_opcd,
        direction,
        op_size,
        opcd_ext,
        opcode->needs_mod_rm ? "RM" : "  ",
        opcode->acc ? 'a' : ' ',

        am_strings[opcode->op1.am],
        type_strings[opcode->op1.type],
        am_strings[opcode->op2.am],
        type_strings[opcode->op2.type],
        am_strings[opcode->op3.am],
        type_strings[opcode->op3.type]
    );
}

void init_opcodes(void) {
    opcode_alias_map = new_strmap();

    for (int i = 0; i < opcode_aliases_count; i++) {
        OpcodeAlias *opcode_alias = &opcode_aliases[i];

        // Ensure opcode aliases are unique and add them to the map
        OpcodeAlias *opcode_alias_test = strmap_get(opcode_alias_map, opcode_alias->alias_mnem);
        if (opcode_alias_test) panic("Duplicate opcode alias %s", opcode_alias->alias_mnem);
        strmap_put(opcode_alias_map, opcode_alias->alias_mnem, opcode_alias);

        #ifdef DEBUG
        printf("%s\n", opcode_alias->alias_mnem);
        #endif

        opcode_alias->opcodes = new_list(16);

        for (int j = 0; j < opcode_count; j++) {
            Opcode *opcode = &opcodes[j];
            if (!strcmp(opcode->mnem, opcode_alias->mnem)) {
                append_to_list(opcode_alias->opcodes, opcode);

                #ifdef DEBUG
                print_opcode(opcode);
                #endif
            }
        }
    }
}

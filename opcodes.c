#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "list.h"
#include "opcodes.h"
#include "strmap.h"
#include "utils.h"

// Uncomment to enable debug output
// #define DEBUG

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

    printf("  %s  %s 0x%02x %c%c %c  %d  %c%s  %c%s\n",
        opcode->mnem,
        prefix,
        opcode->primary_opcode,
        direction,
        op_size,
        opcd_ext,
        opcode->needs_mod_rm,
        opcode->dst.am ? opcode->dst.am : ' ',
        opcode->dst.type,
        opcode->src.am ? opcode->src.am : ' ',
        opcode->src.type
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

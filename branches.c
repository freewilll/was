#include <stdio.h>
#include <stdlib.h>

#include "branches.h"
#include "elf.h"
#include "parser.h"
#include "utils.h"

// Based on the approach in relax_segment in GNU GAS write.c The text chunks are grouped
// into a list of frags. Each frag starts with a branch instruction and has zero or
// more other instructions after it. All branches start with the larger
// (primary) version of the branch instruction. First, all frags are made. Then, until
// no changes are made, all frags are walked over, keeping track of how much the
// running offset has been reduced. Instructions are reduced where possible and symbol
// offsets are updated.
// Typically, the process will stop after a couple of iterations. However, an upper limit
// has been added just in case it goes on forever.

// #define DEBUG

typedef struct fragment {
    int text_chunk_index;           // Index of the branch instruction in the text_chunks list
    int offset;                     // Offset of the first instruction
    int fixed_size;                 // The size of all the instructions except the first one
    int branch_targets_index;       // Index in branch_target_list of the first symbol (if any) following the branch instruction. -1 if none
    int target_symbol_is_before;    // Is the target of the branch instruction before the jump instruction
    struct fragment *prev;          // Previous frag (if any)
    struct fragment *next;          // Next frag (if any)
} Fragment;

Fragment *head;             // The first of the linked list of frags
List *branch_target_list;   // A list of all symbols, in order of declaration

// Dump all frags + symbols
void dump_frags(void) {
    printf("Frags:\n");

    for (Fragment *frag = head; frag; frag = frag->next) {
        TextChunk *tc = text_chunks->elements[frag->text_chunk_index];

        printf("%5d %06x -> %s\n",
            frag->text_chunk_index, frag->offset, tc->cdc.primary->relocation_symbol->name);

        int start = frag->branch_targets_index;
        int end = frag->next ? frag->next->branch_targets_index : branch_target_list->length;

        if (start != -1 && end != -1) {
            for (int j = start; j < end; j++) {
                Symbol *symbol = branch_target_list->elements[j];
                printf("  %06x %s\n", symbol->value, symbol->name);
            }
        }
    }
}

// Update all symbol offsets
static void make_symbol_offsets(void) {
    int offset = 0;

    for (int i = 0; i < text_chunks->length; i++) {
        TextChunk *tc = text_chunks->elements[i];

        List *symbols = tc->symbols;
        if (symbols) {
            for (int j = 0; j < symbols->length; j++) {
                Symbol *symbol = symbols->elements[j];
                symbol->section_index = section_text.index;
                symbol->value = offset;
            }
        }

        if (tc->type != CT_SIZE_EXPR) offset += TEXT_CHUNK_SIZE(tc);
    }
}

static void make_frags(void) {
    head = NULL;

    StrMap *branch_target_set = new_strmap(); // Branch targets
    StrMap *seen_symbols = new_strmap(); // Declared symbols
    char *tc_target_symbol_is_before = malloc(text_chunks->length); // For branches, is the target before or after the instruction?

    // Make branch_target_set and tc_target_symbol_is_before
    for (int i = 0; i < text_chunks->length; i++) {
        TextChunk *tc = text_chunks->elements[i];

        List *symbols = tc->symbols;
        if (symbols) {
            for (int j = 0; j < symbols->length; j++) {
                Symbol *symbol = symbols->elements[j];
                strmap_put(seen_symbols, symbol->name, (void *) 1);
            }
        }

        if (tc->type == CT_CODE && tc->cdc.secondary) {
            tc_target_symbol_is_before[i] = (char) (long) strmap_get(seen_symbols, tc->cdc.primary->relocation_symbol->name);
            strmap_put(branch_target_set, tc->cdc.primary->relocation_symbol->name, (void *) 1);
        }
    }

    free_strmap(seen_symbols);

    int offset = 0;
    Fragment *frag = NULL;
    branch_target_list = new_list(1024);

    // Make fragments: loop over all text chunks
    for (int i = 0; i < text_chunks->length; i++) {
        TextChunk *tc = text_chunks->elements[i];

        // Loop over all labels and add them to branch_target_list if they
        // are a branch target.
        List *symbols = tc->symbols;
        if (symbols) {
            for (int j = 0; j < symbols->length; j++) {
                Symbol *symbol = symbols->elements[j];

                // If the symbol is a branch target ...
                if (strmap_get(branch_target_set, symbol->name)) {
                    // Set branch_targets_index unless there is no frag yet or it's
                    // already set.
                    if (frag && frag->branch_targets_index == -1)
                        frag->branch_targets_index = branch_target_list->length;

                    append_to_list(branch_target_list, symbol);
                }
            }
        }

        // It's a branch
        if (tc->type == CT_CODE && tc->cdc.secondary) {
            // Create a new frag
            if (!frag) {
                head = calloc(1, sizeof(Fragment));
                frag = head;
            }
            else {
                frag->next = calloc(1, sizeof(Fragment));
                frag->next->prev = frag;
                frag = frag->next;
            }


            frag->text_chunk_index = i;
            frag->offset = offset;
            frag->branch_targets_index = -1; // The next instruction (if any) sets this
            frag->target_symbol_is_before = tc_target_symbol_is_before[i];

            if (frag->prev)
                frag->prev->fixed_size = offset - frag->prev->offset - TEXT_CHUNK_SIZE((TextChunk *) text_chunks->elements[frag->prev->text_chunk_index]);
        }

        if (tc->type != CT_SIZE_EXPR) offset += TEXT_CHUNK_SIZE(tc);
    }

    free_strmap(branch_target_set);
    free(tc_target_symbol_is_before);

    if (!frag) return; // Do nothing if there are No branch instructions

    // Patch up any branch_targets_index that are -1 going backwards. This happens in e.g.
    // this case:
    // jne foo          <- this frag ends up with -1 since no new labels have been defined
    // nop
    // jne foo

    int branch_targets_index = frag->branch_targets_index;
    frag = frag->prev;
    while (frag) {
        if (frag->branch_targets_index == -1)
            frag->branch_targets_index = branch_targets_index;
        else
            branch_targets_index = frag->branch_targets_index;

        frag = frag->prev;
    }

    #ifdef DEBUG
    printf("Branch target list:\n");
    for (int i = 0; i < branch_target_list->length; i++) {
        Symbol *symbol = branch_target_list->elements[i];
        printf("%5d %s\n", i, symbol->name);
    }
    #endif
}

static void reduce(void) {
    int iterations = 0;
    const int max_iterations = text_chunks->length * text_chunks->length; // Don't go further than O(n^2)

    int changed = 1;
    while (iterations < max_iterations && changed) {
        changed = 0;

        int offset = head->offset;
        int compression = 0;

        for (Fragment *frag = head; frag; frag = frag->next) {
            TextChunk *tc = text_chunks->elements[frag->text_chunk_index];

            // If it's not already been reduced ...
            if (tc->cdc.using_primary) {
                int symbol_offset = tc->cdc.primary->relocation_symbol->value;

                // Symbols in the past have had their offset set. Symbols in the
                // future are displaced backwards as the iteration goes on
                if (!frag->target_symbol_is_before) symbol_offset += compression;

                int relative_offset = symbol_offset - (offset + tc->cdc.secondary->relocation_offset + 1 + 4);

                if (relative_offset >= -128 && relative_offset <= 127) {
                    tc->cdc.using_primary = 0;
                    changed = 1;
                    compression += tc->cdc.secondary->size - tc->cdc.primary->size;
                }
            }

            // Add compression to all symbols in this fragment.
            // In the next iteration, these are considered "before", and must have
            // their offsets set.
            int start = frag->branch_targets_index;
            int end = frag->next ? frag->next->branch_targets_index : branch_target_list->length;
            if (end == -1) end = branch_target_list->length;
            if (start != -1) {
                for (int j = start; j < end; j++) {
                    Symbol *symbol = branch_target_list->elements[j];
                    symbol->value += compression;
                }
            }

            offset += frag->fixed_size + TEXT_CHUNK_SIZE(tc);
        }

        iterations++;
    }

    #ifdef DEBUG
    printf("Branch reduction with %d / %d iterations\n", iterations,  max_iterations);
    #endif
}

void reduce_branch_instructions(void) {
    make_symbol_offsets();

    if (!text_chunks->length) return;

    make_frags();

    #ifdef DEBUG
    dump_frags();
    #endif

    if (!head) return;

    reduce();
    make_symbol_offsets();
}

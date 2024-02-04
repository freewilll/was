#include <stdio.h>
#include <stdlib.h>

#include "branches.h"
#include "elf.h"
#include "list.h"
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
    int chunk_index;                // Index of the branch instruction in the chunks list
    int offset;                     // Offset of the first instruction
    int fixed_size;                 // The size of all the instructions except the first one
    int branch_targets_index;       // Index in branch_target_list of the first symbol (if any) following the branch instruction. -1 if none
    int target_symbol_is_before;    // Is the target of the branch instruction before the jump instruction
    struct fragment *prev;          // Previous frag (if any)
    struct fragment *next;          // Next frag (if any)
} Fragment;

Fragment *head;             // The first of the linked list of frags
List *branch_target_list;   // A list of all symbols, in order of declaration

#ifdef DEBUG
// Dump all frags + symbols
void dump_frags(List *chunks) {
    printf("Frags:\n");

    for (Fragment *frag = head; frag; frag = frag->next) {
        Chunk *chunk = chunks->elements[frag->chunk_index];

        if (chunk->type == CT_ALIGN)
            printf("%5d %06x align %d\n",
                frag->chunk_index, frag->offset, chunk->aic.alignment);
        else
            printf("%5d %06x -> %s\n",
                frag->chunk_index, frag->offset, chunk->cdc.primary ? chunk->cdc.primary->relocation_symbol->name: "(none)");

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
#endif

// Update all symbol offsets
static void make_symbol_offsets(Section *section, List *chunks) {
    int offset = 0;

    for (int i = 0; i < chunks->length; i++) {
        Chunk *chunk = chunks->elements[i];

        chunk->offset = offset;

        List *symbols = chunk->symbols;
        if (symbols) {
            for (int j = 0; j < symbols->length; j++) {
                Symbol *symbol = symbols->elements[j];
                symbol->section = section;
                symbol->value = offset;
            }
        }

        if (chunk->type == CT_ALIGN)
            offset += PADDING_FOR_ALIGN_UP(offset, chunk->aic.alignment);
        else if (chunk->type != CT_SIZE_EXPR)
            offset += CHUNK_SIZE(chunk);

    }
}

static void make_frags(List *chunks) {
    head = NULL;

    StrMap *branch_target_set = new_strmap(); // Branch targets
    StrMap *seen_symbols = new_strmap(); // Declared symbols
    char *chunk_target_symbol_is_before = malloc(chunks->length); // For branches, is the target before or after the instruction?

    // Make branch_target_set and chunk_target_symbol_is_before
    for (int i = 0; i < chunks->length; i++) {
        Chunk *chunk = chunks->elements[i];

        List *symbols = chunk->symbols;
        if (symbols) {
            for (int j = 0; j < symbols->length; j++) {
                Symbol *symbol = symbols->elements[j];
                strmap_put(seen_symbols, symbol->name, (void *) 1);
            }
        }

        if (chunk->type == CT_CODE && chunk->cdc.secondary) {
            chunk_target_symbol_is_before[i] = (char) (long) strmap_get(seen_symbols, chunk->cdc.primary->relocation_symbol->name);
            strmap_put(branch_target_set, chunk->cdc.primary->relocation_symbol->name, (void *) 1);
        }
    }

    free_strmap(seen_symbols);

    int offset = 0;
    Fragment *frag = NULL;
    branch_target_list = new_list(1024);

    // Make fragments: loop over all text chunks
    for (int i = 0; i < chunks->length; i++) {
        Chunk *chunk = chunks->elements[i];

        // Loop over all labels and add them to branch_target_list if they
        // are a branch target.
        List *symbols = chunk->symbols;
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

        // It's a branch or alignment; any instruction that isn't a fixed size
        if (chunk->type == CT_ALIGN || (chunk->type == CT_CODE && chunk->cdc.secondary)) {
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

            frag->chunk_index = i;
            frag->offset = offset;
            frag->branch_targets_index = -1; // The next instruction (if any) sets this
            frag->target_symbol_is_before = chunk_target_symbol_is_before[i];

            if (frag->prev)
                frag->prev->fixed_size = offset - frag->prev->offset - CHUNK_SIZE((Chunk *) chunks->elements[frag->prev->chunk_index]);
        }

        if (chunk->type != CT_SIZE_EXPR) offset += CHUNK_SIZE(chunk);
    }

    free_strmap(branch_target_set);
    free(chunk_target_symbol_is_before);

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

static void reduce(List *chunks) {
    int iterations = 0;
    const int max_iterations = chunks->length * chunks->length; // Don't go further than O(n^2)

    int changed = 1;
    while (iterations < max_iterations && changed) {
        #ifdef DEBUG
        printf("Branch reduction iteration %d\n", iterations);
        #endif

        changed = 0;

        int offset = head->offset;
        int compression = 0;

        for (Fragment *frag = head; frag; frag = frag->next) {
            Chunk *chunk = chunks->elements[frag->chunk_index];

            // If it's a branch not already been reduced ...
            if (chunk->type == CT_CODE && chunk->cdc.secondary && chunk->cdc.using_primary) {
                int symbol_offset = chunk->cdc.primary->relocation_symbol->value;

                // Symbols in the past have had their offset set. Symbols in the
                // future are displaced backwards as the iteration goes on
                if (!frag->target_symbol_is_before) symbol_offset += compression;

                int relative_offset = symbol_offset - (offset + chunk->cdc.secondary->relocation_offset + 1 + 4);

                if (relative_offset >= -128 && relative_offset <= 127) {
                    chunk->cdc.using_primary = 0;
                    changed = 1;
                    compression += chunk->cdc.secondary->size - chunk->cdc.primary->size;
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

            if (chunk->type == CT_ALIGN)
                offset += PADDING_FOR_ALIGN_UP(offset, chunk->aic.alignment);
            else
                offset += frag->fixed_size + CHUNK_SIZE(chunk);
        }

        iterations++;
    }

    #ifdef DEBUG
    printf("Branch reduction with %d / %d iterations\n", iterations,  max_iterations);
    #endif
}

void reduce_branch_instructions(Section *section, List *chunks) {
    make_symbol_offsets(section, chunks);

    if (!chunks->length) return;

    make_frags(chunks);

    #ifdef DEBUG
    dump_frags(chunks);
    #endif

    if (!head) return;

    reduce(chunks);
    make_symbol_offsets(section, chunks);
}

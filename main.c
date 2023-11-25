#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "was.h"

int main(int argc, char **argv) {
    int exit_code = 0;
    int help = 0;
    int verbose = 0;
    char *output_filename = 0;
    List *input_filenames = new_list(32);

    argc--;
    argv++;
    while (argc > 0) {
        if (*argv[0] == '-') {
                 if (argc > 0 && !strcmp(argv[0], "-h"   )) { help = 1;    argc--; argv++; }
            else if (argc > 0 && !strcmp(argv[0], "-v"   )) { verbose = 1; argc--; argv++; }
            else if (argc > 1 && !memcmp(argv[0], "-o", 2)) {
                output_filename = argv[1];
                argc -= 2;
                argv += 2;
            }
            else {
                printf("Unknown parameter %s\n", argv[0]);
                exit(1);
            }
        }
        else {
            append_to_list(input_filenames, argv[0]);
            argc--;
            argv++;
        }
    }

    if (help) {
        printf("Usage: was [-h -v] [-o OUTPUT-FILE] INPUT-FILE...\n\n");
        printf("Flags\n");
        printf("-h      Help\n");
        printf("-v      Display the programs invoked by the compiler\n");
        printf("-o      Output filename\n");
        exit(1);
    }

    if (verbose) {
        printf("Was assembler\n");
        exit(1);
    }

    if (!input_filenames->length) {
        printf("Missing input filename\n");
        exit(1);
    }

    if (!output_filename) output_filename = "a.out";

    for (int i = 0; i < input_filenames->length; i++) {
        char *input_filename = input_filenames->elements[i];
        printf("TODO process input_filename=%s\n", input_filename);
    }

    free_list(input_filenames);

    exit(exit_code);
}

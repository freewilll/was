#ifndef _BRANCHES_H
#define _BRANCHES_H

#include "elf.h"
#include "list.h"

void reduce_branch_instructions(Section *section, List *chunks);

#endif

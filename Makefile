all: was

HEADERS = \
	elf.h \
	instr.h \
	lexer.h \
	list.h \
	opcodes.h \
	parser.h \
	relocations.h \
	strmap.h \
	symbols.h \
	utils.h \
	was.h \

OBJECTS = \
	elf.o \
	instr.o \
	lexer.o \
	list.o \
	opcodes.o \
	opcodes-generated.o \
	parser.o \
	relocations.o \
	strmap.o \
	symbols.o \
	utils.o \
	was.o \

%.o: %.c ${HEADERS}
	gcc -g  -Wunused -c $< -o $@

scripts/venv:

scripts/venv:
	python3 -m virtualenv scripts/venv
	scripts/venv/bin/pip install bs4 lxml

# Uncomment the following line to regenerate opcodes-generated.c with make
# Requires a python virtualenv to be setup in scripts/venv
# opcodes-generated.c: scripts/parse-x86reference.xml.py scripts/opcodes.j2
# 	scripts/venv/bin/python3 scripts/parse-x86reference.xml.py ../x86reference.xml opcodes-generated.c

was: ${OBJECTS} main.o
	gcc -g ${OBJECTS} main.o -o was

.PHONY: test
test: was run-test-instr
	make -C tests

test-instr: ${OBJECTS} test-instr.o
	gcc -g ${OBJECTS} test-instr.o -o test-instr

.PHONY: run-test-instr
run-test-instr: test-instr
	./test-instr

clean:
	@rm -f *.o
	@rm -f was
	@rm -f venv
	@rm -f test-instr

	make -C tests clean

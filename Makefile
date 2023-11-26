all: was

HEADERS = \
	elf.h \
	lexeer.h \
	list.h \
	parser.h \
	relocations.h \
	strmap.h \
	symbols.h \
	utils.h \
	was.h \

OBJECTS = \
	elf.o \
	lexer.o \
	list.o \
	main.o \
	parser.o \
	relocations.o \
	strmap.o \
	symbols.o \
	utils.o \
	was.o \

%.o: %.c ${HEADERS}
	gcc -g  -Wunused -c $< -o $@

was: ${OBJECTS}
	gcc -g ${OBJECTS} -o was

.PHONY: test
test: was
	make -C tests

clean:
	@rm -f *.o
	@rm -f was

	make -C tests clean

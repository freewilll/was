all: was

%.o: %.c was.h elf.h
	gcc -g  -Wunused -c $< -o $@

was: main.o was.o list.o utils.o lexer.o parser.o elf.o
	gcc main.o was.o list.o utils.o lexer.o parser.o elf.o -o was

clean:
	@rm -f *.o
	@rm -f was

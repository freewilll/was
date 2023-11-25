all: was

%.o: %.c was.h
	gcc -g  -Wunused -c $< -o $@

was: main.o was.o list.o utils.o lexer.o
	gcc main.o was.o list.o utils.o lexer.o -o was

clean:
	@rm -f *.o
	@rm -f was

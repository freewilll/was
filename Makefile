all: was

%.o: %.c was.h
	gcc -g  -Wunused -c $< -o $@

was: main.o list.o utils.o
	gcc main.o list.o utils.o -o was

clean:
	@rm -f *.o
	@rm -f was

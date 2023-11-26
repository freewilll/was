all: test

.PHONY: test
test: hello.s
	../was hello.s -o hello.o
	gcc hello.o -o hello
	./hello | tee /tmp/hello.out
	echo "Hello world!" > /tmp/hello.out.expected
	diff /tmp/hello.out.expected /tmp/hello.out

clean:
	@rm -f *.o
	@rm -f hello

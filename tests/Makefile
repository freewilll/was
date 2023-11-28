all: test-hello

.PHONY: test-hello
test-hello: hello.s
	@../was hello.s -o hello.o
	@gcc hello.o -o hello
	@./hello > /tmp/hello.out
	@echo "Hello world!" > /tmp/hello.out.expected
	diff /tmp/hello.out.expected /tmp/hello.out

clean:
	@rm -f *.o
	@rm -f hello

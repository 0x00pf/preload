all: test.so hello

test.so: test.c delete.s
	${CC} -save-temps -shared -fPIC -o test.so test.c delete.s

hello: hello.c
	${CC} -o $@ $<

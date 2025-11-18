all: test.so

test.so: test.c hello.s
	${CC} -shared -fPIC -o test.so test.c hello.s


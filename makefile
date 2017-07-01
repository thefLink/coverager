libcovmake: libcoverager.c
	gcc -c -Wall -Werror -fpic -rdynamic -pthread libcoverager.c -Wno-unused-function
	gcc -shared -fpic -pthread -o libcoverager.so libcoverager.o

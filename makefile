libcovmake: libcoverager.c mem_map_parser.c
	gcc -c -Wall -Werror -fpic -rdynamic -pthread libcoverager.c -Wno-unused-function
	gcc -c -Wall -Werror -fpic -rdynamic -pthread mem_map_parser.c
	gcc -shared -fpic -pthread -o libcoverager.so libcoverager.o mem_map_parser.o

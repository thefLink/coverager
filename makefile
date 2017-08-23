CC=gcc
coveragermake: main.c libcoverager.c
	gcc -o coverager main.c libcoverager.c -pthread

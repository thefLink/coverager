prefix=/usr/local

covmake: main.c mem_map_parser.c cov_measurer.c
	gcc -o coverager mem_map_parser.c cov_measurer.c main.c -pthread

install:
	install -m 0755 coverager $(prefix)/bin

.PHONY: install


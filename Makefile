CFLAGS=-O2 -DDEBUG

pigz: pigz.o yarn.o
	cc -o pigz pigz.o yarn.o -lpthread -lz

pigz.o: pigz.c yarn.h

yarn.o: yarn.c yarn.h

clean:
	rm -f pigz pigz.o yarn.o

CFLAGS=-O2

pigz: pigz.o yarn.o
	cc -o pigz pigz.o yarn.o -lpthread -lz

pigz.o: pigz.c yarn.h

yarn.o: yarn.c yarn.h

dev: pigz pigzt pigzn

pigzt: pigzt.o yarnt.o
	cc -o pigzt pigzt.o yarnt.o -lpthread -lz

pigzt.o: pigz.c yarn.h
	cc -Wall -pedantic -O3 -DDEBUG -c -o pigzt.o pigz.c

yarnt.o: yarn.c yarn.h
	cc -Wall -pedantic -O3 -DDEBUG -c -o yarnt.o yarn.c

pigzn: pigzn.o
	cc -o pigzn pigzn.o -lz

pigzn.o: pigz.c
	cc -Wall -pedantic -O3 -DDEBUG -DNOTHREAD -c -o pigzn.o pigz.c

clean:
	rm -f *.o pigz pigzn pigzt

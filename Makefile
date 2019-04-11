CC=gcc
CFLAGS=-O3 -Wall -Wextra -Wno-unknown-pragmas -Wcast-qual
LDFLAGS=
# CFLAGS=-O3 -Wall -Wextra -Wno-unknown-pragmas -Wcast-qual -g -fsanitize=thread
# LDFLAGS=-g -fsanitize=thread
# CFLAGS=-O3 -Wall -Wextra -Wno-unknown-pragmas -Wcast-qual -g -fsanitize=address
# LDFLAGS=-g -fsanitize=address
LIBS=-lm -lpthread -lz
ZOPFLI=zopfli/src/zopfli/
ZOP=deflate.o blocksplitter.o tree.o lz77.o cache.o hash.o util.o squeeze.o katajainen.o symbols.o

# use gcc and gmake on Solaris

pigz: pigz.o yarn.o try.o $(ZOP)
	$(CC) $(LDFLAGS) -o pigz pigz.o yarn.o try.o $(ZOP) $(LIBS)
	ln -f pigz unpigz

pigz.o: pigz.c yarn.h try.h $(ZOPFLI)deflate.h $(ZOPFLI)util.h

yarn.o: yarn.c yarn.h

try.o: try.c try.h

deflate.o: $(ZOPFLI)deflate.c $(ZOPFLI)deflate.h $(ZOPFLI)blocksplitter.h $(ZOPFLI)lz77.h $(ZOPFLI)squeeze.h $(ZOPFLI)tree.h $(ZOPFLI)zopfli.h $(ZOPFLI)cache.h $(ZOPFLI)hash.h $(ZOPFLI)util.h $(ZOPFLI)symbols.h
	$(CC) $(CFLAGS) -c $(ZOPFLI)deflate.c

blocksplitter.o: $(ZOPFLI)blocksplitter.c $(ZOPFLI)blocksplitter.h $(ZOPFLI)deflate.h $(ZOPFLI)lz77.h $(ZOPFLI)squeeze.h $(ZOPFLI)tree.h $(ZOPFLI)util.h $(ZOPFLI)zopfli.h $(ZOPFLI)cache.h $(ZOPFLI)hash.h
	$(CC) $(CFLAGS) -c $(ZOPFLI)blocksplitter.c

tree.o: $(ZOPFLI)tree.c $(ZOPFLI)tree.h $(ZOPFLI)katajainen.h $(ZOPFLI)util.h
	$(CC) $(CFLAGS) -c $(ZOPFLI)tree.c

lz77.o: $(ZOPFLI)lz77.c $(ZOPFLI)lz77.h $(ZOPFLI)util.h $(ZOPFLI)cache.h $(ZOPFLI)hash.h $(ZOPFLI)zopfli.h $(ZOPFLI)symbols.h
	$(CC) $(CFLAGS) -c $(ZOPFLI)lz77.c

cache.o: $(ZOPFLI)cache.c $(ZOPFLI)cache.h $(ZOPFLI)util.h
	$(CC) $(CFLAGS) -c $(ZOPFLI)cache.c

hash.o: $(ZOPFLI)hash.c $(ZOPFLI)hash.h $(ZOPFLI)util.h
	$(CC) $(CFLAGS) -c $(ZOPFLI)hash.c

util.o: $(ZOPFLI)util.c $(ZOPFLI)util.h
	$(CC) $(CFLAGS) -c $(ZOPFLI)util.c

squeeze.o: $(ZOPFLI)squeeze.c $(ZOPFLI)squeeze.h $(ZOPFLI)blocksplitter.h $(ZOPFLI)deflate.h $(ZOPFLI)tree.h $(ZOPFLI)util.h $(ZOPFLI)zopfli.h $(ZOPFLI)lz77.h $(ZOPFLI)cache.h $(ZOPFLI)hash.h $(ZOPFLI)symbols.h
	$(CC) $(CFLAGS) -c $(ZOPFLI)squeeze.c

katajainen.o: $(ZOPFLI)katajainen.c $(ZOPFLI)katajainen.h
	$(CC) $(CFLAGS) -c $(ZOPFLI)katajainen.c

symbols.o: $(ZOPFLI)symbols.c $(ZOPFLI)symbols.h
	$(CC) $(CFLAGS) -c $(ZOPFLI)symbols.c

dev: pigz pigzj pigzt pigzn

pigzj: pigzj.o yarn.o try.o
	$(CC) $(LDFLAGS) -o pigzj pigzj.o yarn.o try.o $(LIBS)

pigzj.o: pigz.c yarn.h try.h
	$(CC) $(CFLAGS) -DNOZOPFLI -c -o pigzj.o pigz.c

pigzt: pigzt.o yarnt.o try.o $(ZOP)
	$(CC) $(LDFLAGS) -o pigzt pigzt.o yarnt.o try.o $(ZOP) $(LIBS)

pigzt.o: pigz.c yarn.h try.h
	$(CC) $(CFLAGS) -DPIGZ_DEBUG -g -c -o pigzt.o pigz.c

yarnt.o: yarn.c yarn.h
	$(CC) $(CFLAGS) -DPIGZ_DEBUG -g -c -o yarnt.o yarn.c

pigzn: pigzn.o tryn.o $(ZOP)
	$(CC) $(LDFLAGS) -o pigzn pigzn.o tryn.o $(ZOP) $(LIBS)

pigzn.o: pigz.c try.h
	$(CC) $(CFLAGS) -DPIGZ_DEBUG -DNOTHREAD -g -c -o pigzn.o pigz.c

tryn.o: try.c try.h
	$(CC) $(CFLAGS) -DPIGZ_DEBUG -DNOTHREAD -g -c -o tryn.o try.c

test: pigz
	./pigz -kf pigz.c ; ./pigz -t pigz.c.gz
	./pigz -kfb 32 pigz.c ; ./pigz -t pigz.c.gz
	./pigz -kfp 1 pigz.c ; ./pigz -t pigz.c.gz
	./pigz -kfz pigz.c ; ./pigz -t pigz.c.zz
	./pigz -kfK pigz.c ; ./pigz -t pigz.c.zip
	printf "" | ./pigz -cdf | wc -c | test `cat` -eq 0
	printf "x" | ./pigz -cdf | wc -c | test `cat` -eq 1
	printf "xy" | ./pigz -cdf | wc -c | test `cat` -eq 2
	printf "xyz" | ./pigz -cdf | wc -c | test `cat` -eq 3
	(printf "w" | gzip ; printf "x") | ./pigz -cdf | wc -c | test `cat` -eq 2
	(printf "w" | gzip ; printf "xy") | ./pigz -cdf | wc -c | test `cat` -eq 3
	(printf "w" | gzip ; printf "xyz") | ./pigz -cdf | wc -c | test `cat` -eq 4
	-@if test "`which compress | grep /`" != ""; then \
	  echo 'compress -f < pigz.c | ./unpigz | cmp - pigz.c' ;\
	  compress -f < pigz.c | ./unpigz | cmp - pigz.c ;\
	fi
	@rm -f pigz.c.gz pigz.c.zz pigz.c.zip

tests: dev test
	./pigzn -kf pigz.c ; ./pigz -t pigz.c.gz
	@rm -f pigz.c.gz

docs: pigz.pdf

pigz.pdf: pigz.1
	groff -mandoc -f H -T ps pigz.1 | ps2pdf - pigz.pdf

all: pigz pigzj pigzt pigzn docs

clean:
	@rm -f *.o pigz unpigz pigzj pigzn pigzt pigz.c.gz pigz.c.zz pigz.c.zip

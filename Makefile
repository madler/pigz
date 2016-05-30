CC=cc
CFLAGS=-O3 -Wall -Wextra
LDFLAGS=
LIBS=-lm -lpthread -lz
ZOPFLI=zopfli/src/zopfli/
# use gcc and gmake on Solaris

pigz: pigz.o yarn.o try.o ${ZOPFLI}deflate.o ${ZOPFLI}blocksplitter.o ${ZOPFLI}tree.o ${ZOPFLI}lz77.o ${ZOPFLI}cache.o ${ZOPFLI}hash.o ${ZOPFLI}util.o ${ZOPFLI}squeeze.o ${ZOPFLI}katajainen.o
	$(CC) $(LDFLAGS) -o pigz $^ $(LIBS)
	ln -f pigz unpigz

pigz.o: pigz.c yarn.h try.h ${ZOPFLI}deflate.h ${ZOPFLI}util.h

yarn.o: yarn.c yarn.h

try.o: try.c try.h

${ZOPFLI}deflate.o: ${ZOPFLI}deflate.c ${ZOPFLI}deflate.h ${ZOPFLI}blocksplitter.h ${ZOPFLI}lz77.h ${ZOPFLI}squeeze.h ${ZOPFLI}tree.h ${ZOPFLI}zopfli.h ${ZOPFLI}cache.h ${ZOPFLI}hash.h ${ZOPFLI}util.h

${ZOPFLI}blocksplitter.o: ${ZOPFLI}blocksplitter.c ${ZOPFLI}blocksplitter.h ${ZOPFLI}deflate.h ${ZOPFLI}lz77.h ${ZOPFLI}squeeze.h ${ZOPFLI}tree.h ${ZOPFLI}util.h ${ZOPFLI}zopfli.h ${ZOPFLI}cache.h ${ZOPFLI}hash.h

${ZOPFLI}tree.o: ${ZOPFLI}tree.c ${ZOPFLI}tree.h ${ZOPFLI}katajainen.h ${ZOPFLI}util.h

${ZOPFLI}lz77.o: ${ZOPFLI}lz77.h ${ZOPFLI}util.h ${ZOPFLI}cache.h ${ZOPFLI}hash.h ${ZOPFLI}zopfli.h

${ZOPFLI}cache.o: ${ZOPFLI}cache.c ${ZOPFLI}cache.h ${ZOPFLI}util.h

${ZOPFLI}hash.o: ${ZOPFLI}hash.c ${ZOPFLI}hash.h ${ZOPFLI}util.h

${ZOPFLI}util.o: ${ZOPFLI}util.c ${ZOPFLI}util.h

${ZOPFLI}squeeze.o: ${ZOPFLI}squeeze.c ${ZOPFLI}squeeze.h ${ZOPFLI}blocksplitter.h ${ZOPFLI}deflate.h ${ZOPFLI}tree.h ${ZOPFLI}util.h ${ZOPFLI}zopfli.h ${ZOPFLI}lz77.h ${ZOPFLI}cache.h ${ZOPFLI}hash.h

${ZOPFLI}katajainen.o: ${ZOPFLI}katajainen.c ${ZOPFLI}katajainen.h

dev: pigz pigzt pigzn

pigzt: pigzt.o yarnt.o try.o ${ZOPFLI}deflate.o ${ZOPFLI}blocksplitter.o ${ZOPFLI}tree.o ${ZOPFLI}lz77.o ${ZOPFLI}cache.o ${ZOPFLI}hash.o ${ZOPFLI}util.o ${ZOPFLI}squeeze.o ${ZOPFLI}katajainen.o
	$(CC) $(LDFLAGS) -o pigzt $^ $(LIBS)

pigzt.o: pigz.c yarn.h try.h
	$(CC) $(CFLAGS) -DDEBUG -g -c -o pigzt.o pigz.c

yarnt.o: yarn.c yarn.h
	$(CC) $(CFLAGS) -DDEBUG -g -c -o yarnt.o yarn.c

pigzn: pigzn.o tryn.o ${ZOPFLI}deflate.o ${ZOPFLI}blocksplitter.o ${ZOPFLI}tree.o ${ZOPFLI}lz77.o ${ZOPFLI}cache.o ${ZOPFLI}hash.o ${ZOPFLI}util.o ${ZOPFLI}squeeze.o ${ZOPFLI}katajainen.o
	$(CC) $(LDFLAGS) -o pigzn $^ $(LIBS)

pigzn.o: pigz.c try.h
	$(CC) $(CFLAGS) -DDEBUG -DNOTHREAD -g -c -o pigzn.o pigz.c

tryn.o: try.c try.h
	$(CC) $(CFLAGS) -DDEBUG -DNOTHREAD -g -c -o tryn.o try.c

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

clean:
	@rm -f *.o ${ZOPFLI}*.o pigz unpigz pigzn pigzt pigz.c.gz pigz.c.zz pigz.c.zip

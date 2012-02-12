CC=cc
CFLAGS=-O3 -Wall -Wextra

pigz: pigz.o yarn.o
	$(CC) -o pigz pigz.o yarn.o -lpthread -lz
	ln -f pigz unpigz

pigz.o: pigz.c yarn.h

yarn.o: yarn.c yarn.h

dev: pigz pigzt pigzn

pigzt: pigzt.o yarnt.o
	$(CC) -o pigzt pigzt.o yarnt.o -lpthread -lz

pigzt.o: pigz.c yarn.h
	$(CC) -Wall -O3 -DDEBUG -g -c -o pigzt.o pigz.c

yarnt.o: yarn.c yarn.h
	$(CC) -Wall -O3 -DDEBUG -g -c -o yarnt.o yarn.c

pigzn: pigzn.o
	$(CC) -o pigzn pigzn.o -lz

pigzn.o: pigz.c
	$(CC) -Wall -O3 -DDEBUG -DNOTHREAD -g -c -o pigzn.o pigz.c

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
	-@if test "`whereis compress | grep /`" != ""; then \
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
	@rm -f *.o pigz unpigz pigzn pigzt pigz.c.gz pigz.c.zz pigz.c.zip

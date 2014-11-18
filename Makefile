CC=cc
CFLAGS=-O3 -Wall -Wextra


pigz: pigz.o yarn.o zopfli/deflate.o zopfli/blocksplitter.o zopfli/tree.o zopfli/lz77.o zopfli/cache.o zopfli/hash.o zopfli/util.o zopfli/squeeze.o zopfli/katajainen.o
	$(CC) $(LDFLAGS) -o pigz $^ -lpthread -lz -lm
	ln -f pigz unpigz

pigz.o: pigz.c yarn.h zopfli/deflate.h zopfli/util.h

yarn.o: yarn.c yarn.h

zopfli/deflate.o: zopfli/deflate.c zopfli/deflate.h zopfli/blocksplitter.h zopfli/lz77.h zopfli/squeeze.h zopfli/tree.h zopfli/zopfli.h zopfli/cache.h zopfli/hash.h zopfli/util.h

zopfli/blocksplitter.o: zopfli/blocksplitter.c zopfli/blocksplitter.h zopfli/deflate.h zopfli/lz77.h zopfli/squeeze.h zopfli/tree.h zopfli/util.h zopfli/zopfli.h zopfli/cache.h zopfli/hash.h

zopfli/tree.o: zopfli/tree.c zopfli/tree.h zopfli/katajainen.h zopfli/util.h

zopfli/lz77.o: zopfli/lz77.h zopfli/util.h zopfli/cache.h zopfli/hash.h zopfli/zopfli.h

zopfli/cache.o: zopfli/cache.c zopfli/cache.h zopfli/util.h

zopfli/hash.o: zopfli/hash.c zopfli/hash.h zopfli/util.h

zopfli/util.o: zopfli/util.c zopfli/util.h

zopfli/squeeze.o: zopfli/squeeze.c zopfli/squeeze.h zopfli/blocksplitter.h zopfli/deflate.h zopfli/tree.h zopfli/util.h zopfli/zopfli.h zopfli/lz77.h zopfli/cache.h zopfli/hash.h

zopfli/katajainen.o: zopfli/katajainen.c zopfli/katajainen.h

dev: pigz pigzt pigzn

pigzt: pigzt.o yarnt.o zopfli/deflate.o zopfli/blocksplitter.o zopfli/tree.o zopfli/lz77.o zopfli/cache.o zopfli/hash.o zopfli/util.o zopfli/squeeze.o zopfli/katajainen.o
	$(CC) $(LDFLAGS) -o pigzt $^ -lpthread -lz -lm

pigzt.o: pigz.c yarn.h
	$(CC) $(CFLAGS) -DDEBUG -g -c -o pigzt.o pigz.c

yarnt.o: yarn.c yarn.h
	$(CC) $(CFLAGS) -DDEBUG -g -c -o yarnt.o yarn.c

pigzn: pigzn.o zopfli/deflate.o zopfli/blocksplitter.o zopfli/tree.o zopfli/lz77.o zopfli/cache.o zopfli/hash.o zopfli/util.o zopfli/squeeze.o zopfli/katajainen.o
	$(CC) $(LDFLAGS) -o pigzn $^ -lz -lm

pigzn.o: pigz.c
	$(CC) $(CFLAGS) -DDEBUG -DNOTHREAD -g -c -o pigzn.o pigz.c

#
# set up pattern rules for tests
#

LONG_NAME = VeryLongNameVeryLongNameVeryLongNameVeryLongNameVeryLongNameVeryLongNameVeryLongNameVeryLongNameVeryLongNameVeryLongNameVeryLongNameVeryLongNameVeryLongNameVeryLongNameVeryLongNameVeryLongNameVeryLongNameVeryLongName

TESTFILES =  $(addprefix .testfile-, empty pigz.c largefile $(LONG_NAME) )

.testfile-empty:
	cat /dev/null > $@

.testfile-pigz.c: pigz.c
	cp pigz.c $@

.testfile-largefile: pigz.c
	number=1 ; while [[ $$number -le 100 ]] ; do cat pigz.c >> $@ ; ((number = number + 1)) ; done

.testfile-$(LONG_NAME): .testfile-largefile
	cp $< $@

TEST_OPTIONS = .gz .b32.gz .1.gz .B.gz .B1.gz .gz2.gz .B-gz2.gz

.testfile-%.gz : .testfile-% pigz
	./pigz -kf $< && touch $@

.testfile-%.b32.gz : .testfile-% pigz
	./pigz -kfb 32 $< && mv $<.gz $@ && touch $@

.testfile-%.1.gz : .testfile-% pigz
	./pigz -kfp 1 $< && mv $<.gz $@ && touch $@

.testfile-%.B.gz : .testfile-% pigz
	./pigz -kfB $< && mv $<.gz $@ && touch $@

.testfile-%.B1.gz : .testfile-% pigz
	./pigz -kfBp 1 $< && mv $<.gz $@ && touch $@

.testfile-%.gz2.gz : .testfile-% pigz
	./pigz -kf $< && ./pigz -kf $<.gz && mv $<.gz.gz $@ && touch $@

.testfile-%.B-gz2.gz : .testfile-% pigz
	./pigz -kfB $< && ./pigz -kfB $<.gz && mv $<.gz.gz $@ && touch $@

.test% : %.gz pigz
	./pigz -t $<
	./pigz -tp 1 $<
	gzip -t $<
	./pigz -dc $< > $@.out && diff -q $@.out $*
	./pigz -dcp 1 $< > $@.out && diff -q $@.out $*
	gzip -dc $< > $@.out && diff -q $@.out $*

TESTFILES_GZ = $(foreach option, $(TEST_OPTIONS), $(addsuffix $(option), $(TESTFILES)) )

testfiles : $(TESTFILES) $(TESTFILES_GZ)

TESTS = $(addprefix .test, $(TESTFILES_GZ))

.SECONDARY : $(TESTFILES) $(TESTFILES_GZ) $(TESTS)

moretests : pigz $(TESTS)

test: pigz moretests
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
	./pigz -cp 1 < /dev/null | ./pigz -t -
	./pigz -c < /dev/null | ./pigz -t -
	./pigz -kfB -p 1 pigz.c \
	&& ./pigz -d -c -p 1 pigz.c.gz | diff -q - pigz.c \
	&& ./pigz -d -c -p 2 pigz.c.gz | diff -q - pigz.c
	./pigz -kfB pigz.c \
	&& ./pigz -d -c -p 1 pigz.c.gz | diff -q - pigz.c \
	&& ./pigz -d -c -p 2 pigz.c.gz | diff -q - pigz.c
	@rm -f pigz.c.gz pigz.c.zz pigz.c.zip .test*

tests: dev test
	./pigzn -kf pigz.c ; ./pigz -t pigz.c.gz
	@rm -f pigz.c.gz

docs: pigz.pdf

pigz.pdf: pigz.1
	groff -mandoc -f H -T ps pigz.1 | ps2pdf - pigz.pdf

clean:
	@rm -f *.o zopfli/*.o pigz unpigz pigzn pigzt pigz.c.gz pigz.c.zz pigz.c.zip .test*

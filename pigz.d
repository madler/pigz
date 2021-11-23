pigz.o: pigz.c yarn.h zopfli/src/zopfli/deflate.h \
 zopfli/src/zopfli/lz77.h zopfli/src/zopfli/cache.h \
 zopfli/src/zopfli/util.h zopfli/src/zopfli/hash.h \
 zopfli/src/zopfli/zopfli.h try.h
try.o: try.c try.h
yarn.o: yarn.c yarn.h

blocksplitter.o: zopfli/src/zopfli/blocksplitter.c \
 zopfli/src/zopfli/blocksplitter.h zopfli/src/zopfli/lz77.h \
 zopfli/src/zopfli/cache.h zopfli/src/zopfli/util.h \
 zopfli/src/zopfli/hash.h zopfli/src/zopfli/zopfli.h \
 zopfli/src/zopfli/deflate.h zopfli/src/zopfli/squeeze.h \
 zopfli/src/zopfli/tree.h
cache.o: zopfli/src/zopfli/cache.c zopfli/src/zopfli/cache.h \
 zopfli/src/zopfli/util.h
deflate.o: zopfli/src/zopfli/deflate.c zopfli/src/zopfli/deflate.h \
 zopfli/src/zopfli/lz77.h zopfli/src/zopfli/cache.h \
 zopfli/src/zopfli/util.h zopfli/src/zopfli/hash.h \
 zopfli/src/zopfli/zopfli.h zopfli/src/zopfli/blocksplitter.h \
 zopfli/src/zopfli/squeeze.h zopfli/src/zopfli/symbols.h \
 zopfli/src/zopfli/tree.h
hash.o: zopfli/src/zopfli/hash.c zopfli/src/zopfli/hash.h \
 zopfli/src/zopfli/util.h
katajainen.o: zopfli/src/zopfli/katajainen.c \
 zopfli/src/zopfli/katajainen.h
lz77.o: zopfli/src/zopfli/lz77.c zopfli/src/zopfli/lz77.h \
 zopfli/src/zopfli/cache.h zopfli/src/zopfli/util.h \
 zopfli/src/zopfli/hash.h zopfli/src/zopfli/zopfli.h \
 zopfli/src/zopfli/symbols.h
squeeze.o: zopfli/src/zopfli/squeeze.c zopfli/src/zopfli/squeeze.h \
 zopfli/src/zopfli/lz77.h zopfli/src/zopfli/cache.h \
 zopfli/src/zopfli/util.h zopfli/src/zopfli/hash.h \
 zopfli/src/zopfli/zopfli.h zopfli/src/zopfli/blocksplitter.h \
 zopfli/src/zopfli/deflate.h zopfli/src/zopfli/symbols.h \
 zopfli/src/zopfli/tree.h
symbols.o: zopfli/src/zopfli/symbols.c zopfli/src/zopfli/symbols.h
tree.o: zopfli/src/zopfli/tree.c zopfli/src/zopfli/tree.h \
 zopfli/src/zopfli/katajainen.h zopfli/src/zopfli/util.h
util.o: zopfli/src/zopfli/util.c zopfli/src/zopfli/util.h \
 zopfli/src/zopfli/zopfli.h


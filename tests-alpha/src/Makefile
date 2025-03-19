CC=cc
CFLAGS=-O5 -fno-shared

all: anagram test-printf test-fmath test-math test-llong test-lswlr test-cache

anagram: anagram.c
	$(CC) $(CFLAGS) -o anagram anagram.c

test-printf: test-printf.c
	$(CC) $(CFLAGS) -o test-printf test-printf.c -lm

test-fmath: test-fmath.c
	$(CC) $(CFLAGS) -o test-fmath test-fmath.c -lm

test-math: test-math.c
	$(CC) $(CFLAGS) -o test-math test-math.c -lm

test-llong: test-llong.c
	$(CC) $(CFLAGS) -o test-llong test-llong.c

test-lswlr: test-lswlr.c
	$(CC) $(CFLAGS) -o test-lswlr test-lswlr.c

test-cache: test-cache.c
	$(CC) $(CFLAGS) -o test-cache test-cache.c

test:	all
	../simplesim-0.1/sim-safe anagram words < input.txt
	../simplesim-0.1/sim-safe test-printf
	../simplesim-0.1/sim-safe test-fmath
	../simplesim-0.1/sim-safe test-math
	../simplesim-0.1/sim-safe test-llong
	../simplesim-0.1/sim-safe test-lswlr
	../simplesim-0.1/sim-safe test-cache

distclean:
	-make clean

clean:
	rm -f anagram test-printf test-fmath test-math test-llong test-lswlr test-cache test-as *.[oia] core *~


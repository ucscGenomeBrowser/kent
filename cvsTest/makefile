.c.o:
	gcc -ggdb -O -Wimplicit -c $*.c

L = -lm

O = test.o

test: $O $(MYLIBS)
	gcc -ggdb -o test $O  $L


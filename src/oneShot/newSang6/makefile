.c.o:
	gcc -ggdb -Wimplicit -I../../inc -c $*.c

L = $(MYSQLLIBS) -lm

O = newSang6.o

newSang6: $O ../../lib/$(MACHTYPE)/jkweb.a
	gcc -ggdb -o newSang6 $O ../../lib/$(MACHTYPE)/jkweb.a $L

lib:
	cd ../../lib; make

.c.o:
	gcc -ggdb -I../../inc -c $*.c

L = $(MYSQLLIBS) -lm 

O = killRabbit.o

killRabbit: lib $O ../../lib/$(MACHTYPE)/jkweb.a
	gcc $O ../../lib/$(MACHTYPE)/jkweb.a $L
	mv a.out killRabbit
	strip killRabbit

lib:
	cd ../../lib; make

.c.o:
	gcc -ggdb -O -I../../inc -c $*.c

O = aliStats.o 

aliStats: $(O)
	gcc -o aliStats $O ../../lib/$(MACHTYPE)/jkweb.a

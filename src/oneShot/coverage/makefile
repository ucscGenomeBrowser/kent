.c.o:
	gcc -O -ggdb -I../../inc -c $*.c

O = coverage.o 

coverage: $(O)
	gcc -o coverage $O ../../lib/$(MACHTYPE)/jkweb.a



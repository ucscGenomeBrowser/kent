.c.o:
	gcc -O -ggdb -I../../inc -c $*.c

O = splitSql.o 

splitSql: $(O)
	gcc -o $(HOME)/bin/$(MACHTYPE)/splitSql $O ../../lib/$(MACHTYPE)/jkweb.a



.c.o:
	gcc -O -ggdb -Wformat -Wimplicit -Wuninitialized -I../../inc -c $*.c

O = faOneRecord.o 

faOneRecord: $(O)
	gcc -o $(HOME)/bin/$(MACHTYPE)/faOneRecord $O ../../lib/$(MACHTYPE)/jkweb.a



.c.o:
	gcc -I../../inc -c $*.c

O = stToXao.o 

stToXao: $(O)
	gcc $O ../../lib/$(MACHTYPE)/jkweb.a
	mv a.out $(HOME)/bin/$(MACHTYPE)/stToXao

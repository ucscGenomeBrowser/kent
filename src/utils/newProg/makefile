.c.o:
	gcc -O -ggdb -Wformat -Wimplicit -Wuninitialized -Wreturn-type -I../../inc -c $*.c

O = newProg.o 

newProg: $(O)
	gcc -o $(HOME)/bin/$(MACHTYPE)/newProg $O ../../lib/$(MACHTYPE)/jkweb.a



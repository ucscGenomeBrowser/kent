.c.o:
	gcc -I../../inc -c $*.c

O = fsg.o 

$(HOME)/bin/$(MACHTYPE)/fsg: $(O)
	gcc -o $(HOME)/bin/$(MACHTYPE)/fsg $O ../../lib/$(MACHTYPE)/jkweb.a

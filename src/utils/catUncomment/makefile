.c.o:
	gcc -Wformat -Wimplicit -Wuninitialized -O -ggdb -I../../inc -c $*.c

O = catUncomment.o 

catUncomment: $(O)
	gcc -o $(HOME)/bin/$(MACHTYPE)/catUncomment $O ../../lib/$(MACHTYPE)/jkweb.a
	strip $(HOME)/bin/$(MACHTYPE)/catUncomment



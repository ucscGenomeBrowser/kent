.c.o:
	gcc -I../../inc -c $*.c

O = subChar.o 

subChar: $O
	gcc -o $(HOME)/bin/$(MACHTYPE)/subChar $O ../../lib/$(MACHTYPE)/jkweb.a 
	strip $(HOME)/bin/$(MACHTYPE)/subChar


.c.o:
	gcc -I../../inc -c $*.c

O = countChars.o 

countChars: $O
	gcc -o $(HOME)/bin/$(MACHTYPE)/countChars $O ../../lib/$(MACHTYPE)/jkweb.a 
	strip $(HOME)/bin/$(MACHTYPE)/countChars


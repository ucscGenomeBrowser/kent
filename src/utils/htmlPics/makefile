.c.o:
	gcc -I../../inc -c $*.c

O = htmlPics.o 

htmlPics: $O
	gcc -o $(HOME)/bin/$(MACHTYPE)/htmlPics $O ../../lib/$(MACHTYPE)/jkweb.a 


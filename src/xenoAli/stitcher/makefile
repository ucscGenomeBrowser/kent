.c.o:
	gcc -I../../inc -c $*.c

O = stitcher.o 

stitcher: $(O)
	gcc $O ../../lib/$(MACHTYPE)/jkOwnLib.a ../../lib/$(MACHTYPE)/jkweb.a -lm
	mv a.out $(HOME)/bin/$(MACHTYPE)/stitcher

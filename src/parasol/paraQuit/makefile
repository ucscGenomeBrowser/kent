.c.o:
	gcc -O -ggdb -I../inc -I../../inc -c $*.c

O = paraQuit.o

paraQuit: $(O)
	gcc -o $(HOME)/bin/$(MACHTYPE)/paraQuit $O ../lib/$(MACHTYPE)/paralib.a ../../lib/$(MACHTYPE)/jkweb.a
	strip $(HOME)/bin/$(MACHTYPE)/paraQuit


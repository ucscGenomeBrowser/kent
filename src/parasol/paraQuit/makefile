.c.o:
	gcc -ggdb -O -Wformat -Wimplicit -Wuninitialized -Wreturn-type -I../inc -I../../inc -I../../../inc -c $*.c

O = paraQuit.o

paraQuit: $(O)
	gcc -o $(HOME)/bin/$(MACHTYPE)/paraQuit $O ../lib/$(MACHTYPE)/paralib.a ../../lib/$(MACHTYPE)/jkweb.a
	strip $(HOME)/bin/$(MACHTYPE)/paraQuit


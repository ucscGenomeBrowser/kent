.c.o:
	gcc -I../inc -I../../inc -c $*.c

O =  cntGlue.o

$(HOME)/bin/$(MACHTYPE)/cntGlue: $(O)  
	gcc -o $(HOME)/bin/$(MACHTYPE)/cntGlue $O ../../lib/$(MACHTYPE)/jkweb.a



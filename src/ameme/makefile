.c.o:
	gcc -ggdb -O -Wformat -Wimplicit -Wuninitialized -Wreturn-type -I../inc -c $*.c

O = ameme.o fragFind.o

alpha: $(O)
	gcc $O ../lib/$(MACHTYPE)/jkweb.a -lm 
	strip a.out
	mv a.out $(HOME)/bin/$(MACHTYPE)/ameme 

cgi: $(O)
	gcc $O ../lib/$(MACHTYPE)/jkweb.a -lm
	mv a.out ameme.exe
	chmod a+rx ameme.exe
	mv ameme.exe $(HOME)/.html/cgi-bin



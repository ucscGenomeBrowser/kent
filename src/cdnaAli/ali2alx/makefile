.c.o:
	gcc -I../inc -I../../inc -c $*.c

O =  ali2alx.o ../lib/cdnaAli.o

$(HOME)/bin/$(MACHTYPE)/ali2alx: $(O)  
	gcc -o $(HOME)/bin/$(MACHTYPE)/ali2alx $O ../../lib/$(MACHTYPE)/jkweb.a



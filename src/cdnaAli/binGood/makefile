.c.o:
	gcc -I../inc -I../../inc -c $*.c

O =  binGood.o ../lib/cdnaAli.o

$(HOME)/bin/$(MACHTYPE)/binGood: $(O)  
	gcc -o $(HOME)/bin/$(MACHTYPE)/binGood $O ../../lib/$(MACHTYPE)/jkweb.a



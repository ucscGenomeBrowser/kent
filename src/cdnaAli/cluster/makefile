.c.o:
	gcc -I../inc -I../../inc -c $*.c

O =  cluster.o ../lib/cdnaAli.o

L = ../../lib/$(MACHTYPE)/jkweb.a

$(HOME)/bin/$(MACHTYPE)/cluster: $(O)  $(L)
	gcc -o $(HOME)/bin/$(MACHTYPE)/cluster $O $(L)



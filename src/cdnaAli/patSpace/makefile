.c.o:
	gcc -ggdb -O -I../inc -I../../inc -c $*.c

O =  patSpace.o

$(HOME)/bin/$(MACHTYPE)/patSpace: $(O)  
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/patSpace $O ../../lib/$(MACHTYPE)/jkweb.a



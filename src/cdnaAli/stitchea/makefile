.c.o:
	gcc -I../inc -I../../inc -c $*.c

O =  stitchea.o

$(HOME)/bin/$(MACHTYPE)/stitchea: $(O)  
	gcc -o $(HOME)/bin/$(MACHTYPE)/stitchea $O ../../lib/$(MACHTYPE)/jkweb.a



.c.o:
	gcc -I../../inc -I../../../inc -c $*.c

L = 
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = patJobs.o 

patJobs: $(O)
	gcc -o $(HOME)/bin/$(MACHTYPE)/patJobs $O ../../../lib/$(MACHTYPE)/jkweb.a



include ../../../inc/makefile.h

L = 
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = patJobs.o 

patJobs: $(O)
	gcc -o $(HOME)/bin/$(MACHTYPE)/patJobs $O ../../../lib/$(MACHTYPE)/jkweb.a



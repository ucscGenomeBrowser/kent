.c.o:
	gcc -I../../inc -I../../../inc -c $*.c

L = 
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = repMaskJobs.o 

repMaskJobs: $(O)
	gcc -o repMaskJobs $O ../../../lib/$(MACHTYPE)/jkweb.a



include ../../../../inc/common.mk

L = 
MYLIBDIR = ../../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = patJobs.o 

patJobs: $(O)
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/patJobs $O ../../../../lib/$(MACHTYPE)/jkweb.a



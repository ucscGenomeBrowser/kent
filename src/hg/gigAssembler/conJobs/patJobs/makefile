include ../../../../inc/common.mk

L += 
MYLIBDIR = ../../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = patJobs.o 

patJobs: $(O)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/patJobs $O ../../../../lib/$(MACHTYPE)/jkweb.a ${L}

clean::
	rm -f ${O}

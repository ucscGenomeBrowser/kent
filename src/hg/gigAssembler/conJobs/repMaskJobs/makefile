include ../../../../inc/common.mk

L += 
MYLIBDIR = ../../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = repMaskJobs.o 

repMaskJobs: $(O)
	${CC} ${COPT} ${CFLAGS} -o repMaskJobs $O ../../../../lib/$(MACHTYPE)/jkweb.a ${L}

clean::
	rm -f ${O}

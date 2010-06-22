include ../../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = sanger22gtf.o

sanger22gtf: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/sanger22gtf $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/sanger22gtf${EXE}

clean::
	rm -f ${O}

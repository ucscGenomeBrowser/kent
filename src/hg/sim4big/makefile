include ../../inc/common.mk


L += -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = sim4big.o

sim4big: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/sim4big $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/sim4big${EXE}

clean::
	rm -f ${O}

include ../../inc/common.mk


L += -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = semiNorm.o

semiNorm: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/semiNorm $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/semiNorm${EXE}

clean::
	rm -f ${O}

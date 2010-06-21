include ../../inc/common.mk


L += -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = pslToXa.o

pslToXa: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/pslToXa $O $(MYLIBS) $L
#	${STRIP} ${BINDIR}/pslToXa${EXE}

clean::
	rm -f ${O}

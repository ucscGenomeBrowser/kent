include ../../inc/common.mk


L += -lm ${SOCKETLIB}
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = pslToPslx.o

pslToPslx: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/pslToPslx${EXE} $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/pslToPslx${EXE}

clean::
	rm -f ${O}

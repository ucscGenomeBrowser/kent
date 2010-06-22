include ../../inc/common.mk


L += -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = faNcbiToUcsc.o

faNcbiToUcsc: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/faNcbiToUcsc $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/faNcbiToUcsc${EXE}

clean::
	rm -f ${O}

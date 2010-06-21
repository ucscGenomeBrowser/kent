include ../../inc/common.mk


L += -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = faRc.o

faRc: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/faRc $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/faRc${EXE}

clean::
	rm -f ${O}

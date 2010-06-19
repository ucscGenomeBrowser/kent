include ../../inc/common.mk


L += -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = randomLines.o

randomLines: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/randomLines $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/randomLines${EXE}

clean::
	rm -f ${O}

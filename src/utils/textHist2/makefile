include ../../inc/common.mk


L += -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = textHist2.o

textHist2: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/textHist2 $O $(MYLIBS) $L
#	${STRIP} ${BINDIR}/textHist2${EXE}

clean::
	rm -f ${O}

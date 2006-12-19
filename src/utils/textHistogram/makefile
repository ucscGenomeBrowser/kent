include ../../inc/common.mk
HG_WARN = ${HG_WARN_ERR}

L = -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = textHistogram.o

textHistogram: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/textHistogram $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/textHistogram${EXE}

clean:
	rm -f ${O}

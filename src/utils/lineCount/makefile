include ../../inc/common.mk

L += -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = lineCount.o

lineCount: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/lineCount $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/lineCount${EXE}

clean::
	rm -f ${O}

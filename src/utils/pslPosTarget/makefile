include ../../inc/common.mk

L += -lm
MYLIBDIR = ../..//lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = pslPosTarget.o

pslPosTarget: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/pslPosTarget $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/pslPosTarget${EXE}

clean::
	rm -f ${O}

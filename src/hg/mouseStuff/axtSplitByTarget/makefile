include ../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = axtSplitByTarget.o

axtSplitByTarget: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/axtSplitByTarget $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/axtSplitByTarget${EXE}

clean::
	rm -f ${O}

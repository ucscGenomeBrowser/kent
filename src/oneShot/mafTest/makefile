kentSrc = ../..
include ../../inc/common.mk


L += -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = mafTest.o mafXml.o

mafTest: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/mafTest $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/mafTest${EXE}

clean::
	rm -f ${O}

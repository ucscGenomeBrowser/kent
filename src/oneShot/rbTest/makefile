kentSrc = ../..
include ../../inc/common.mk


L += -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = rbTest.o 

rbTest: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/rbTest $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/rbTest${EXE}

clean::
	rm -f ${O}

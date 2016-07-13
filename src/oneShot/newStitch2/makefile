kentSrc = ../..
include ../../inc/common.mk


L += -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = newStitch2.o

newStitch2: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/newStitch2 $O $(MYLIBS) $L
#	${STRIP} ${BINDIR}/newStitch2${EXE}

clean::
	rm -f ${O}

kentSrc = ../..
include ../../inc/common.mk


L += -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = checkSeed.o

checkSeed: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/checkSeed $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/checkSeed${EXE}

clean::
	rm -f ${O}

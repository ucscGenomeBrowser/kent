kentSrc = ../..
include ../../inc/common.mk


L += -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = newStitch.o

newStitch: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/newStitch $O $(MYLIBS) $L
#	${STRIP} ${BINDIR}/newStitch${EXE}

clean::
	rm -f ${O}

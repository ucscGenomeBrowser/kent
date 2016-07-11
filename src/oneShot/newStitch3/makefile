kentSrc = ../..
include ../../inc/common.mk


L += -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = newStitch3.o

newStitch3: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/newStitch3 $O $(MYLIBS) $L
#	${STRIP} ${BINDIR}/newStitch3${EXE}

clean::
	rm -f ${O}

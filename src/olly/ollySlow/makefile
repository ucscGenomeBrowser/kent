kentSrc = ../..
include ../../inc/common.mk


L += -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = ollySlow.o

ollySlow: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/ollySlow $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/ollySlow${EXE}

clean::
	rm -f ${O}

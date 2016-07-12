kentSrc = ../..
include ../../inc/common.mk


L += -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkOwnLib.a $(MYLIBDIR)/jkweb.a

O = blat2p.o

blat2p: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/blat2p $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/blat2p${EXE}

clean::
	rm -f ${O}

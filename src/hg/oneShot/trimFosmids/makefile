kentSrc = ../../..
include ../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = trimFosmids.o

trimFosmids: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/trimFosmids $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/trimFosmids${EXE}

clean::
	rm -f ${O}

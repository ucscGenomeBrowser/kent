kentSrc = ../../..
include ../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = raPriority.o

raPriority: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/raPriority $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/raPriority${EXE}

clean::
	rm -f ${O}

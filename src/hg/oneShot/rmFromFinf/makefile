kentSrc = ../../..
include ../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = rmFromFinf.o

rmFromFinf: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/rmFromFinf $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/rmFromFinf${EXE}

clean::
	rm -f ${O}

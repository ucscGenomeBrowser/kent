kentSrc = ../..
include ../../inc/common.mk

L += -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = countServer.o

countServer: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/countServer $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/countServer${EXE}

clean::
	rm -f ${O}

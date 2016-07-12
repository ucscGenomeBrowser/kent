kentSrc = ../..
include ../../inc/common.mk

L += -lm -lpthread
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = burstServer.o

burstServer: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/burstServer $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/burstServer${EXE}

clean::
	rm -f ${O}

kentSrc = ../..
include ../../inc/common.mk

L += -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = burstClient.o

burstClient: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/burstClient $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/burstClient${EXE}

clean::
	rm -f ${O}

include ../../inc/common.mk

L = -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = burstClient.o

burstClient: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/burstClient $O $(MYLIBS) $L
	${STRIP} $(HOME)/bin/$(MACHTYPE)/burstClient${EXE}

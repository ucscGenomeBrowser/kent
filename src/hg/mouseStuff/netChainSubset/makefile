include ../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = netChainSubset.o

netChainSubset: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/netChainSubset $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/netChainSubset${EXE}

clean::
	rm -f ${O}

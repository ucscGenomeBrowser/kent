include ../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = subsetTraces.o

subsetTraces: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/subsetTraces $O $(MYLIBS) $L

clean::
	rm -f ${O}

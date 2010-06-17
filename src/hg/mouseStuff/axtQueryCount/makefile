include ../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = axtQueryCount.o

axtQueryCount: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/axtQueryCount $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/axtQueryCount${EXE}

clean::
	rm -f ${O}

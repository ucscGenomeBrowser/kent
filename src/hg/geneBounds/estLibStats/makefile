include ../../../inc/common.mk

L += $(MYSQLLIBS) -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = estLibStats.o

estLibStats: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/estLibStats $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/estLibStats${EXE}

clean::
	rm -f ${O}

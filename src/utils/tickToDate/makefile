include ../../inc/common.mk

L += -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = tickToDate.o

tickToDate: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/tickToDate $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/tickToDate${EXE}

clean::
	rm -f ${O}

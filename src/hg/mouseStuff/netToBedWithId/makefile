include ../../../inc/common.mk

L += ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = netToBedWithId.o

netToBedWithId: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/netToBedWithId $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/netToBedWithId${EXE}

clean::
	rm -f ${O}

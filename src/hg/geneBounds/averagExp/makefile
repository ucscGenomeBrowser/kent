include ../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = averagExp.o

averagExp: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/averagExp $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/averagExp${EXE}

clean::
	rm -f ${O}

include ../../../inc/common.mk

L += -lm $(MYSQLLIBS)
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = doOmimAv.o

doOmimAv: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/doOmimAv $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/doOmimAv${EXE}

clean::
	rm -f ${O}

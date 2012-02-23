include ../../../inc/common.mk

L += -lm $(MYSQLLIBS)
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = doOmimGene2.o

doOmimGene2: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/doOmimGene2 $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/doOmimGene2${EXE}

clean::
	rm -f ${O}

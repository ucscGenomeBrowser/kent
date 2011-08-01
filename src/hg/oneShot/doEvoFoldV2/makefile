include ../../../inc/common.mk

L += -lm $(MYSQLLIBS)
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = doEvoFoldV2.o

doEvoFoldV2: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/doEvoFoldV2 $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/doEvoFoldV2${EXE}

clean::
	rm -f ${O}

include ../../../../inc/common.mk

L += $(MYSQLLIBS) -lm
MYLIBDIR = ../../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = joinerGraph.o

joinerGraph: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/joinerGraph $O $(MYLIBS) $L
#	${STRIP} ${BINDIR}/joinerGraph${EXE}

clean::
	rm -f ${O}

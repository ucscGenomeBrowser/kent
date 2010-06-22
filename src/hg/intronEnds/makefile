include ../../inc/common.mk


L += -lm $(MYSQLLIBS)
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = intronEnds.o

intronEnds: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/intronEnds $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/intronEnds${EXE}

clean::
	rm -f ${O}

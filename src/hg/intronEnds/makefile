include ../../inc/common.mk


L = -lm $(MYSQLLIBS)
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = intronEnds.o

intronEnds: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/intronEnds $O $(MYLIBS) $L
	${STRIP} $(HOME)/bin/$(MACHTYPE)/intronEnds${EXE}

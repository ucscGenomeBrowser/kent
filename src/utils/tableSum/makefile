include ../../inc/common.mk


L = -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = tableSum.o

tableSum: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/tableSum $O $(MYLIBS) $L
	${STRIP} $(HOME)/bin/$(MACHTYPE)/tableSum${EXE}

include ../../inc/common.mk


L = -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = addFinFinf.o

addFinFinf: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/addFinFinf $O $(MYLIBS) $L
	${STRIP} $(HOME)/bin/$(MACHTYPE)/addFinFinf${EXE}

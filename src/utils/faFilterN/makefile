include ../../inc/common.mk


L = -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = faFilterN.o

faFilterN: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/faFilterN $O $(MYLIBS) $L
	${STRIP} $(HOME)/bin/$(MACHTYPE)/faFilterN${EXE}

include ../../inc/common.mk


L = -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = detab.o

detab: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/detab $O $(MYLIBS) $L
	${STRIP} $(HOME)/bin/$(MACHTYPE)/detab${EXE}

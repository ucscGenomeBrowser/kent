include ../../inc/common.mk


L = -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = randomLines.o

randomLines: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/randomLines $O $(MYLIBS) $L
	${STRIP} $(HOME)/bin/$(MACHTYPE)/randomLines${EXE}

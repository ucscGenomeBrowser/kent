include ../../inc/common.mk

L = -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = lineCount.o

lineCount: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/lineCount $O $(MYLIBS) $L
	${STRIP} $(HOME)/bin/$(MACHTYPE)/lineCount${EXE}

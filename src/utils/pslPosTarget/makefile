include ../../inc/common.mk

L = -lm
MYLIBDIR = ../..//lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = pslPosTarget.o

pslPosTarget: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/pslPosTarget $O $(MYLIBS) $L
	${STRIP} $(HOME)/bin/$(MACHTYPE)/pslPosTarget${EXE}

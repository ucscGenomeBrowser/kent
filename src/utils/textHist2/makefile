include ../../inc/common.mk


L = -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = textHist2.o

textHist2: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/textHist2 $O $(MYLIBS) $L
#	${STRIP} $(HOME)/bin/$(MACHTYPE)/textHist2${EXE}

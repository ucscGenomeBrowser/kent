include ../../../inc/common.mk

L = -lm
MYLIBDIR = $(HOME)/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = rnaFoldBig.o

rnaFoldBig: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/rnaFoldBig $O $(MYLIBS) $L
	${STRIP} $(HOME)/bin/$(MACHTYPE)/rnaFoldBig${EXE}

include ../../../inc/common.mk

L = -lm
MYLIBDIR = $(HOME)/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = netToBedWithId.o

netToBedWithId: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/netToBedWithId $O $(MYLIBS) $L
	${STRIP} $(HOME)/bin/$(MACHTYPE)/netToBedWithId${EXE}

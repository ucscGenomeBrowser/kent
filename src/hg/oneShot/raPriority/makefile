include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = raPriority.o

raPriority: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/raPriority $O $(MYLIBS) $L
	${STRIP} $(HOME)/bin/$(MACHTYPE)/raPriority${EXE}

include ../../../inc/common.mk

L = -lm
MYLIBDIR = $(HOME)/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = chimpChromQuals.o

chimpChromQuals: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/chimpChromQuals $O $(MYLIBS) $L
	${STRIP} $(HOME)/bin/$(MACHTYPE)/chimpChromQuals${EXE}

include ../../../inc/common.mk

L = -lm
MYLIBDIR = $(HOME)/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = chimpSuperQuals.o

chimpSuperQuals: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/chimpSuperQuals $O $(MYLIBS) $L
	${STRIP} $(HOME)/bin/$(MACHTYPE)/chimpSuperQuals${EXE}

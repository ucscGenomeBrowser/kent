include ../../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = hgSgdPep.o

hgSgdPep: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/hgSgdPep $O $(MYLIBS) $L
	${STRIP} $(HOME)/bin/$(MACHTYPE)/hgSgdPep${EXE}

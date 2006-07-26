include ../../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = hgAvidShortBed.o

hgAvidShortBed: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/hgAvidShortBed $O $(MYLIBS) $L
	${STRIP} $(HOME)/bin/$(MACHTYPE)/hgAvidShortBed${EXE}

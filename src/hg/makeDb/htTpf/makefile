include ../../../inc/common.mk

L = -lm
MYLIBDIR = $(HOME)/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = htTpf.o

htTpf: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/htTpf $O $(MYLIBS) $L
	${STRIP} $(HOME)/bin/$(MACHTYPE)/htTpf${EXE}

include ../../../../inc/common.mk

L = $(MYSQLLIBS) -lm
MYLIBDIR = $(HOME)/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = hgSgdGfp.o

hgSgdGfp: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/hgSgdGfp $O $(MYLIBS) $L
	${STRIP} $(HOME)/bin/$(MACHTYPE)/hgSgdGfp${EXE}

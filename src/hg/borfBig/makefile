include ../../inc/common.mk


L = -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = borfBig.o

borfBig: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/borfBig $O $(MYLIBS) $L
	${STRIP} $(HOME)/bin/$(MACHTYPE)/borfBig${EXE}

include ../../inc/common.mk


L = -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = endsInLf.o

endsInLf: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/endsInLf $O $(MYLIBS) $L
	${STRIP} $(HOME)/bin/$(MACHTYPE)/endsInLf${EXE}

include ../../inc/common.mk


L = -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = stringify.o

stringify: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/stringify $O $(MYLIBS) $L
	${STRIP} $(HOME)/bin/$(MACHTYPE)/stringify${EXE}

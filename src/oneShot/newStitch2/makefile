include ../../inc/common.mk


L = -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = newStitch2.o

newStitch2: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/newStitch2 $O $(MYLIBS) $L
#	${STRIP} $(HOME)/bin/$(MACHTYPE)/newStitch2${EXE}

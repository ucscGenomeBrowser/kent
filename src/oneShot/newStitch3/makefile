include ../../inc/common.mk


L = -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = newStitch3.o

newStitch3: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/newStitch3 $O $(MYLIBS) $L
#	${STRIP} $(HOME)/bin/$(MACHTYPE)/newStitch3${EXE}

include ../../inc/common.mk


L = -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = rbTest.o 

rbTest: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/rbTest $O $(MYLIBS) $L
	${STRIP} $(HOME)/bin/$(MACHTYPE)/rbTest${EXE}

include ../../inc/common.mk


L = -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a


O = pslReps.o 

pslReps: $O $(MYLIBS)
	gcc -o $(HOME)/bin/$(MACHTYPE)/pslReps $O $(MYLIBS) $L
	${STRIP} $(HOME)/bin/$(MACHTYPE)/pslReps${EXE}

lib:
	cd ../../lib; make

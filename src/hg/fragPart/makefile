include ../../inc/common.mk


L = $(MYSQLLIBS) -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a


O = fragPart.o 

fragPart: $O $(MYLIBS)
	gcc -o $(HOME)/bin/$(MACHTYPE)/fragPart $O $(MYLIBS) $L
	${STRIP} $(HOME)/bin/$(MACHTYPE)/fragPart${EXE}

lib:
	cd ../../lib; make

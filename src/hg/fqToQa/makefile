include ../../inc/common.mk


L = -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = fqToQa.o

fqToQa: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/fqToQa $O $(MYLIBS) $L
	${STRIP} $(HOME)/bin/$(MACHTYPE)/fqToQa${EXE}


lib:
	cd ../lib; make
	cd ../../lib; make

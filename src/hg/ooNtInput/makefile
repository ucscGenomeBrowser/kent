include ../../inc/common.mk


L = -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = ooNtInput.o

ooNtInput: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/ooNtInput $O $(MYLIBS) $L
	strip $(HOME)/bin/$(MACHTYPE)/ooNtInput


lib:
	cd ../lib; make
	cd ../../lib; make

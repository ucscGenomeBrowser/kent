include ../../inc/common.mk


L = $(MYSQLLIBS) -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = pslToBed.o 

myown:pslToBed
	mv pslToBed $(HOME)/bin/$(MACHTYPE)/

pslToBed:
	gcc -Wimplicit -ggdb  -I../inc -I../../inc  -o $@ $(addsuffix .c,$@) $(MYLIBS) $(L)

lib:
	cd ../../lib; make

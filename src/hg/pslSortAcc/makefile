include ../../inc/common.mk


L = -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = pslSortAcc.o

pslSortAcc: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/pslSortAcc $O $(MYLIBS) $L


lib:
	cd ../lib; make
	cd ../../lib; make

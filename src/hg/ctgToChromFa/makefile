include ../../inc/common.mk


L = 
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a


O = ctgToChromFa.o 

ctgToChromFa: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/ctgToChromFa $O $(MYLIBS) $L
	${STRIP} $(HOME)/bin/$(MACHTYPE)/ctgToChromFa${EXE}

lib:
	cd ../../lib; make

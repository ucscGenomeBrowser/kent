include ../../inc/common.mk


L = -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = gpStats.o

gpStats: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/gpStats $O $(MYLIBS) $L
	${STRIP} $(HOME)/bin/$(MACHTYPE)/gpStats${EXE}


lib:
	cd ../lib; make
	cd ../../lib; make

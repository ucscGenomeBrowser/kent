include ../../../inc/common.mk


L = $(MYSQLLIBS) -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a


O = g2gOverlap.o 

g2gOverlap: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/g2gOverlap $O $(MYLIBS) $L
	${STRIP} $(HOME)/bin/$(MACHTYPE)/g2gOverlap${EXE}

lib:
	cd ../../../lib; make

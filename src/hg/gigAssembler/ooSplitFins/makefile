include ../../../inc/common.mk


L = -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = ooSplitFins.o

ooSplitFins: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/ooSplitFins $O $(MYLIBS) $L


lib:
	cd ../../lib && ${MAKE}
	cd ../../../lib && ${MAKE}

include ../../../inc/common.mk

L = -lm $(MYSQLLIBS)
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = spLoadRankProp.o rankPropProt.o

spLoadRankProp: $O $(MYLIBS)
	${CC} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/spLoadRankProp $O $(MYLIBS) $L

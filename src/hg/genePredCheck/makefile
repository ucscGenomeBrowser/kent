include ../../inc/common.mk

MYLIBDIR = ../../lib/$(MACHTYPE)
L = $(MYSQLLIBS) -lm
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = genePredCheck.o

genePredCheck: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/genePredCheck $O $(MYLIBS) $L

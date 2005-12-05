include ../../inc/common.mk
HG_WARN = ${HG_WARN_ERR}

MYLIBDIR = ../../lib/$(MACHTYPE)
L = $(MYSQLLIBS) -lm
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = genePredHisto.o

genePredHisto: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/genePredHisto $O $(MYLIBS) $L

test:
	(cd tests && ${MAKE} test)

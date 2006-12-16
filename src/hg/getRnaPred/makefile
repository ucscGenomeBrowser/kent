include ../../inc/common.mk

L = -lm $(MYSQLLIBS)
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = getRnaPred.o

getRnaPred: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/getRnaPred $O $(MYLIBS) $L
	${STRIP} $(HOME)/bin/$(MACHTYPE)/getRnaPred${EXE}

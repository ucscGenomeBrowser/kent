include ../../../../inc/common.mk

L = $(MYSQLLIBS) -lm
MYLIBDIR = ../../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = hgExonerate.o

hgExonerate: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/hgExonerate $O $(MYLIBS) $L

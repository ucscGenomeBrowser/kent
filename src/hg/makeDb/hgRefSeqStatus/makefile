include ../../../inc/common.mk

L = $(MYSQLLIBS) -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

A = hgRefSeqStatus
O = $A.o

$A: $O ../../../lib/$(MACHTYPE)/jkweb.a
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/$A $O $(MYLIBS) $L

lib:
	cd ../../../lib && ${MAKE}

clean::
	rm -f ${O} ${A}

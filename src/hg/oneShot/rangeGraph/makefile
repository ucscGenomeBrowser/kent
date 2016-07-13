kentSrc = ../../..
include ../../../inc/common.mk

L += $(MYSQLLIBS)
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = rangeGraph.o rgi.o

rangeGraph: $O 
	${CC} ${COPT} ${CFLAGS} -o rangeGraph $O $(MYLIBS) $L

clean::
	rm -f ${O}

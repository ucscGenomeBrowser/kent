kentSrc = ../../..
include ../../../inc/common.mk

L += $(MYSQLLIBS)
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = faFiltMasked.o

faFiltMasked: $O 
	${CC} ${COPT} ${CFLAGS} -o faFiltMasked $O $(MYLIBS) $L

clean::
	rm -f ${O}

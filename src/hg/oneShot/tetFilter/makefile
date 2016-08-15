kentSrc = ../../..
include ../../../inc/common.mk

L += $(MYSQLLIBS)
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = tetFilter.o

tetFilter: $O 
	${CC} ${COPT} ${CFLAGS} -o tetFilter $O $(MYLIBS) $L

clean::
	rm -f ${O}

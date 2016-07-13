kentSrc = ../../..
include ../../../inc/common.mk

L += $(MYSQLLIBS)
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = tetDensity.o

tetDensity: $O 
	${CC} ${COPT} ${CFLAGS} -o tetDensity $O $(MYLIBS) $L

clean::
	rm -f ${O}

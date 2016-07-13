kentSrc = ../../..
include ../../../inc/common.mk

L += $(MYSQLLIBS)
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = addChrPsl.o

addChrPsl: $O 
	${CC} ${COPT} ${CFLAGS} -o addChrPsl $O $(MYLIBS) $L

clean::
	rm -f ${O}

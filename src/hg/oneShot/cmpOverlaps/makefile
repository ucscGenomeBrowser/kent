kentSrc = ../../..
include ../../../inc/common.mk

L += $(MYSQLLIBS)
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = cmpOverlaps.o

cmpOverlaps: $O 
	${CC} ${COPT} ${CFLAGS} -o cmpOverlaps $O $(MYLIBS) $L

clean::
	rm -f ${O}

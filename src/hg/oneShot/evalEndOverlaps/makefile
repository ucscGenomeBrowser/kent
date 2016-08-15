kentSrc = ../../..
include ../../../inc/common.mk

L += $(MYSQLLIBS)
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = evalEndOverlaps.o ocp.o

evalEndOverlaps: $O 
	${CC} ${COPT} ${CFLAGS} -o evalEndOverlaps $O $(MYLIBS) $L

clean::
	rm -f ${O}

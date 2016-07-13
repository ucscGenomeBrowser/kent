kentSrc = ../../..
include ../../../inc/common.mk

L += $(MYSQLLIBS)
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = clumpEst3.o

clumpEst3: $O 
	${CC} ${COPT} ${CFLAGS} -o clumpEst3 $O $(MYLIBS) $L

clean::
	rm -f ${O}

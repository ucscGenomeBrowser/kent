kentSrc = ../../..
include ../../../inc/common.mk

L += 
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = joinToPatch.o

joinToPatch: $O 
	${CC} ${COPT} ${CFLAGS} -o joinToPatch $O $(MYLIBS) $L

clean::
	rm -f ${O}

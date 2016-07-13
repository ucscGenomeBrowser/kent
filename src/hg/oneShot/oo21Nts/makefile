kentSrc = ../../..
include ../../../inc/common.mk

L += 
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = oo21Nts.o

oo21Nts: $O 
	${CC} ${COPT} ${CFLAGS} -o oo21Nts $O $(MYLIBS) $L

clean::
	rm -f ${O}

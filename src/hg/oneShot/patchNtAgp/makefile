kentSrc = ../../..
include ../../../inc/common.mk

L += 
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = patchNtAgp.o

patchNtAgp: $O 
	${CC} ${COPT} ${CFLAGS} -o patchNtAgp $O $(MYLIBS) $L

clean::
	rm -f ${O}

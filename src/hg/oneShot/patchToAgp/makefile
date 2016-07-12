kentSrc = ../../..
include ../../../inc/common.mk

L += 
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = patchToAgp.o

patchToAgp: $O 
	${CC} ${COPT} ${CFLAGS} -o patchToAgp $O $(MYLIBS) $L

clean::
	rm -f ${O}

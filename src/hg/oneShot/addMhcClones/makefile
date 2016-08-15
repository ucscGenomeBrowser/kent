kentSrc = ../../..
include ../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = addMhcClones.o

addMhcClones: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/addMhcClones $O $(MYLIBS) $L

clean::
	rm -f ${O}

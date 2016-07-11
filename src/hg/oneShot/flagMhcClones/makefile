kentSrc = ../../..
include ../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = flagMhcClones.o

flagMhcClones: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/flagMhcClones $O $(MYLIBS) $L

clean::
	rm -f ${O}

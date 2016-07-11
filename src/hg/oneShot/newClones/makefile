kentSrc = ../../..
include ../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = newClones.o

newClones: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/newClones $O $(MYLIBS) $L

clean::
	rm -f ${O}

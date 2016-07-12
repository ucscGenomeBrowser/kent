kentSrc = ..
include ../inc/common.mk

L += -lm
MYLIBDIR = ../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = blatTheory.o

blatTheory: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/blatTheory $O $(MYLIBS) $L

clean::
	rm -f ${O}

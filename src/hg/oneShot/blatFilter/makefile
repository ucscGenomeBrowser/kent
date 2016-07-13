kentSrc = ../../..
include ../../../inc/common.mk

L += -lm 
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = blatFilter.o

blatFilter: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/blatFilter $O $(MYLIBS) $L

clean::
	rm -f ${O}

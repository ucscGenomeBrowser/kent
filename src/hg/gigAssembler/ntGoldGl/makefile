kentSrc = ../../..
include ../../../inc/common.mk


L += -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = ntGoldGl.o

ntGoldGl: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/ntGoldGl $O $(MYLIBS) $L

clean::
	rm -f ${O}

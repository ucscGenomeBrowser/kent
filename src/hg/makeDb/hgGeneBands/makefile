include ../../../inc/common.mk

L += $(MYSQLLIBS) -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a


O = hgGeneBands.o

hgGeneBands: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/hgGeneBands $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/hgGeneBands${EXE}

clean::
	rm -f ${O}

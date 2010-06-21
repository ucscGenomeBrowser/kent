include ../../../inc/common.mk

L += $(MYSQLLIBS) -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = tfbsConsSort.o

tfbsConsSort: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/tfbsConsSort $O $(MYLIBS) $L
#	${STRIP} ${BINDIR}/tfbsConsSort${EXE}

clean::
	rm -f ${O}

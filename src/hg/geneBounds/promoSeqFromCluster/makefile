include ../../../inc/common.mk

L += -lm $(MYSQLLIBS)
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = promoSeqFromCluster.o

promoSeqFromCluster: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/promoSeqFromCluster $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/promoSeqFromCluster${EXE}

clean::
	rm -f ${O}

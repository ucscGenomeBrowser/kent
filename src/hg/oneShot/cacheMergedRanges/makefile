kentSrc = ../../..
include ../../../inc/common.mk

L = ${kentSrc}/htslib/libhts.a ${MYSQLLIBS}
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = cacheMergedRanges.o rbmTree.o

cacheMergedRanges: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o cacheMergedRanges $O $(MYLIBS) $L
	mv cacheMergedRanges${EXE} ${BINDIR}/
	${STRIP} ${BINDIR}/cacheMergedRanges${EXE}

clean::
	rm -f ${O}

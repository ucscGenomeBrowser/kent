include ../../../inc/common.mk

L = -lm $(MYSQLLIBS)
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = cacheMergedRanges.o rbmTree.o

cacheMergedRanges: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o cacheMergedRanges $O $(MYLIBS) $L
	mv cacheMergedRanges${EXE} ${HOME}/bin/${MACHTYPE}/
	${STRIP} ${HOME}/bin/${MACHTYPE}/cacheMergedRanges${EXE}

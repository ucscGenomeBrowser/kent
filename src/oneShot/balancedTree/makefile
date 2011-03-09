include ../../inc/common.mk


L += -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = balancedTree.o

balancedTree: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/balancedTree $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/balancedTree${EXE}

clean::
	rm -f ${O}

include ../../inc/common.mk


L += -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = agpToGl.o

agpToGl: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/agpToGl $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/agpToGl${EXE}

clean::
	rm -f ${O}

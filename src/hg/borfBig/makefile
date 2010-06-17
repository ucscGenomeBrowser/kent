include ../../inc/common.mk


L += -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = borfBig.o

borfBig: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/borfBig $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/borfBig${EXE}

clean::
	rm -f ${O}

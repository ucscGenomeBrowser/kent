include ../../inc/common.mk


L += -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = mdToNcbiLift.o

mdToNcbiLift: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/mdToNcbiLift $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/mdToNcbiLift${EXE}

clean::
	rm -f ${O}

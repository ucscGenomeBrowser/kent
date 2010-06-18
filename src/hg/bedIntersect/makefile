include ../../inc/common.mk


L += -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = bedIntersect.o

bedIntersect: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/bedIntersect $O $(MYLIBS) $L
#	${STRIP} ${BINDIR}/bedIntersect${EXE}

clean::
	rm -f ${O}

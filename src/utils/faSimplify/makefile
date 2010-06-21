include ../../inc/common.mk

L += -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = faSimplify.o

faSimplify: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/faSimplify $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/faSimplify${EXE}

clean::
	rm -f ${O}

include ../../../../inc/common.mk

L += -lm $(MYSQLLIBS)
MYLIBDIR = ../../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = hgPhMouse.o

hgPhMouse: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/hgPhMouse $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/hgPhMouse${EXE}

clean::
	rm -f ${O}

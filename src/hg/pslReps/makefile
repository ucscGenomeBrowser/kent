include ../../inc/common.mk


L += -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a


O = pslReps.o 

pslReps: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${DESTDIR}${BINDIR}/pslReps${EXE} $O $(MYLIBS) $L

lib:
	cd ../../lib && ${MAKE}

clean::
	rm -f ${O}

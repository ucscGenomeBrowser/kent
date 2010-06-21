include ../../inc/common.mk


L += -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = detab.o

detab: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/detab $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/detab${EXE}

clean::
	rm -f ${O}

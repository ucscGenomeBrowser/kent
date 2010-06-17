include ../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = axtToMaf.o

axtToMaf: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/axtToMaf $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/axtToMaf${EXE}

test: $(INSTALLDIR)/$(PROG)
	cd tests && ${MAKE} test

clean::
	rm -f ${O}

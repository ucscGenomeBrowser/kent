kentSrc = ../../..
include ../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a
INSTALLDIR = ${BINDIR}

PROG = chimpChromContigAgp

O = ${PROG}.o

${PROG}: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o $(INSTALLDIR)/${PROG} $O $(MYLIBS) $L
#${STRIP} ${BINDIR}/${PROG}${EXE}

test: $(INSTALLDIR)/$(PROG)
	cd tests && ${MAKE} test

clean::
	rm -f ${O}

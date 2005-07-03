include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a
INSTALLDIR = ${HOME}/bin/${MACHTYPE}

PROG = chimpChromContigAgp

O = ${PROG}.o

${PROG}: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o $(INSTALLDIR)/${PROG} $O $(MYLIBS) $L
#${STRIP} $(HOME)/bin/$(MACHTYPE)/${PROG}${EXE}

test: $(INSTALLDIR)/$(PROG)
	cd tests && ${MAKE} test

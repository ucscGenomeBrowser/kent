include ../../../inc/common.mk

L = ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a
INSTALLDIR = ${HOME}/bin/${MACHTYPE}

PROG = ldGencodeIntron

all: ${PROG}

O = ${PROG}.o

${PROG}: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o $(INSTALLDIR)/${PROG} $O $(MYLIBS) $L
#${STRIP} $(HOME)/bin/$(MACHTYPE)/${PROG}${EXE}

test: $(INSTALLDIR)/$(PROG)
	cd tests && ${MAKE} test

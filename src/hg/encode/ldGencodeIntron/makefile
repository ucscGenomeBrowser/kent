kentSrc = ../../..
include ../../../inc/common.mk

L = ${kentSrc}/htslib/libhts.a ${MYSQLLIBS}
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a
INSTALLDIR = ${BINDIR}

PROG = ldGencodeIntron

all: ${PROG}

O = ${PROG}.o

${PROG}: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o $(INSTALLDIR)/${PROG} $O $(MYLIBS) $L
#${STRIP} ${BINDIR}/${PROG}${EXE}

test: $(INSTALLDIR)/$(PROG)
	cd tests && ${MAKE} test

clean::
	rm -f ${O}

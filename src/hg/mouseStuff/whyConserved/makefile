kentSrc = ../../..
include ../../../inc/common.mk

L += -lm $(MYSQLLIBS)
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = whyConserved.o 

all: whyConserved trackOverlap

whyConserved: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${DESTDIR}${BINDIR}/whyConserved $O $(MYLIBS) $L
	${STRIP} ${DESTDIR}${BINDIR}/whyConserved${EXE}

trackOverlap: trackOverlap.o
	${CC} ${COPT} ${CFLAGS} -o ${DESTDIR}${BINDIR}/trackOverlap trackOverlap.o $(MYLIBS) $L
	${STRIP} ${DESTDIR}${BINDIR}/trackOverlap${EXE}

clean::
	rm -f ${O}

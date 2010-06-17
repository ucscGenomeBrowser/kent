include ../../../inc/common.mk

L += -lm $(MYSQLLIBS)
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = whyConserved.o 

all: whyConserved trackOverlap

whyConserved: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/whyConserved $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/whyConserved${EXE}

trackOverlap: trackOverlap.o
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/trackOverlap trackOverlap.o $(MYLIBS) $L
	${STRIP} ${BINDIR}/trackOverlap${EXE}

clean::
	rm -f ${O}

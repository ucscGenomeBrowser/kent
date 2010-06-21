include ../../../inc/common.mk

L += -lm $(MYSQLLIBS)
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = iriToDnaMotif.o

iriToDnaMotif: gbLib $O $(MYLIBS)
	cd ../lib && ${MAKE}
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/iriToDnaMotif $O ../lib/improbRunInfo.o $(MYLIBS) $L
	${STRIP} ${BINDIR}/iriToDnaMotif${EXE}

gbLib::
	( cd ../lib && ${MAKE} )

clean::
	rm -f ${O}

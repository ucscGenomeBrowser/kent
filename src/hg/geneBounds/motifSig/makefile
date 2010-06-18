include ../../../inc/common.mk

L += -lm $(MYSQLLIBS)
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = motifSig.o

motifSig: gbLib $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/motifSig $O ../lib/improbRunInfo.o $(MYLIBS) $L
	${STRIP} ${BINDIR}/motifSig${EXE}

gbLib::
	( cd ../lib && ${MAKE} )

clean::
	rm -f ${O}

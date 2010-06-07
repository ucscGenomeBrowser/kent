include ../../../inc/common.mk

L += -lm $(MYSQLLIBS)
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = motifSig.o ../lib/improbRunInfo.o

motifSig: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/motifSig $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/motifSig${EXE}

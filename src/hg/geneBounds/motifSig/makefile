include ../../../inc/common.mk

L = -lm $(MYSQLLIBS)
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = motifSig.o ../lib/improbRunInfo.o

motifSig: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/motifSig $O $(MYLIBS) $L
	${STRIP} $(HOME)/bin/$(MACHTYPE)/motifSig${EXE}

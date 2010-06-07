include ../../../inc/common.mk

L += $(MYSQLLIBS) -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a


O = hgLoadCdsEvidence.o

hgLoadCdsEvidence: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/hgLoadCdsEvidence $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/hgLoadCdsEvidence${EXE}

clean:
	rm -f $O

kentSrc = ../../..
include ../../../inc/common.mk

L += -lm ${MYSQLLIBS}
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = hgEnsGeneXRef.o

hgEnsGeneXRef: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/hgEnsGeneXRef $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/hgEnsGeneXRef${EXE}
compile: $O 
	${CC} -o hgEnsGeneXRef $O ${MYLIBS} ${L}

clean::
	rm -f ${O}

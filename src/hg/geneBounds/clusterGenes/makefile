include ../../../inc/common.mk
HG_WARN = ${HG_WARN_ERR}

L += $(MYSQLLIBS) -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = clusterGenes.o

clusterGenes: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/clusterGenes $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/clusterGenes${EXE}

test::	clusterGenes
	(cd tests && ${MAKE} test)

clean::
	rm -f $O
	(cd tests && ${MAKE} clean)

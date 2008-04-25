include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = segExpandToAnchors.o

segExpandToAnchors: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/segExpandToAnchors $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/segExpandToAnchors${EXE}

test: segExpandToAnchors.o
	segExpandToAnchors test.seg out.seg

#segExpandToAnchors ~/projects/gene-clusters/single.clusters.pairwise.seg out.seg

clean:
	rm -f $O

include ../../inc/common.mk

L += -lm -lz
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a
TESTOUT = test/output
TESTIN = test/in
TESTEXPECTED = test/expected

A = bigWigMerge
O = bigWigMerge.o

bigWigMerge: ${O} ${MYLIBS}
	${CC} ${COPT} -o ${DESTDIR}${BINDIR}/${A}${EXE} $O ${MYLIBS} $L
	${STRIP} ${DESTDIR}${BINDIR}/${A}${EXE}

compile:: ${O}
	${CC} ${COPT} -o ${A}${EXE} ${O} ${MYLIBS} $L

clean::
	rm -f ${A}${EXE} ${O}
	rm -rf ${TESTOUT}

tests:: bigWigMerge
	mkdir -p ${TESTOUT}
	bedGraphToBigWig ${TESTIN}/t2.bed ${TESTIN}/hg19.chrom.sizes ${TESTOUT}/t2.bw
	bedGraphToBigWig ${TESTIN}/t1.bed ${TESTIN}/hg19.chrom.sizes  ${TESTOUT}/t1.bw
	bigWigMerge ${TESTOUT}/t1.bw ${TESTOUT}/t2.bw ${TESTOUT}/12merge.bedGraph
	diff ${TESTOUT}/12merge.bedGraph ${TESTEXPECTED}/12merge.bedGraph 
	bigWigMerge ${TESTOUT}/t2.bw ${TESTOUT}/t1.bw ${TESTOUT}/21merge.bedGraph
	diff ${TESTOUT}/21merge.bedGraph ${TESTEXPECTED}/21merge.bedGraph 
	rm -rf ${TESTOUT}

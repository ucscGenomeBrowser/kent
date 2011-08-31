include ../../inc/common.mk

L += -lm -lz
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

A = bigWigMerge
O = bigWigMerge.o

bigWigMerge: ${O} ${MYLIBS}
	${CC} ${COPT} -o ${DESTDIR}${BINDIR}/${A}${EXE} $O ${MYLIBS} $L
	${STRIP} ${DESTDIR}${BINDIR}/${A}${EXE}

compile:: ${O}
	${CC} ${COPT} -o ${A}${EXE} ${O} ${MYLIBS} $L

clean::
	rm -f ${A}${EXE} ${O}
	rm -f t1.bw t2.bw 12merge.bedGraph 21merge.bedGraph 

t2.bw:
	bedGraphToBigWig test/in/t2.bed /cluster/data/hg19/chrom.sizes t2.bw

t1.bw:
	bedGraphToBigWig test/in/t1.bed /cluster/data/hg19/chrom.sizes t1.bw

tests:: t1.bw t2.bw bigWigMerge
	bigWigMerge t1.bw t2.bw 12merge.bedGraph
	diff 12merge.bedGraph test/expected/12merge.bedGraph 
	bigWigMerge t2.bw t1.bw 21merge.bedGraph
	diff 21merge.bedGraph test/expected/21merge.bedGraph 

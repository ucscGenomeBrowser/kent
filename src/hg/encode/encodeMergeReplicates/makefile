include ../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

A = encodeMergeReplicates
O = encodeMergeReplicates.o

encodeMergeReplicates: ${O} ${MYLIBS}
	${CC} ${COPT} -o ${DESTDIR}${BINDIR}/${A}${EXE} $O ${MYLIBS} $L
	${STRIP} ${DESTDIR}${BINDIR}/${A}${EXE}

compile:: ${O}
	${CC} ${COPT} -o ${A}${EXE} ${O} ${MYLIBS} $L

clean::
	rm -f ${A}${EXE} ${O}

test: rep1Rep2Default rep1Rep2Agree rep1Rep2Add rep1Rep2Threshold10 rep1Rep2AddAddMin
	@echo done tests

rep1Rep2Default:
	encodeMergeReplicates rep1.narrowPeak rep2.narrowPeak outRep1Rep2Default.narrowPeak
	diff expRep1Rep2Default.narrowPeak outRep1Rep2Default.narrowPeak

rep1Rep2Agree:
	encodeMergeReplicates -agree=2 rep1.narrowPeak rep2.narrowPeak outRep1Rep2Agree.narrowPeak
	diff expRep1Rep2Agree.narrowPeak outRep1Rep2Agree.narrowPeak

rep1Rep2Add:
	encodeMergeReplicates -add rep1.narrowPeak rep2.narrowPeak outRep1Rep2Add.narrowPeak
	diff expRep1Rep2Add.narrowPeak outRep1Rep2Add.narrowPeak

rep1Rep2Threshold10:
	encodeMergeReplicates -threshold=10 rep1.narrowPeak rep2.narrowPeak outRep1Rep2Threshold10.narrowPeak
	diff expRep1Rep2Threshold10.narrowPeak outRep1Rep2Threshold10.narrowPeak

rep1Rep2AddAddMin:
	encodeMergeReplicates -add -addMin rep1.narrowPeak rep2.narrowPeak outRep1Rep2AddAddMin.narrowPeak
	diff expRep1Rep2AddAddMin.narrowPeak outRep1Rep2AddAddMin.narrowPeak

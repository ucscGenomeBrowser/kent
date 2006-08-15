include ../../inc/common.mk

L = ${MYSQLLIBS} -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = apacheMonitor.o checkCardinality.o countNib.o \
    dumpNib.o gbSeqCheck.o reviewIndexes.o seqCheck.o \
    getChroms.o checkChain.o

all: apacheMonitor checkCardinality countNib \
    dumpNib gbSeqCheck reviewIndexes seqCheck \
    getChroms checkChain

apacheMonitor: apacheMonitor.o ${MYLIBS}
	${CC} -O -o apacheMonitor apacheMonitor.o ${MYLIBS} $L

checkCardinality: checkCardinality.o ${MYLIBS}
	${CC} -O -o checkCardinality checkCardinality.o ${MYLIBS} $L

countNib: countNib.o ${MYLIBS}
	${CC} -O -o countNib countNib.o ${MYLIBS} $L

dumpNib: dumpNib.o ${MYLIBS}
	${CC} -O -o dumpNib dumpNib.o ${MYLIBS} $L

gbSeqCheck: gbSeqCheck.o ${MYLIBS}
	${CC} -O -o gbSeqCheck gbSeqCheck.o ${MYLIBS} $L

reviewIndexes: reviewIndexes.o ${MYLIBS}
	${CC} -O -o reviewIndexes reviewIndexes.o ${MYLIBS} $L

seqCheck: seqCheck.o ${MYLIBS}
	${CC} -O -o seqCheck seqCheck.o ${MYLIBS} $L

getChroms: getChroms.o ${MYLIBS}
	${CC} -O -o getChroms getChroms.o ${MYLIBS} $L

checkChain: checkChain.o ${MYLIBS}
	${CC} -O -o checkChain checkChain.o ${MYLIBS} $L

include ../../inc/common.mk

L = ${MYSQLLIBS} -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = apacheMonitor.o checkCardinality.o countNib.o \
    dumpNib.o gbSeqCheck.o reviewIndexes.o seqCheck.o \
    getChroms.o checkChain.o testSearch.o

all: apacheMonitor checkCardinality countNib \
    dumpNib gbSeqCheck reviewIndexes seqCheck \
    getChroms checkChain testSearch

apacheMonitor: apacheMonitor.o ${MYLIBS}
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/apacheMonitor apacheMonitor.o ${MYLIBS} $L

checkCardinality: checkCardinality.o ${MYLIBS}
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/checkCardinality checkCardinality.o ${MYLIBS} $L

countNib: countNib.o ${MYLIBS}
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/countNib countNib.o ${MYLIBS} $L

dumpNib: dumpNib.o ${MYLIBS}
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/dumpNib dumpNib.o ${MYLIBS} $L

gbSeqCheck: gbSeqCheck.o ${MYLIBS}
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/gbSeqCheck gbSeqCheck.o ${MYLIBS} $L

reviewIndexes: reviewIndexes.o ${MYLIBS}
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/reviewIndexes reviewIndexes.o ${MYLIBS} $L

seqCheck: seqCheck.o ${MYLIBS}
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/seqCheck seqCheck.o ${MYLIBS} $L

getChroms: getChroms.o ${MYLIBS}
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/getChroms getChroms.o ${MYLIBS} $L

checkChain: checkChain.o ${MYLIBS}
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/checkChain checkChain.o ${MYLIBS} $L

testSearch: testSearch.o ${MYLIBS}
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/testSearch testSearch.o ${MYLIBS} $L

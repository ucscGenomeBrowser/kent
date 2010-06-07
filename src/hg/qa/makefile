include ../../inc/common.mk

L += ${MYSQLLIBS} -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

progs = apacheMonitor checkCardinality countNib \
    dumpNib gbSeqCheck reviewIndexes seqCheck \
    getChroms checkChain testSearch timePosTable

all: ${progs}

%: %.c ${MYLIBS}
	${CC} ${COPT} ${CFLAGS} ${HG_DEFS} ${HG_WARN} ${HG_INC} ${XINC} -o ${BINDIR}/$* $*.c ${MYLIBS} $L


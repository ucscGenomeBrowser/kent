kentSrc = ../..
include ../../inc/common.mk

L += -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

DIRS = \
    agxToTxg \
    bedToTxEdges \
    txAbFragFind \
    txBedToGraph \
    txOrtho \
    txPslFilter \
    txPslToBed \
    txReadRa \
    txWalk \
    txgAddEvidence \
    txgAnalyze \
    txgGoodEdges \
    txgToAgx \
    txgToXml \
    txgTrim

all: ${DIRS:%=%_all}
%_all:
	${MAKE} -C $*

clean: ${DIRS:%=%_clean}
%_clean:
	${MAKE} -C $* clean

compile: ${DIRS:%=%_compile}
%_compile:
	${MAKE} -C $* compile

include ../../../inc/common.mk

PSLHISTO = ../pslHisto
DIFF = diff -u

test:   apqBaseTest apqBaseMultTest cdBaseTest cdBaseMultTest \
	idBaseTest idNonZeroTest

apqBaseTest:
	${MAKE} doHisto name=$@ what=alignsPerQuery inPsl=basic.psl

apqBaseMultTest:
	${MAKE} doHisto name=$@ what=alignsPerQuery opts=-multiOnly inPsl=basic.psl

cdBaseTest:
	${MAKE} doHisto name=$@ what=coverDiff inPsl=basic.psl

cdBaseMultTest:
	${MAKE} doHisto name=$@ what=coverDiff opts=-multiOnly inPsl=basic.psl

idBaseTest:
	${MAKE} doHisto name=$@ what=idDiff inPsl=basic.psl

idNonZeroTest:
	${MAKE} doHisto name=$@ what=idDiff opts=-nonZero inPsl=basic.psl

# recursive target to do test:
#  name - test name
#  inPsl - relative to input dir
#  what - what stat to collect
#  opts - options if needed
doHisto:
	@${MKDIR} output
	${PSLHISTO} ${opts} ${what} input/${inPsl} output/${name}.histo
	${DIFF} expected/${name}.histo output/${name}.histo

clean:
	rm -rf output

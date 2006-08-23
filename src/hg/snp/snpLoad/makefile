include ../../../inc/common.mk

L = ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = snpContigLocFilter.o snpContigLocFilter125.o \
    snpContigLocusIdFilter.o snpContigLocusIdFilter125.o snpContigLocusIdCondense.o \
    snpSplitByChrom.o snpSplitByChrom2.o snpLocType.o snpLocType125.o \
    snpExpandAllele.o snpSort.o snpRefUCSC.o \
    snpCheckAlleles.o snpLoadFasta.o snpReadFasta.o snpCheckClassAndObserved.o \
    snpFunction.o snpSNP.o snpFinalTable.o snpMultiple.o snpMissing.o snpCompareLoctype.o \
    snpPAR.o illuminaLookup.o snpCompareWeight.o subSnpCondense.o \
    snpClassAndObserved.o snpMoltype.o affyLookup.o cnpLookup.o \
    snpGetSimple.o fetchSeq.o snpOrthoStats.o

all: snpContigLocFilter snpContigLocFilter125 \
     snpContigLocusIdFilter snpContigLocusIdFilter125 snpContigLocusIdCondense \
     snpSplitByChrom snpSplitByChrom2 snpLocType snpLocType125 \
     snpExpandAllele snpSort snpRefUCSC \
     snpCheckAlleles snpLoadFasta snpReadFasta snpCheckClassAndObserved \
     snpFunction snpSNP snpFinalTable snpMultiple snpMissing snpCompareLoctype \
     snpPAR illuminaLookup snpCompareWeight subSnpCondense \
     snpClassAndObserved snpMoltype affyLookup cnpLookup \
     snpGetSimple fetchSeq snpOrthoStats

snpContigLocFilter: snpContigLocFilter.o ${MYLIBS}
	${CC} -O -o snpContigLocFilter snpContigLocFilter.o ${MYLIBS} $L

snpContigLocFilter125: snpContigLocFilter125.o ${MYLIBS}
	${CC} -O -o snpContigLocFilter125 snpContigLocFilter125.o ${MYLIBS} $L

snpContigLocusIdFilter: snpContigLocusIdFilter.o ${MYLIBS}
	${CC} -O -o snpContigLocusIdFilter snpContigLocusIdFilter.o ${MYLIBS} $L

snpContigLocusIdFilter125: snpContigLocusIdFilter125.o ${MYLIBS}
	${CC} -O -o snpContigLocusIdFilter125 snpContigLocusIdFilter125.o ${MYLIBS} $L

snpContigLocusIdCondense: snpContigLocusIdCondense.o ${MYLIBS}
	${CC} -O -o snpContigLocusIdCondense snpContigLocusIdCondense.o ${MYLIBS} $L

snpSplitByChrom: snpSplitByChrom.o ${MYLIBS}
	${CC} -O -o snpSplitByChrom snpSplitByChrom.o ${MYLIBS} $L

snpSplitByChrom2: snpSplitByChrom2.o ${MYLIBS}
	${CC} -O -o snpSplitByChrom2 snpSplitByChrom2.o ${MYLIBS} $L

snpLocType: snpLocType.o ${MYLIBS}
	${CC} -O -o snpLocType snpLocType.o ${MYLIBS} $L

snpLocType125: snpLocType125.o ${MYLIBS}
	${CC} -O -o snpLocType125 snpLocType125.o ${MYLIBS} $L

snpExpandAllele: snpExpandAllele.o ${MYLIBS}
	${CC} -O -o snpExpandAllele snpExpandAllele.o ${MYLIBS} $L

snpSort: snpSort.o ${MYLIBS}
	${CC} -O -o snpSort snpSort.o ${MYLIBS} $L

snpRefUCSC: snpRefUCSC.o ${MYLIBS}
	${CC} -O -o snpRefUCSC snpRefUCSC.o ${MYLIBS} $L

snpCheckAlleles: snpCheckAlleles.o ${MYLIBS}
	${CC} -O -o snpCheckAlleles snpCheckAlleles.o ${MYLIBS} $L

snpLoadFasta: snpLoadFasta.o ${MYLIBS}
	${CC} -O -o snpLoadFasta snpLoadFasta.o ${MYLIBS} $L

snpReadFasta: snpReadFasta.o ${MYLIBS}
	${CC} -O -o snpReadFasta snpReadFasta.o ${MYLIBS} $L

snpCheckClassAndObserved: snpCheckClassAndObserved.o ${MYLIBS}
	${CC} -O -o snpCheckClassAndObserved snpCheckClassAndObserved.o ${MYLIBS} $L

snpFunction: snpFunction.o ${MYLIBS}
	${CC} -O -o snpFunction snpFunction.o ${MYLIBS} $L

snpSNP: snpSNP.o ${MYLIBS}
	${CC} -O -o snpSNP snpSNP.o ${MYLIBS} $L

snpFinalTable: snpFinalTable.o ${MYLIBS}
	${CC} -O -o snpFinalTable snpFinalTable.o ${MYLIBS} $L

snpMultiple: snpMultiple.o ${MYLIBS}
	${CC} -O -o snpMultiple snpMultiple.o ${MYLIBS} $L

snpCompareLoctype: snpCompareLoctype.o ${MYLIBS}
	${CC} -O -o snpCompareLoctype snpCompareLoctype.o ${MYLIBS} $L

snpMissing: snpMissing.o ${MYLIBS}
	${CC} -O -o snpMissing snpMissing.o ${MYLIBS} $L

snpPAR: snpPAR.o ${MYLIBS}
	${CC} -O -o snpPAR snpPAR.o ${MYLIBS} $L

illuminaLookup: illuminaLookup.o ${MYLIBS}
	${CC} -O -o illuminaLookup illuminaLookup.o ${MYLIBS} $L

snpCompareWeight: snpCompareWeight.o ${MYLIBS}
	${CC} -O -o snpCompareWeight snpCompareWeight.o ${MYLIBS} $L

subSnpCondense: subSnpCondense.o ${MYLIBS}
	${CC} -O -o subSnpCondense subSnpCondense.o ${MYLIBS} $L

snpClassAndObserved: snpClassAndObserved.o ${MYLIBS}
	${CC} -O -o snpClassAndObserved snpClassAndObserved.o ${MYLIBS} $L

snpMoltype: snpMoltype.o ${MYLIBS}
	${CC} -O -o snpMoltype snpMoltype.o ${MYLIBS} $L

affyLookup: affyLookup.o ${MYLIBS}
	${CC} -O -o affyLookup affyLookup.o ${MYLIBS} $L

cnpLookup: cnpLookup.o ${MYLIBS}
	${CC} -O -o cnpLookup cnpLookup.o ${MYLIBS} $L

fetchSeq: fetchSeq.o ${MYLIBS}
	${CC} -O -o fetchSeq fetchSeq.o ${MYLIBS} $L

snpGetSimple: snpGetSimple.o ${MYLIBS}
	${CC} -O -o snpGetSimple snpGetSimple.o ${MYLIBS} $L

snpOrthoStats: snpOrthoStats.o ${MYLIBS}
	${CC} -O -o snpOrthoStats snpOrthoStats.o ${MYLIBS} $L

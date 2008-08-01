include ../../../inc/common.mk

L = ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

A = snpNcbiToUcsc
${A}: ${A}.o ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/${A} ${A}.o ${MYLIBS} $L
	${STRIP} ${BINDIR}/${A}${EXE}

O = affyLookup.o cnpLookup.o fetchSeq.o \
    hapmap1.o hapmap2.o hapmapLookup.o hapmapMixed.o \
    hapmapOrtho.o hapmapSummary.o hapmapValidate.o \
    illuminaLookup.o \
    illuminaLookup2.o \
    illuminaLookup1M.o \
    snpCheckAlleles.o snpCheckClassAndObserved.o snpCheckCluster.o snpCheckCluster2.o \
    snpClassAndObserved.o snpCleanSeq.o snpCompareLoctype.o snpCompareWeight.o \
    snpContigLocFilter.o snpContigLocFilter125.o \
    snpContigLocusIdCondense.o snpContigLocusIdFilter.o snpContigLocusIdFilter125.o \
    snpExpandAllele.o snpFinalTable.o snpFreq.o snpFunction.o \
    snpGetDeletions.o snpGetInsertions.o snpGetSeqDup.o snpGetSimple.o \
    snpLoadFasta.o snpLocType.o snpLocType125.o \
    snpMissing.o snpMoltype.o snpMultiple.o \
    snpOrtho.o snpOrthoJoin.o \
    snpOrthoLookup.o snpPAR.o \
    snpReadFasta.o snpReadSeq.o snpReadSeq2.o snpRefUCSC.o \
    snpSort.o snpSplitByChrom.o snpSplitByChrom2.o snpSNP.o \
    subSnpCondense.o trimHeader.o

all: affyLookup cnpLookup fetchSeq \
     hapmap1 hapmap2 hapmapLookup hapmapMixed \
     hapmapOrtho hapmapSummary hapmapValidate \
     illuminaLookup \
     illuminaLookup2 \
     illuminaLookup1M \
     snpCheckAlleles snpCheckClassAndObserved snpCheckCluster snpCheckCluster2 \
     snpClassAndObserved snpCleanSeq snpCompareLoctype snpCompareWeight \
     snpContigLocFilter snpContigLocFilter125 \
     snpContigLocusIdCondense snpContigLocusIdFilter snpContigLocusIdFilter125 \
     snpExpandAllele snpFinalTable snpFreq snpFunction \
     snpGetDeletions snpGetInsertions snpGetSeqDup snpGetSimple \
     snpLoadFasta snpLocType snpLocType125 \
     snpMissing snpMoltype snpMultiple \
     snpOrtho snpOrthoJoin \
     snpOrthoLookup snpPAR \
     snpReadFasta snpReadSeq snpReadSeq2 snpRefUCSC \
     snpSort snpSplitByChrom snpSplitByChrom2 snpSNP \
     subSnpCondense trimHeader

affyLookup: affyLookup.o ${MYLIBS}
	${CC} -O -o affyLookup affyLookup.o ${MYLIBS} $L

cnpLookup: cnpLookup.o ${MYLIBS}
	${CC} -O -o cnpLookup cnpLookup.o ${MYLIBS} $L

fetchSeq: fetchSeq.o ${MYLIBS}
	${CC} -O -o fetchSeq fetchSeq.o ${MYLIBS} $L

hapmap1: hapmap1.o ${MYLIBS}
	${CC} -O -o hapmap1 hapmap1.o ${MYLIBS} $L

hapmap2: hapmap2.o ${MYLIBS}
	${CC} -O -o hapmap2 hapmap2.o ${MYLIBS} $L

hapmapLookup: hapmapLookup.o ${MYLIBS}
	${CC} -O -o hapmapLookup hapmapLookup.o ${MYLIBS} $L

hapmapMixed: hapmapMixed.o ${MYLIBS}
	${CC} -O -o hapmapMixed hapmapMixed.o ${MYLIBS} $L

hapmapOrtho: hapmapOrtho.o ${MYLIBS}
	${CC} -O -o hapmapOrtho hapmapOrtho.o ${MYLIBS} $L

hapmapSummary: hapmapSummary.o ${MYLIBS}
	${CC} -O -o hapmapSummary hapmapSummary.o ${MYLIBS} $L

hapmapValidate: hapmapValidate.o ${MYLIBS}
	${CC} -O -o hapmapValidate hapmapValidate.o ${MYLIBS} $L

illuminaLookup: illuminaLookup.o ${MYLIBS}
	${CC} -O -o illuminaLookup illuminaLookup.o ${MYLIBS} $L

illuminaLookup2: illuminaLookup2.o ${MYLIBS}
	${CC} -O -o illuminaLookup2 illuminaLookup2.o ${MYLIBS} $L

illuminaLookup1M: illuminaLookup1M.o ${MYLIBS}
	${CC} -O -o illuminaLookup1M illuminaLookup1M.o ${MYLIBS} $L

snpCheckAlleles: snpCheckAlleles.o ${MYLIBS}
	${CC} -O -o snpCheckAlleles snpCheckAlleles.o ${MYLIBS} $L

snpCheckClassAndObserved: snpCheckClassAndObserved.o ${MYLIBS}
	${CC} -O -o snpCheckClassAndObserved snpCheckClassAndObserved.o ${MYLIBS} $L

snpCheckCluster: snpCheckCluster.o ${MYLIBS}
	${CC} -O -o snpCheckCluster snpCheckCluster.o ${MYLIBS} $L

snpCheckCluster2: snpCheckCluster2.o ${MYLIBS}
	${CC} -O -o snpCheckCluster2 snpCheckCluster2.o ${MYLIBS} $L

snpClassAndObserved: snpClassAndObserved.o ${MYLIBS}
	${CC} -O -o snpClassAndObserved snpClassAndObserved.o ${MYLIBS} $L

snpCleanSeq: snpCleanSeq.o ${MYLIBS}
	${CC} -O -o snpCleanSeq snpCleanSeq.o ${MYLIBS} $L

snpCompareLoctype: snpCompareLoctype.o ${MYLIBS}
	${CC} -O -o snpCompareLoctype snpCompareLoctype.o ${MYLIBS} $L

snpCompareWeight: snpCompareWeight.o ${MYLIBS}
	${CC} -O -o snpCompareWeight snpCompareWeight.o ${MYLIBS} $L

snpContigLocFilter: snpContigLocFilter.o ${MYLIBS}
	${CC} -O -o snpContigLocFilter snpContigLocFilter.o ${MYLIBS} $L

snpContigLocFilter125: snpContigLocFilter125.o ${MYLIBS}
	${CC} -O -o snpContigLocFilter125 snpContigLocFilter125.o ${MYLIBS} $L

snpContigLocusIdCondense: snpContigLocusIdCondense.o ${MYLIBS}
	${CC} -O -o snpContigLocusIdCondense snpContigLocusIdCondense.o ${MYLIBS} $L

snpContigLocusIdFilter: snpContigLocusIdFilter.o ${MYLIBS}
	${CC} -O -o snpContigLocusIdFilter snpContigLocusIdFilter.o ${MYLIBS} $L

snpContigLocusIdFilter125: snpContigLocusIdFilter125.o ${MYLIBS}
	${CC} -O -o snpContigLocusIdFilter125 snpContigLocusIdFilter125.o ${MYLIBS} $L

snpExpandAllele: snpExpandAllele.o ${MYLIBS}
	${CC} -O -o snpExpandAllele snpExpandAllele.o ${MYLIBS} $L

snpFinalTable: snpFinalTable.o ${MYLIBS}
	${CC} -O -o snpFinalTable snpFinalTable.o ${MYLIBS} $L

snpFreq: snpFreq.o ${MYLIBS}
	${CC} -O -o snpFreq snpFreq.o ${MYLIBS} $L

snpFunction: snpFunction.o ${MYLIBS}
	${CC} -O -o snpFunction snpFunction.o ${MYLIBS} $L

snpGetDeletions: snpGetDeletions.o ${MYLIBS}
	${CC} -O -o snpGetDeletions snpGetDeletions.o ${MYLIBS} $L

snpGetInsertions: snpGetInsertions.o ${MYLIBS}
	${CC} -O -o snpGetInsertions snpGetInsertions.o ${MYLIBS} $L

snpGetSeqDup: snpGetSeqDup.o ${MYLIBS}
	${CC} -O -o snpGetSeqDup snpGetSeqDup.o ${MYLIBS} $L

snpGetSimple: snpGetSimple.o ${MYLIBS}
	${CC} -O -o snpGetSimple snpGetSimple.o ${MYLIBS} $L

snpLoadFasta: snpLoadFasta.o ${MYLIBS}
	${CC} -O -o snpLoadFasta snpLoadFasta.o ${MYLIBS} $L

snpLocType: snpLocType.o ${MYLIBS}
	${CC} -O -o snpLocType snpLocType.o ${MYLIBS} $L

snpLocType125: snpLocType125.o ${MYLIBS}
	${CC} -O -o snpLocType125 snpLocType125.o ${MYLIBS} $L

snpMissing: snpMissing.o ${MYLIBS}
	${CC} -O -o snpMissing snpMissing.o ${MYLIBS} $L

snpMoltype: snpMoltype.o ${MYLIBS}
	${CC} -O -o snpMoltype snpMoltype.o ${MYLIBS} $L

snpMultiple: snpMultiple.o ${MYLIBS}
	${CC} -O -o snpMultiple snpMultiple.o ${MYLIBS} $L

snpOrtho: snpOrtho.o ${MYLIBS}
	${CC} -O -o snpOrtho snpOrtho.o ${MYLIBS} $L

snpOrthoJoin: snpOrthoJoin.o ${MYLIBS}
	${CC} -O -o snpOrthoJoin snpOrthoJoin.o ${MYLIBS} $L

snpOrthoLookup: snpOrthoLookup.o ${MYLIBS}
	${CC} -O -o snpOrthoLookup snpOrthoLookup.o ${MYLIBS} $L

snpPAR: snpPAR.o ${MYLIBS}
	${CC} -O -o snpPAR snpPAR.o ${MYLIBS} $L

snpReadFasta: snpReadFasta.o ${MYLIBS}
	${CC} -O -o snpReadFasta snpReadFasta.o ${MYLIBS} $L

snpReadSeq: snpReadSeq.o ${MYLIBS}
	${CC} -O -o snpReadSeq snpReadSeq.o ${MYLIBS} $L

snpReadSeq2: snpReadSeq2.o ${MYLIBS}
	${CC} -O -o snpReadSeq2 snpReadSeq2.o ${MYLIBS} $L

snpRefUCSC: snpRefUCSC.o ${MYLIBS}
	${CC} -O -o snpRefUCSC snpRefUCSC.o ${MYLIBS} $L

snpSort: snpSort.o ${MYLIBS}
	${CC} -O -o snpSort snpSort.o ${MYLIBS} $L

snpSplitByChrom: snpSplitByChrom.o ${MYLIBS}
	${CC} -O -o snpSplitByChrom snpSplitByChrom.o ${MYLIBS} $L

snpSplitByChrom2: snpSplitByChrom2.o ${MYLIBS}
	${CC} -O -o snpSplitByChrom2 snpSplitByChrom2.o ${MYLIBS} $L

snpSNP: snpSNP.o ${MYLIBS}
	${CC} -O -o snpSNP snpSNP.o ${MYLIBS} $L

subSnpCondense: subSnpCondense.o ${MYLIBS}
	${CC} -O -o subSnpCondense subSnpCondense.o ${MYLIBS} $L

trimHeader: trimHeader.o ${MYLIBS}
	${CC} -O -o trimHeader trimHeader.o ${MYLIBS} $L


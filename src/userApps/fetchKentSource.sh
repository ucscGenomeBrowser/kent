#!/bin/bash
#
# exit on any error:
set -beEu -o pipefail

# for version 286 we will use beta branch instead of HEAD
# export branch="beta"

export branch="HEAD"

# script to fetch extra source to use with the kent build,
# and then selectively parts of the kent source tree, enough to
# build just the user utilities

# the combined samtabix source for the SAM/BAM/TABIX functions:
rm -fr samtabix
echo "fetch samtabix" 1>&2
git clone http://genome-source.cse.ucsc.edu/samtabix.git samtabix \
  > /dev/null 2>&1

# These selective git archive commands only work up to a certain size
# of fetched source (number of arguments), hence the multiple set of
# individual fetches to get all the parts

rm -f part1Src.zip part2Src.zip part3Src.zip part4Src.zip part5Src.zip
export partNumber=1
export ofN="of 5"

echo "fetch kent source part ${partNumber} ${ofN}" 1>&2
git archive --format=zip -9 --remote=git://genome-source.cse.ucsc.edu/kent.git \
--prefix=kent/ ${branch} \
src/machTest.sh \
src/ameme \
src/aladdin \
src/blat \
src/dnaDust \
src/fuse \
src/gfClient \
src/gfServer \
src/index \
src/makefile \
src/meta \
src/parasol \
src/primeMate \
src/product \
src/protDust \
src/weblet \
src/inc \
src/utils \
src/jkOwnLib \
src/lib \
src/hg/affyTransciptome \
src/hg/agpAllToFaFile \
src/hg/agpCloneCheck \
src/hg/agpCloneList \
src/hg/agpToFa \
src/hg/agpToGl \
src/hg/altSplice \
src/hg/autoDtd \
src/hg/autoSql \
src/hg/autoXml \
src/hg/bedIntersect \
src/hg/bedItemOverlapCount \
src/hg/bedOrBlocks \
src/hg/bedSort \
src/hg/bedSplitOnChrom \
src/hg/bedToGenePred \
src/hg/blastToPsl \
src/hg/borfBig \
src/hg/checkCoverageGaps \
src/hg/checkHgFindSpec \
src/hg/checkTableCoords \
src/hg/ctgFaToFa \
src/hg/ctgToChromFa \
src/hg/dbTrash \
src/hg/embossToPsl \
src/hg/estOrient \
src/hg/encode/validateFiles \
src/hg/fakeFinContigs \
src/hg/fakeOut \
src/hg/featureBits \
src/hg/ffaToFa \
src/hg/fishClones \
src/hg/fqToQa \
src/hg/fqToQac \
src/hg/fragPart \
src/hg/gbGetEntries \
src/hg/gbOneAcc > part${partNumber}Src.zip

unzip -o -q part${partNumber}Src.zip

((partNumber++))
echo "fetch kent source part ${partNumber} ${ofN}" 1>&2

git archive --format=zip -9 --remote=git://genome-source.cse.ucsc.edu/kent.git \
--prefix=kent/ ${branch} \
src/hg/gbToFaRa \
src/hg/geneBounds \
src/hg/genePredHisto \
src/hg/genePredSingleCover \
src/hg/genePredToBed \
src/hg/genePredToFakePsl \
src/hg/genePredToGtf \
src/hg/genePredToMafFrames \
src/hg/getFeatDna \
src/hg/getRna \
src/hg/getRnaPred \
src/hg/gpStats \
src/hg/gpToGtf \
src/hg/gpcrParser \
src/hg/gsBig \
src/hg/hgChroms \
src/hg/hgGetAnn \
src/hg/hgKnownGeneList \
src/hg/hgSelect \
src/hg/hgSpeciesRna \
src/hg/hgTablesTest \
src/hg/hgsql \
src/hg/hgsqlLocal \
src/hg/hgsqlSwapTables \
src/hg/hgsqlTableDate \
src/hg/hgsqladmin \
src/hg/hgsqldump \
src/hg/hgsqldumpLocal \
src/hg/hgsqlimport \
src/hg/inc \
src/hg/intronEnds \
src/hg/lib \
src/hg/lfsOverlap \
src/hg/liftAcross \
src/hg/liftAgp \
src/hg/liftFrags \
src/hg/liftUp \
src/hg/liftOver \
src/hg/makefile \
src/hg/makeDb/hgLoadWiggle \
src/hg/makeDb/hgGcPercent \
src/hg/utils \
src/hg/maskOutFa \
src/hg/mdToNcbiLift \
src/hg/mrnaToGene \
src/hg/orthoMap \
src/hg/patCount \
src/hg/perf \
src/hg/pslCat \
src/hg/pslCheck \
src/hg/pslCoverage \
src/hg/pslCDnaFilter \
src/hg/pslPretty \
src/hg/pslReps \
src/hg/pslSort \
src/hg/pslDropOverlap > part${partNumber}Src.zip

unzip -o -q part${partNumber}Src.zip

((partNumber++))
echo "fetch kent source part ${partNumber} ${ofN}" 1>&2

git archive --format=zip -9 --remote=git://genome-source.cse.ucsc.edu/kent.git \
--prefix=kent/ ${branch} \
src/hg/pslFilter \
src/hg/pslFilterPrimers \
src/hg/pslGlue \
src/hg/pslHisto \
src/hg/pslHitPercent \
src/hg/pslIntronsOnly \
src/hg/pslPairs \
src/hg/pslPartition \
src/hg/pslQuickFilter \
src/hg/pslRecalcMatch \
src/hg/pslSelect \
src/hg/pslSimp \
src/hg/pslSortAcc \
src/hg/pslSplitOnTarget \
src/hg/pslStats \
src/hg/pslSwap \
src/hg/pslToBed \
src/hg/pslUnpile \
src/hg/pslxToFa \
src/hg/qa \
src/hg/qaToQac \
src/hg/qacAgpLift \
src/hg/qacToQa \
src/hg/qacToWig \
src/hg/ratStuff/mafsInRegion \
src/hg/ratStuff/mafSpeciesSubset \
src/hg/recycleDb \
src/hg/relPairs \
src/hg/reviewSanity \
src/hg/rnaStructure \
src/hg/scanRa \
src/hg/semiNorm \
src/hg/sim4big \
src/hg/snp \
src/hg/snpException \
src/hg/spideyToPsl \
src/hg/encode3 \
src/hg/splitFa \
src/hg/splitFaIntoContigs \
src/hg/sqlEnvTest.sh \
src/hg/sqlToXml \
src/hg/test \
src/hg/trfBig \
src/hg/txCds \
src/hg/txGene \
src/hg/txGraph \
src/hg/uniqSize \
src/hg/updateStsInfo \
src/hg/xmlCat \
src/hg/xmlToSql \
src/hg/hgTables \
src/hg/near \
src/hg/pslDiff \
src/hg/sage \
src/hg/gigAssembler/checkAgpAndFa \
src/hg/genePredCheck > part${partNumber}Src.zip

unzip -o -q part${partNumber}Src.zip

((partNumber++))
echo "fetch kent source part ${partNumber} ${ofN}" 1>&2

git archive --format=zip -9 --remote=git://genome-source.cse.ucsc.edu/kent.git \
--prefix=kent/ ${branch} \
src/hg/makeDb/makefile \
src/hg/makeDb/hgAar \
src/hg/makeDb/hgAddLiftOverChain \
src/hg/makeDb/hgBbiDbLink \
src/hg/makeDb/hgClonePos \
src/hg/makeDb/hgCountAlign \
src/hg/makeDb/hgCtgPos \
src/hg/makeDb/hgDeleteChrom \
src/hg/makeDb/hgExperiment \
src/hg/makeDb/hgExtFileCheck \
src/hg/makeDb/hgFakeAgp \
src/hg/makeDb/hgFindSpec \
src/hg/makeDb/hgGeneBands \
src/hg/makeDb/hgGenericMicroarray \
src/hg/makeDb/hgPar \
src/hg/makeDb/hgGoldGapGl \
src/hg/makeDb/hgKgGetText \
src/hg/makeDb/hgKgMrna \
src/hg/makeDb/hgKnownMore \
src/hg/makeDb/hgKnownMore.oo21 \
src/hg/makeDb/hgLoadBed \
src/hg/makeDb/hgLoadBlastTab \
src/hg/makeDb/hgLoadChain \
src/hg/makeDb/hgLoadChromGraph \
src/hg/makeDb/hgLoadGenePred \
src/hg/makeDb/hgLoadItemAttr \
src/hg/makeDb/hgLoadMaf \
src/hg/makeDb/hgLoadMafFrames \
src/hg/makeDb/hgLoadNet \
src/hg/makeDb/hgLoadOut \
src/hg/makeDb/hgLoadPsl \
src/hg/makeDb/hgLoadSeq \
src/hg/makeDb/hgLoadSample \
src/hg/makeDb/hgLoadSqlTab \
src/hg/makeDb/hgMapMicroarray \
src/hg/makeDb/hgMedianMicroarray \
src/hg/makeDb/hgNibSeq \
src/hg/makeDb/hgPepPred \
src/hg/makeDb/hgRatioMicroarray \
src/hg/makeDb/hgDropSplitTable \
src/hg/makeDb/hgRenameSplitTable \
src/hg/makeDb/hgSanger20 \
src/hg/makeDb/hgSanger22 \
src/hg/makeDb/hgStanfordMicroarray \
src/hg/makeDb/hgStsAlias \
src/hg/makeDb/hgStsMarkers \
src/hg/makeDb/hgTomRough \
src/hg/makeDb/hgTpf \
src/hg/makeDb/hgTraceInfo \
src/hg/makeDb/hgTrackDb \
src/hg/makeDb/hgWaba \
src/hg/makeDb/ldHgGene \
src/hg/makeDb/hgMrnaRefseq \
src/hg/makeDb/schema \
src/hg/makeDb/tfbsConsLoc \
src/hg/makeDb/tfbsConsSort > part${partNumber}Src.zip

unzip -o -q part${partNumber}Src.zip

((partNumber++))
echo "fetch kent source part ${partNumber} ${ofN}" 1>&2

git archive --format=zip -9 --remote=git://genome-source.cse.ucsc.edu/kent.git \
--prefix=kent/ ${branch} \
src/parasol \
src/hg/pslToChain \
src/hg/makeDb/outside \
src/hg/makeDb/trackDbRaFormat \
src/hg/makeDb/trackDbPatch \
src/hg/mouseStuff \
src/hg/ratStuff \
src/hg/nci60 \
src/hg/visiGene/knownToVisiGene \
src/hg/visiGene/hgVisiGene > part${partNumber}Src.zip

unzip -o -q part${partNumber}Src.zip

#!/bin/bash
#
# exit on any error:
set -beEu -o pipefail

# script to fetch extra source to use with the kent build,
# and then selectively parts of the kent source tree, enough to
# build just the user utilities

# the combined samtabix source for the SAM/BAM/TABIX functions:
rm -fr samtabix
echo "fetch samtabix" 1>&2
git clone http://genome-source.cse.ucsc.edu/samtabix.git samtabix \
  > /dev/null 2>&1

# These selective git archive commands only work up to a certain size
# of fetched source, hence the multiple set of individual fetches to
# get all the parts

rm -f part1Src.zip part2Src.zip part3Src.zip part4Src.zip
export partNumber=1
export ofN="of 6"

echo "fetch kent source part ${partNumber} ${ofN}" 1>&2
git archive --format=zip -9 --remote=git://genome-source.cse.ucsc.edu/kent.git \
--prefix=kent/ HEAD \
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
src/hg/gbOneAcc \
src/hg/gbToFaRa > part${partNumber}Src.zip

unzip -o -q part${partNumber}Src.zip

((partNumber++))
echo "fetch kent source part ${partNumber} ${ofN}" 1>&2

git archive --format=zip -9 --remote=git://genome-source.cse.ucsc.edu/kent.git \
--prefix=kent/ HEAD \
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

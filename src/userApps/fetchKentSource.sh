#!/bin/bash
#
# script to fetch extra source to use with the kent build,
# and then selectively parts of the kent source tree, enough to
# build just the user utilities

# the combined samtabix source for the SAM/BAM/TABIX functions:
echo "fetch samtabix" 1>&2
git clone http://genome-source.cse.ucsc.edu/samtabix.git samtabix

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
src/cdnaAli \
src/dnaDust \
src/fuse \
src/getgene \
src/gfClient \
src/gfServer \
src/idbQuery \
src/index \
src/makefile \
src/meta \
src/primeMate \
src/product \
src/protDust \
src/reformat \
src/scanIntrons \
src/tracks \
src/weblet \
src/wormAli \
src/xenoAli \
src/inc \
src/utils \
src/jkOwnLib \
src/lib > part${partNumber}.zip

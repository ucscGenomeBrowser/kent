# Jonathan - began 2024-02-26

export db=mm39
export GENCODE_VERSION=VM34
export PREV_GENCODE_VERSION=VM33
screen -S knownGene${GENCODE_VERSION}
mkdir /hive/data/genomes/$db/bed/gencode$GENCODE_VERSION/build
cd /hive/data/genomes/$db/bed/gencode$GENCODE_VERSION/build

PATH=$HOME/kent/src/hg/utils/otto/knownGene":$PATH"
cp /hive/data/genomes/${db}/bed/gencode${PREV_GENCODE_VERSION}/build/buildEnv.sh  buildEnv.sh

# edit buildEnv.sh
 . buildEnv.sh

cp ${oldGeneDir}/${PREV_GENCODE_VERSION}.files.txt .
# This failed on account of the V32 file was missing.  I reconstructed it and the V33 version by hand.

cp ${oldGeneDir}/${PREV_GENCODE_VERSION}.tables.txt .

hgsql ${oldKnownDb} -Ne "show tables" > ${oldKnownDb}.tables.txt
diff <(sort ${PREV_GENCODE_VERSION}.tables.txt) <(sort ${oldKnownDb}.tables.txt)
# no difference

buildKnown.sh &
# wait for completion

tail -n 1 *.log
# ==> doBioCyc.log <==
# BuildBioCyc successfully finished
#
# ==> doBlast.log <==
# BuildBlast successfully finished
#
# ==> doFoldUtr.log <==
# BuildFoldUtr successfully finished
#
# ==> doKnown.log <==
# BuildKnown successfully finished
#
# ==> doKnownTo.log <==
# Expecting 2 words line 136223 of refToLl.txt got 1.
#
# ==> doPfamScop.log <==
# BuildPfamScop successfully finished

# So something went wrong with doKnownTo.
# Turned out it was because ncbiRefSeqLink now has records without locusLinkId
# populated, which was unexpected.  Since LocusLink is now discontinued, we'll
# have to look into what purpose this still serves.  Skipping over those
# records for now was enough to permit completion.
# The build is now finished.

# Current next steps: Adding IsPcr server, double check all.joiner, check in logs,
# update tickets & tables. (all in knownGene build wiki page).  Then do post-release
# "push other species" blast tables

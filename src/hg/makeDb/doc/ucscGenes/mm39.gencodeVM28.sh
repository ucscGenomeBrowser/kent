export db=mm39
export GENCODE_VERSION=VM28
export PREV_GENCODE_VERSION=VM27
screen -S knownGene${GENCODE_VERSION}
mkdir /hive/data/genomes/$db/bed/gencode$GENCODE_VERSION/build
cd /hive/data/genomes/$db/bed/gencode$GENCODE_VERSION/build

PATH=$HOME/kent/src/hg/utils/otto/knownGene":$PATH"
cp /hive/data/genomes/${db}/bed/gencode${PREV_GENCODE_VERSION}/build/buildEnv.sh  buildEnv.sh

# edit buildEnv.sh
 . buildEnv.sh

cp ${oldGeneDir}/${PREV_GENCODE_VERSION}.files.txt  ${GENCODE_VERSION}.files.txt
cp ${oldGeneDir}/${PREV_GENCODE_VERSION}.tables.txt  ${GENCODE_VERSION}.tables.txt

hgsql ${oldKnownDb} -Ne "show tables"

buildKnown.sh &

screen -S knownGeneV39
mkdir /hive/data/genomes/$db/bed/gencode$GENCODE_VERSION/build
cd /hive/data/genomes/$db/bed/gencode$GENCODE_VERSION/build
PATH=$HOME/kent/src/hg/utils/otto/knownGene":$PATH"
cp /hive/data/genomes/hg38/bed/gencodeV38/build/buildEnv.sh  buildEnv.sh

# edit buildEnv.sh

cp ${oldGeneDir}/${PREV_GENCODE_VERSION}.files.txt  ${GENCODE_VERSION}.files.txt
cp ${oldGeneDir}/${PREV_GENCODE_VERSION}.tables.txt  ${GENCODE_VERSION}.tables.txt

buildKnown.sh &

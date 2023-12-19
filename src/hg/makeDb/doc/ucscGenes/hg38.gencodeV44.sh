export db=hg38
export GENCODE_VERSION=V44
export PREV_GENCODE_VERSION=V43
screen -S knownGene${GENCODE_VERSION}
mkdir /hive/data/genomes/$db/bed/gencode$GENCODE_VERSION/build
cd /hive/data/genomes/$db/bed/gencode$GENCODE_VERSION/build
PATH=$HOME/kent/src/hg/utils/otto/knownGene":$PATH"
cp /hive/data/genomes/${db}/bed/gencode${PREV_GENCODE_VERSION}/build/buildEnv.sh  buildEnv.sh

# edit buildEnv.sh
. buildEnv.sh

# NB: Had to manually construct the files.txt file; it was missing.
cp ${oldGeneDir}/${PREV_GENCODE_VERSION}.files.txt .
cp ${oldGeneDir}/${PREV_GENCODE_VERSION}.tables.txt .

hgsql ${oldKnownDb} -Ne "show tables" > ${oldKnownDb}.tables.txt
diff <(sort ${PREV_GENCODE_VERSION}.tables.txt) <(sort ${oldKnownDb}.tables.txt)
# no difference

buildKnown.sh &

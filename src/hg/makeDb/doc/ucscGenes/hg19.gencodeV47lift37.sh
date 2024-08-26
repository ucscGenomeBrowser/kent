export db=hg19
export GENCODE_VERSION=V47lift37
export PREV_GENCODE_VERSION=V45lift37
screen -S knownGene${GENCODE_VERSION}
mkdir /hive/data/genomes/$db/bed/gencode$GENCODE_VERSION/build
cd /hive/data/genomes/$db/bed/gencode$GENCODE_VERSION/build
PATH=$HOME/kent/src/hg/utils/otto/knownGene":$PATH"
cp /hive/data/genomes/${db}/bed/gencode${PREV_GENCODE_VERSION}/build/buildEnv.sh  buildEnv.sh

# edit buildEnv.sh
. buildEnv.sh

# files didn't exist; created it based on a template from V47 on hg38
cp ${oldGeneDir}/${PREV_GENCODE_VERSION}.files.txt .
cp ${oldGeneDir}/${PREV_GENCODE_VERSION}.tables.txt .

hgsql ${oldKnownDb} -Ne "show tables" > ${oldKnownDb}.tables.txt
diff <(sort ${PREV_GENCODE_VERSION}.tables.txt) <(sort ${oldKnownDb}.tables.txt)
# no difference

buildKnown.sh &

# Continue with the steps to load the tables into the database.

# make sure knownGeneOld${PREV_GENCODE_VERSION} is populated with the previous knownGene
# contents, and that the track and HTML page are updated accordingly.  This step should be
# obsolete soon.

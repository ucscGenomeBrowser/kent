export db=hg38
export GENCODE_VERSION=V48
export PREV_GENCODE_VERSION=V47
screen -S knownGene${GENCODE_VERSION}
mkdir /hive/data/genomes/$db/bed/gencode$GENCODE_VERSION/build
cd /hive/data/genomes/$db/bed/gencode$GENCODE_VERSION/build
PATH=$HOME/kent/src/hg/utils/otto/knownGene":$PATH"
cp /hive/data/genomes/${db}/bed/gencode${PREV_GENCODE_VERSION}/build/buildEnv.sh  buildEnv.sh

# edit buildEnv.sh
. buildEnv.sh

cp ${oldGeneDir}/${PREV_GENCODE_VERSION}.files.txt .
cp ${oldGeneDir}/${PREV_GENCODE_VERSION}.tables.txt .

hgsql ${oldKnownDb} -Ne "show tables" > ${oldKnownDb}.tables.txt
diff <(sort ${PREV_GENCODE_VERSION}.tables.txt) <(sort ${oldKnownDb}.tables.txt)
# no difference

buildKnown.sh &

# Continue with the steps to load the tables into the database.

# After that's done, then can do the myGene2 knownTo (hasn't been integrated because it needs
# manual gene list fetch).
#myGene2
mkdir $dir/myGene2
cd $dir/myGene2

# copy list of genes to a file from https://mygene2.org/MyGene2/genes
awk '{print $1}' thatfile | sort > genes.lst
hgsql hg38 -Ne "select geneSymbol, kgId from kgXref" | sort > ids.txt
join -t $'\t' genes.lst  ids.txt | tawk '{print $2,$1}' | sort > knownToMyGene2.txt
hgLoadSqlTab $db knownToMyGene2 ~/kent/src/hg/lib/knownTo.sql knownToMyGene2.txt

# make sure knownGeneOld${PREV_GENCODE_VERSION} is populated with the previous knownGene
# contents, and that the track and HTML page are updated accordingly.  This step should be
# obsolete soon.

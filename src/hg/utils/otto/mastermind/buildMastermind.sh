#wget 'https://mastermind.genomenon.com/cvr/download?format=csv' -O - > mastermind.2018.11.26.csv.gz
#unzip mastermind.2018.11.26.csv.zip
#mv mastermind_cited_variants_reference-2018.11.26-csv/ 2018-11-26
#hgsql hg19 -NB -e 'select alias, chrom from chromAlias where source = "refseq";' > chromAlias.tab
#ln -s `pwd`/mastermind.bb /gbdb/hg19/bbi/mastermind.bb
#python mastermindToBed.py 2018-11-26/mastermind_cited_variants_reference-2018.11.26.csv 
#bedSort mastermind.bed mastermind.bed
#bedToBigBed -type=bed9+ -as=mastermind.as -tab mastermind.bed //hive/data/genomes/hg19/chrom.sizes  mastermind.bb

set -e -o pipefail
cd /hive/data/outside/otto/mastermind
date=`date --iso-8601`
mkdir -p hg19 hg38
mkdir -p archive/$date
hgsql hg19 -NB -e 'select alias, chrom from chromAlias where source = "refseq";' > hg19/chromAlias.tab
hgsql hg38 -NB -e 'select alias, chrom from chromAlias where source = "refseq";' > hg38/chromAlias.tab
wget -q 'https://mastermind.genomenon.com/cvr/download?build=grch37&format=csv' -O archive/$date/hg19.zip
wget -q 'https://mastermind.genomenon.com/cvr/download?build=grch38&format=csv' -O archive/$date/hg38.zip
unzip -o archive/$date/hg19.zip -d archive/$date
unzip -o archive/$date/hg38.zip -d archive/$date
python mastermindToBed.py hg19/chromAlias.tab archive/$date/mastermind_cited_variants_reference-*-grch37.csv hg19/mastermind.bed
python mastermindToBed.py hg38/chromAlias.tab archive/$date/mastermind_cited_variants_reference-*-grch38.csv hg38/mastermind.bed
bedSort hg19/mastermind.bed hg19/mastermind.bed
bedSort hg38/mastermind.bed hg38/mastermind.bed
bedToBigBed -type=bed9+ -as=mastermind.as -tab hg19/mastermind.bed /hive/data/genomes/hg19/chrom.sizes  hg19/mastermind.new.bb
bedToBigBed -type=bed9+ -as=mastermind.as -tab hg38/mastermind.bed /hive/data/genomes/hg38/chrom.sizes  hg38/mastermind.new.bb
curl -s https://mastermind.genomenon.com/cvr/version | cut -d\" -f4 > mastermindRelease.new.txt
mv mastermindRelease.new.txt mastermindRelease.txt
mv hg19/mastermind.new.bb hg19/mastermind.bb
mv hg38/mastermind.new.bb hg38/mastermind.bb

# build archive
REL=`cat mastermindRelease.txt`
ARCH=/hive/data/inside/archive
mkdir -p $ARCH/{hg19,hg38}/mastermind/$REL
cp hg19/mastermind.bb $ARCH/hg19/mastermind/$REL/
cp hg38/mastermind.bb $ARCH/hg38/mastermind/$REL/
hgsql hg19 -e 'select * from trackDb where tableName="mastermind"' > $ARCH/hg19/mastermind/$REL/trackDb.tab
hgsql hg38 -e 'select * from trackDb where tableName="mastermind"' > $ARCH/hg38/mastermind/$REL/trackDb.tab
cp archive/$date/LICENSE.pdf $ARCH/hg19/mastermind/$REL/
cp archive/$date/LICENSE.pdf $ARCH/hg38/mastermind/$REL/
echo "These file were derived from Genomenom CVR, see https://www.genomenon.com/cvr/" > $ARCH/hg19/mastermind/$REL/README.txt
echo "See the UCSC track help page for details on update frequency and source code." >> $ARCH/hg19/mastermind/$REL/README.txt
cp $ARCH/hg19/mastermind/$REL/README.txt $ARCH/hg38/mastermind/$REL/README.txt
echo "Mastermind done"

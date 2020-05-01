# This doc assumes that the gencode* tables have been built on $db
db=hg38
GENCODE_VERSION=V32
dir=/hive/data/genomes/$db/bed/mergeGene$GENCODE_VERSION
genomes=/hive/data/genomes
tempDb=braNey38
kent=$HOME/kent

mkdir -p $dir
cd $dir

echo "create database $tempDb" | hgsql ""
echo "create table chromInfo like  $db.chromInfo" | hgsql $tempDb
echo "insert into chromInfo select * from   $db.chromInfo" | hgsql $tempDb

hgsql -e "select * from gencodeAnnot$GENCODE_VERSION" --skip-column-names $db | cut -f 2-16 |  genePredToBed stdin tmp
#hgsql -e "select * from wgEncodeGencodePseudoGene$GENCODE_VERSION" --skip-column-names $db | cut -f 2-16 |  genePredToBed stdin tmp2
sort -k1,1 -k2,2n tmp | gzip -c > gencode${GENCODE_VERSION}.bed.gz

touch oldToNew.tab

# check to make sure we don't have any dups.  These two numbers should
# be the same.   
awk '{print $2}' txToAcc.tab | sed 's/\..*//' | sort -u | wc -l
# 227368
awk '{print $2}' txToAcc.tab | sed 's/\..*//' | sort  | wc -l
# 227368

zcat gencode${GENCODE_VERSION}.bed.gz > ucscGenes.bed
txBedToGraph ucscGenes.bed ucscGenes ucscGenes.txg
txgAnalyze ucscGenes.txg $genomes/$db/$db.2bit stdout | sort | uniq | bedClip stdin /cluster/data/hg38/chrom.sizes  ucscSplice.bed

makeUcscGeneTables -justKnown $db $tempDb $GENCODE_VERSION txToAcc.tab
hgLoadSqlTab -notOnServer $tempDb knownGene $kent/src/hg/lib/knownGene.sql knownGene.gp
hgLoadGenePred -genePredExt $tempDb  knownGeneExt knownGeneExt.gp
hgMapToGene -type=psl -all -tempDb=$tempDb $db all_mrna knownGene knownToMrna
hgMapToGene -tempDb=$tempDb $db refGene knownGene knownToRefSeq
hgMapToGene -type=psl -tempDb=$tempDb $db all_mrna knownGene knownToMrnaSingle
makeUcscGeneTables $db $tempDb $GENCODE_VERSION txToAcc.tab

hgLoadSqlTab -notOnServer $tempDb kgXref $kent/src/hg/lib/kgXref.sql kgXref.tab

# calculate score field with bitfields
hgsql $db -Ne "select * from gencodeAnnot$GENCODE_VERSION" | cut -f 2- | sort > gencodeAnnot$GENCODE_VERSION.txt
hgsql $db -Ne "select name,2 from gencodeAnnot$GENCODE_VERSION" | sort  > knownCanon.txt
hgsql $db -Ne "select * from gencodeTag$GENCODE_VERSION" | grep basic |  sed 's/basic/1/' | sort > knownTag.txt
hgsql $db -Ne "select transcriptId,transcriptClass from gencodeAttrs$GENCODE_VERSION where transcriptClass='pseudo'" |  sed 's/pseudo/4/' > knownAttrs.txt
sort knownCanon.txt knownTag.txt knownAttrs.txt | awk '{if ($1 != last) {print last, sum; sum=$2; last=$1}  else {sum += $2; }} END {print last, sum}' | tail -n +2  > knownScore.txt

#ifdef NOTNOW
cat  << __EOF__  > colors.sed
s/coding/12, 12, 120/
s/nonCoding/0, 100, 0/
s/pseudo/255,51,255/
s/other/254, 0, 0/
__EOF__ 
#endif

hgsql $db -Ne "select * from gencodeAttrs$GENCODE_VERSION" | tawk '{print $4,$10}' | sed -f colors.sed > colors.txt

#ifdef NOTNOW
hgsql $db -Ne "select * from gencodeToUniProt$GENCODE_VERSION" | tawk '{print $1,$2}'|  sort > uniProt.txt
hgsql $db -Ne "select * from gencodeAnnot$GENCODE_VERSION" | tawk '{print $1,$12}' | sort > gene.txt
join -a 1 gene.txt uniProt.txt > geneNames.txt
#endif

//genePredToBigGenePred -score=knownScore.txt  -colors=colors.txt -geneNames=geneNames.txt  -known gencodeAnnot$GENCODE_VERSION.txt  gencodeAnnot$GENCODE_VERSION.bgpInput
genePredToBigGenePred -score=knownScore.txt  -colors=colors.txt gencodeAnnot$GENCODE_VERSION.txt stdout | sort -k1,1 -k2,2n >  gencodeAnnot$GENCODE_VERSION.bgpInput

sort -k 4 gencodeAnnot$GENCODE_VERSION.bgpInput > join1

hgsql $tempDb -Ne "select kgId, description from kgXref"  | sort > joinDescription.txt


join -t $'\t' -1 4 -2 1 join1 joinDescription.txt > join2

hgsql $tempDb -Ne "select r.name, f.summary from hgFixed.refSeqSummary f,knownToRefSeq r where f.mrnaAcc=r.value" | \
    sort > joinRefSeqSummary.txt

join -t $'\t' join2 joinRefSeqSummary.txt > join3

tawk '{printf "%s\t%s\t%s\t%s\t", $2,$3,$4,$1; for(ii=5; ii <= NF; ii++) printf "%s\t",$ii; printf "\n";}' join3 | \
         sed 's/.$//' | sort  -k1,1 -k2,2n > bigBedInput.bed

bedToBigBed -type=bed12+10 -tab -as=$HOME/kent/src/hg/lib/mergedBGP.as bigBedInput.bed /cluster/data/$db/chrom.sizes $db.mergedBGP.bb

mkdir -p /gbdb/$db/mergedBGP
ln -s `pwd`/$db.mergedBGP.bb /gbdb/$db/mergedBGP/mergedBGP.bb

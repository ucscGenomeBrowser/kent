# This doc assumes that the gencode* tables have been built on $db
db=hg38
GENCODE_VERSION=V35
dir=/hive/data/genomes/$db/bed/gencode$GENCODE_VERSION
genomes=/hive/data/genomes
tempDb=knownGeneV35
kent=$HOME/kent

mkdir -p $dir
cd $dir

echo "create database $tempDb" | hgsql ""
echo "create table chromInfo like  $db.chromInfo" | hgsql $tempDb
echo "insert into chromInfo select * from   $db.chromInfo" | hgsql $tempDb

hgsql -e "select * from gencodeAnnot$GENCODE_VERSION" --skip-column-names $db | cut -f 2-16 |  genePredToBed stdin stdout | sort -k1,1 -k2,2n | gzip -c > gencode${GENCODE_VERSION}.bed.gz

touch oldToNew.tab

zcat gencode${GENCODE_VERSION}.bed.gz > ucscGenes.bed
txBedToGraph ucscGenes.bed ucscGenes ucscGenes.txg
txgAnalyze ucscGenes.txg $genomes/$db/$db.2bit stdout | sort | uniq | bedClip stdin /cluster/data/hg38/chrom.sizes  ucscSplice.bed

zcat gencode${GENCODE_VERSION}.bed.gz |  awk '{print $4}' | sort > newGencodeName.txt
hgsql $db -Ne "select name,alignId from knownGene" | sort > oldGenToUcsc.txt
kgAllocId oldGenToUcsc.txt newGencodeName.txt 5070122 stdout | sort -u >  txToAcc.tab
# lastId 5072896

makeUcscGeneTables -justKnown $db $tempDb $GENCODE_VERSION txToAcc.tab
hgLoadSqlTab -notOnServer $tempDb knownGene $kent/src/hg/lib/knownGene.sql knownGene.gp
hgLoadGenePred -genePredExt $tempDb  knownGeneExt knownGeneExt.gp
hgMapToGene -type=psl -all -tempDb=$tempDb $db all_mrna knownGene knownToMrna
hgMapToGene -tempDb=$tempDb $db refGene knownGene knownToRefSeq
hgMapToGene -type=psl -tempDb=$tempDb $db all_mrna knownGene knownToMrnaSingle
makeUcscGeneTables $db $tempDb $GENCODE_VERSION txToAcc.tab
hgLoadSqlTab -notOnServer $tempDb knownCanonical $kent/src/hg/lib/knownCanonical.sql knownCanonical.tab
sort knownIsoforms.tab | uniq | hgLoadSqlTab -notOnServer $tempDb knownIsoforms $kent/src/hg/lib/knownIsoforms.sql stdin
genePredToProt knownGeneExt.gp /cluster/data/$db/$db.2bit tmp.faa
faFilter -uniq tmp.faa ucscGenes.faa
hgPepPred $tempDb generic knownGenePep ucscGenes.faa


hgLoadSqlTab -notOnServer $tempDb kgXref $kent/src/hg/lib/kgXref.sql kgXref.tab

#ifdef NOTNOW
# calculate score field with bitfields
hgsql $db -Ne "select * from gencodeAnnot$GENCODE_VERSION" | cut -f 2- | sort > gencodeAnnot$GENCODE_VERSION.txt
hgsql $db -Ne "select name,2 from gencodeAnnot$GENCODE_VERSION" | sort  > knownCanon.txt
hgsql $db -Ne "select * from gencodeTag$GENCODE_VERSION" | grep basic |  sed 's/basic/1/' | sort > knownTag.txt
hgsql $db -Ne "select transcriptId,transcriptClass from gencodeAttrs$GENCODE_VERSION where transcriptClass='pseudo'" |  sed 's/pseudo/4/' > knownAttrs.txt
sort knownCanon.txt knownTag.txt knownAttrs.txt | awk '{if ($1 != last) {print last, sum; sum=$2; last=$1}  else {sum += $2; }} END {print last, sum}' | tail -n +2  > knownScore.txt
#endif

hgsql -e "select * from gencodeAnnot$GENCODE_VERSION" --skip-column-names $db | cut -f 2-16 |  tawk '{print $1,$13,$14,$8,$15}' | sort | uniq > knownCds.tab
hgLoadSqlTab -notOnServer $tempDb knownCds $kent/src/hg/lib/knownCds.sql knownCds.tab


cat  << __EOF__  > colors.sed
s/coding/12\t12\t120/
s/nonCoding/0\t100\t0/
s/pseudo/255\t51\t255/
s/other/254\t0\t0/
__EOF__

hgsql $db -Ne "select * from gencodeAttrs$GENCODE_VERSION" | tawk '{print $4,$10}' | sed -f colors.sed > colors.txt

#ifdef NOTNOW
hgsql $db -Ne "select * from gencodeToUniProt$GENCODE_VERSION" | tawk '{print $1,$2}'|  sort > uniProt.txt
hgsql $db -Ne "select * from gencodeAnnot$GENCODE_VERSION" | tawk '{print $1,$12}' | sort > gene.txt
join -a 1 gene.txt uniProt.txt > geneNames.txt
#endif

//genePredToBigGenePred -score=knownScore.txt  -colors=colors.txt -geneNames=geneNames.txt  -known gencodeAnnot$GENCODE_VERSION.txt  gencodeAnnot$GENCODE_VERSION.bgpInput
hgsql $db -Ne "select * from gencodeAnnot$GENCODE_VERSION" | cut -f 2- > gencodeAnnot$GENCODE_VERSION.txt
#genePredToBigGenePred -colors=colors.txt gencodeAnnot$GENCODE_VERSION.txt stdout | sort -k1,1 -k2,2n >  gencodeAnnot$GENCODE_VERSION.bgpInput

hgsql $tempDb -Ne "select kgId, geneSymbol, spID from kgXref" > geneNames.txt
#hgsql $tempDb -Ne "select * from knownCds" > knownCds.txt
#genePredToBigGenePred -colors=colors.txt -known knownGene.gp stdout | sort -k1,1 -k2,2n >  gencodeAnnot$GENCODE_VERSION.bgpInput
genePredToBigGenePred -colors=colors.txt -geneNames=geneNames.txt -known -cds=knownCds.tab   knownGene.gp  stdout | sort -k1,1 -k2,2n >  gencodeAnnot$GENCODE_VERSION.bgpInput

tawk '{print $4,$0}' gencodeAnnot$GENCODE_VERSION.bgpInput | sort > join1
hgsql $db -Ne "select transcriptId, transcriptClass from gencodeAttrs$GENCODE_VERSION" | sort > attrs.txt
join -t $'\t'   join1 attrs.txt > join2
hgsql $db -Ne "select transcriptId, source from gencodeTranscriptSource$GENCODE_VERSION" | sort > source.txt
join -t $'\t'   join2 source.txt > join3
hgsql $db -Ne "select transcriptId, transcriptType from gencodeAttrs$GENCODE_VERSION" | sort > biotype.txt
join -t $'\t'   join3 biotype.txt > join4
hgsql $db -Ne "select transcriptId, tag from gencodeTag$GENCODE_VERSION" | sort | tawk '{if ($1 != last) {print last,buff; buff=$2}else {buff=buff "," $2} last=$1} END {print last,buff}' | tail -n +2 > tags.txt
join -t $'\t' -a 1  -e"none" -o auto   join4 tags.txt > join5
hgsql $db -Ne "select transcriptId, level from gencodeAttrs$GENCODE_VERSION" | sort > level.txt
join -t $'\t'   join5 level.txt > join6
cut -f 2- -d $'\t' join6 | sort -k1,1 -k2,2n > bgpInput.txt

bedToBigBed -type=bed12+15 -tab -as=$HOME/kent/src/hg/lib/gencodeBGP.as bgpInput.txt /cluster/data/$db/chrom.sizes $db.gencode$GENCODE_VERSION.bb

ln -s `pwd`/$db.gencode$GENCODE_VERSION.bb /gbdb/$db/gencode/gencode$GENCODE_VERSION.bb

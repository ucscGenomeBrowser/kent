# This doc is deprecated.   Look at hg/utils/otto/knownGene for up-to-date methodology
# This doc assumes that the gencode* tables have been built on $db
db=mm39
GENCODE_VERSION=VM27
PREV_GENCODE_VERSION=NOT_VALID
dir=/hive/data/genomes/$db/bed/gencode$GENCODE_VERSION/build
genomes=/hive/data/genomes
tempDb=knownGeneVM26
oldDb=mm10
kent=$HOME/kent
spDb=sp180404
cpuFarm=ku
export oldGeneDir=/cluster/data/mm10/bed/ucsc.19.1
export oldGeneBed=/cluster/data/mm10/bed/ucsc.19.1/ucscGenes.bed
export lastVer=12
export curVer=13
export Db=Mm39
export xdb=hg38
export Xdb=Hg38
export ydb=canFam3
export zdb=rheMac8
export ratDb=rn6
export RatDb=Rn6
export fishDb=danRer11
export flyDb=dm6
export wormDb=ce11
export yeastDb=sacCer3
export tempFa=$dir/ucscGenes.faa
export genomes=/hive/data/genomes

#export xdbFa=$genomes/$xdb/bed/ucsc.18.1/ucscGenes.faa
export ratFa=$genomes/$ratDb/bed/ensGene.95/ensembl.faa
#export ratFa=$genomes/$ratDb/bed/blastpRgdGene2/rgdGene2Pep.faa
export fishFa=$genomes/$fishDb/bed/ensGene.95/ensembl.faa
#export fishFa=$genomes/$fishDb/bed/blastp/ensembl.faa
export flyFa=$genomes/$flyDb/bed/ensGene.95/ensembl.faa
#export flyFa=$genomes/$flyDb/bed/hgNearBlastp/100806/$flyDb.flyBasePep.faa
export wormFa=$genomes/$wormDb/bed/ws245Genes/ws245Pep.faa
#export wormFa=$genomes/$wormDb/bed/blastp/wormPep190.faa
export yeastFa=$genomes/$yeastDb/bed/sgdAnnotations/blastTab/sacCer3.sgd.faa
export scratchDir=/hive/users/braney/scratch

export blastTab=mmBlastTab
export xBlastTab=mmBlastTab
export rnBlastTab=rnBlastTab
export dbHost=hgwdev
export ramFarm=ku
export cpuFarm=ku

mkdir -p $dir
cd $dir

#DIDN'T DO
# first get list of tables from push request in $lastVer.table.lst
# here's the redmine http://redmine.soe.ucsc.edu/issues/21644
wc -l $oldGeneDir/$lastVer.table.lst 
# 62

(
cat $oldGeneDir/$lastVer.table.lst | grep -v "ToKg$lastVer" | grep -v "XrefOld" | grep -v "knownGeneOld" | grep -v "knownToGencode" 
echo kg${lastVer}ToKg${curVer}
echo knownGeneOld$lastVer
echo kgXrefOld$lastVer
) | sort >  $curVer.table.lst 
#end DIDN'T DO

echo "create database $tempDb" | hgsql ""
echo "create table chromInfo like  $db.chromInfo" | hgsql $tempDb
echo "insert into chromInfo select * from   $db.chromInfo" | hgsql $tempDb

hgsql -e "select * from wgEncodeGencodeComp$GENCODE_VERSION" --skip-column-names $db | cut -f 2-16 |  genePredToBed stdin tmp
hgsql -e "select * from wgEncodeGencodePseudoGene$GENCODE_VERSION" --skip-column-names $db | cut -f 2-16 |  genePredToBed stdin tmp2
sort -k1,1 -k2,2n tmp tmp2 | gzip -c > gencode${GENCODE_VERSION}.bed.gz
#hgsql -e "select * from gencodeAnnot$GENCODE_VERSION" --skip-column-names $db | cut -f 2-16 |  genePredToBed stdin stdout | sort -k1,1 -k2,2n | gzip -c > gencode${GENCODE_VERSION}.bed.gz

touch oldToNew.tab

zcat gencode${GENCODE_VERSION}.bed.gz > ucscGenes.bed
txBedToGraph ucscGenes.bed ucscGenes ucscGenes.txg
txgAnalyze ucscGenes.txg $genomes/$db/$db.2bit stdout | sort | uniq | bedClip stdin /cluster/data/hg38/chrom.sizes  ucscSplice.bed
hgLoadBed $tempDb knownAlt ucscSplice.bed

zcat gencode${GENCODE_VERSION}.bed.gz |  awk '{print $4}' | sort > newGencodeName.txt
hgsql $oldDb -Ne "select name,alignId from knownGene" | sort > oldGenToUcsc.txt

kgAllocId oldGenToUcsc.txt newGencodeName.txt 5075761 stdout | sort -u >  txToAcc.tab
# lastId 5077200

makeGencodeKnownGene -justKnown $db $tempDb $GENCODE_VERSION txToAcc.tab
#makeUcscGeneTables -justKnown $db $tempDb $GENCODE_VERSION txToAcc.tab
hgLoadSqlTab -notOnServer $tempDb knownGene $kent/src/hg/lib/knownGene.sql knownGene.gp
#hgsql $db -Ne "create view $tempDb.all_mrna as select * from all_mrna"
#hgsql $db -Ne "create view $tempDb.trackDb as select * from trackDb"
hgLoadGenePred -genePredExt $tempDb  knownGeneExt knownGeneExt.gp
hgMapToGene -type=psl -all -tempDb=$tempDb $db all_mrna knownGene knownToMrna
hgMapToGene -tempDb=$tempDb $db refGene knownGene knownToRefSeq
hgMapToGene -type=psl -tempDb=$tempDb $db all_mrna knownGene knownToMrnaSingle
makeGencodeKnownGene $db $tempDb $GENCODE_VERSION txToAcc.tab
#makeUcscGeneTables $db $tempDb $GENCODE_VERSION txToAcc.tab
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

hgsql -e "select * from wgEncodeGencodeComp$GENCODE_VERSION" --skip-column-names $db | cut -f 2-16 |  tawk '{print $1,$13,$14,$8,$15}' | sort | uniq > knownCds.tab
# hgsql -e "select * from gencodeAnnot$GENCODE_VERSION" --skip-column-names $db | cut -f 2-16 |  tawk '{print $1,$13,$14,$8,$15}' | sort | uniq > knownCds.tab
hgLoadSqlTab -notOnServer $tempDb knownCds $kent/src/hg/lib/knownCds.sql knownCds.tab

#hgsql -e "select * from gencodeTag$GENCODE_VERSION" --skip-column-names $db |  sort | uniq  > knownToTag.tab
hgsql -e "select * from wgEncodeGencodeTag$GENCODE_VERSION" --skip-column-names $db |  sort | uniq  > knownToTag.tab
hgLoadSqlTab -notOnServer $tempDb knownToTag $kent/src/hg/lib/knownTo.sql knownToTag.tab

#hgsql $tempDb -Ne "select k.name, g.geneId, g.unused1, g.geneType,g.transcriptName,g.transcriptType,g.unused2, g.unused3,  g.ccdsId, g.level, g.transcriptClass from knownGene k, $db.gencodeAttrs$GENCODE_VERSION g where k.name=g.transcriptId" | sort | uniq > knownAttrs.tab
hgsql $tempDb -Ne "select k.name, g.geneId, g.geneStatus, g.geneType,g.transcriptName,g.transcriptType,g.transcriptStatus, g.havanaGeneId,  g.ccdsId, g.level, g.transcriptClass from knownGene k, $db.wgEncodeGencodeAttrs$GENCODE_VERSION g where k.name=g.transcriptId" | sort | uniq > knownAttrs.tab

hgLoadSqlTab -notOnServer $tempDb knownAttrs $kent/src/hg/lib/knownAttrs.sql knownAttrs.tab


cat  << __EOF__  > colors.sed
s/coding/12\t12\t120/
s/nonCoding/0\t100\t0/
s/pseudo/255\t51\t255/
s/problem/254\t0\t0/
__EOF__

#hgsql $db -Ne "select * from gencodeAttrs$GENCODE_VERSION" | tawk '{print $5,$13}' | sed -f colors.sed > colors.txt
hgsql $db -Ne "select * from wgEncodeGencodeAttrs$GENCODE_VERSION" | tawk '{print $5,$13}' | sed -f colors.sed > colors.txt
hgLoadSqlTab -notOnServer $tempDb kgColor $kent/src/hg/lib/kgColor.sql colors.txt

hgsql $tempDb -e "select * from knownToMrna" | tail -n +2 | tawk '{if ($1 != last) {print last, count, buffer; count=1; buffer=$2} else {count++;buffer=$2","buffer} last=$1}' | tail -n +2 | sort > tmp1
hgsql $tempDb  -e "select * from knownToMrnaSingle" | tail -n +2 | sort > tmp2
join  tmp2 tmp1 > knownGene.ev
txGeneAlias $db $spDb kgXref.tab knownGene.ev oldToNew.tab foo.alias foo.protAlias
tawk '{split($2,a,"."); for(ii = 1; ii <= a[2]; ii++) print $1,a[1] "." ii }' txToAcc.tab >> foo.alias
tawk '{split($1,a,"."); for(ii = 1; ii <= a[2] - 1; ii++) print $1,a[1] "." ii }' txToAcc.tab >> foo.alias
sort foo.alias | uniq > ucscGenes.alias
sort foo.protAlias | uniq > ucscGenes.protAlias
rm foo.alias foo.protAlias
hgLoadSqlTab -notOnServer $tempDb kgAlias $kent/src/hg/lib/kgAlias.sql ucscGenes.alias
hgLoadSqlTab -notOnServer $tempDb kgProtAlias $kent/src/hg/lib/kgProtAlias.sql ucscGenes.protAlias
hgsql $tempDb -N -e 'select kgXref.kgID, spID, alias from kgXref, kgAlias where kgXref.kgID=kgAlias.kgID' > kgSpAlias_0.tmp

hgsql $tempDb -N -e 'select kgXref.kgID, spID, alias from kgXref, kgProtAlias where kgXref.kgID=kgProtAlias.kgID' >> kgSpAlias_0.tmp
cat kgSpAlias_0.tmp|sort -u  > kgSpAlias.tab
rm kgSpAlias_0.tmp

hgLoadSqlTab -notOnServer $tempDb kgSpAlias $kent/src/hg/lib/kgSpAlias.sql kgSpAlias.tab

txGeneExplainUpdate2 $oldGeneBed ucscGenes.bed kgOldToNew.tab
hgLoadSqlTab -notOnServer $tempDb kg${lastVer}ToKg${curVer} $kent/src/hg/lib/kg1ToKg2.sql kgOldToNew.tab


#ifdef NOTNOW
hgsql $db -Ne "select * from gencodeToUniProt$GENCODE_VERSION" | tawk '{print $1,$2}'|  sort > uniProt.txt
hgsql $db -Ne "select * from gencodeAnnot$GENCODE_VERSION" | tawk '{print $1,$12}' | sort > gene.txt
join -a 1 gene.txt uniProt.txt > geneNames.txt
#endif

//genePredToBigGenePred -score=knownScore.txt  -colors=colors.txt -geneNames=geneNames.txt  -known gencodeAnnot$GENCODE_VERSION.txt  gencodeAnnot$GENCODE_VERSION.bgpInput
#hgsql $db -Ne "select * from gencodeAnnot$GENCODE_VERSION" | cut -f 2- > gencodeAnnot$GENCODE_VERSION.txt
#genePredToBigGenePred -colors=colors.txt gencodeAnnot$GENCODE_VERSION.txt stdout | sort -k1,1 -k2,2n >  gencodeAnnot$GENCODE_VERSION.bgpInput

hgsql $tempDb -Ne "select kgId, geneSymbol, spID from kgXref" > geneNames.txt
#hgsql $tempDb -Ne "select * from knownCds" > knownCds.txt
#genePredToBigGenePred -colors=colors.txt -known knownGene.gp stdout | sort -k1,1 -k2,2n >  gencodeAnnot$GENCODE_VERSION.bgpInput
genePredToBigGenePred -colors=colors.txt -geneNames=geneNames.txt -known -cds=knownCds.tab   knownGene.gp  stdout | sort -k1,1 -k2,2n >  gencodeAnnot$GENCODE_VERSION.bgpInput

tawk '{print $4,$0}' gencodeAnnot$GENCODE_VERSION.bgpInput | sort > join1
hgsql $db -Ne "select transcriptId, transcriptClass from wgEncodeGencodeAttrs$GENCODE_VERSION" |  sed 's/other/nonCoding/'| sort > attrs.txt
join -t $'\t'   join1 attrs.txt > join2
hgsql $db -Ne "select transcriptId, source from wgEncodeGencodeTranscriptSource$GENCODE_VERSION" | sort > source.txt
join -t $'\t'   join2 source.txt > join3
hgsql $db -Ne "select transcriptId, transcriptType from wgEncodeGencodeAttrs$GENCODE_VERSION" | sort > biotype.txt
join -t $'\t'   join3 biotype.txt > join4
hgsql $db -Ne "select transcriptId, tag from wgEncodeGencodeTag$GENCODE_VERSION" | sort | tawk '{if ($1 != last) {print last,buff; buff=$2}else {buff=buff "," $2} last=$1} END {print last,buff}' | tail -n +2 > tags.txt
join -t $'\t' -a 1  -e"none" -o auto   join4 tags.txt > join5
hgsql $db -Ne "select transcriptId, level from wgEncodeGencodeAttrs$GENCODE_VERSION" | sort > level.txt
join -t $'\t'   join5 level.txt > join6
grep basic tags.txt | tawk '{print $1, 1, "basic"}' > basic.txt
tawk '{print $5,0,"canonical"}'  knownCanonical.tab | sort > canonical.txt
tawk '{print $4,2,"all"}' wgEncodeGencodeAnnot$GENCODE_VERSION.bgpInput | sort > all.txt
sort -k1,1 -k2,2n basic.txt canonical.txt all.txt | tawk '{if ($1 != last) {print last,buff; buff=$3}else {buff=buff "," $3} last=$1} END {print last,buff}' | tail -n +2  > tier.txt
join -t $'\t'   join6 tier.txt > join7
cut -f 2- -d $'\t' join7 | sort -k1,1 -k2,2n > bgpInput.txt

bedToBigBed -extraIndex=name -type=bed12+16 -tab -as=$HOME/kent/src/hg/lib/gencodeBGP.as bgpInput.txt /cluster/data/$db/chrom.sizes $db.gencode$GENCODE_VERSION.bb

ln -s `pwd`/$db.gencode$GENCODE_VERSION.bb /gbdb/$db/gencode/gencode$GENCODE_VERSION.bb



hgsql --skip-column-names -e "select mrnaAcc,locusLinkId from hgFixed.refLink" $db > refToLl.txt
hgMapToGene -tempDb=$tempDb $db refGene knownGene knownToLocusLink -lookup=refToLl.txt
knownToVisiGene $tempDb -probesDb=$db

hgMapToGene $db -tempDb=$tempDb -all -type=genePred gtexGeneModelV6 knownGene knownToGtex

awk '{OFS="\t"} {print $4,$4}' ucscGenes.bed | sort | uniq > knownToEnsembl.tab
cp knownToEnsembl.tab knownToGencode${GENCODE_VERSION}.tab
hgLoadSqlTab -notOnServer $tempDb  knownToEnsembl  $kent/src/hg/lib/knownTo.sql  knownToEnsembl.tab
hgLoadSqlTab -notOnServer $tempDb  knownToGencode${GENCODE_VERSION}  $kent/src/hg/lib/knownTo.sql  knownToGencode${GENCODE_VERSION}.tab

# make knownToLynx
mkdir -p $dir/lynx
cd $dir/lynx

wget "http://lynx.ci.uchicago.edu/downloads/LYNX_GENES.tab"
awk '{print $2}' LYNX_GENES.tab | sort > lynxExists.txt
hgsql -e "select geneSymbol,kgId from kgXref" --skip-column-names $tempDb | awk '{if (NF == 2) print}' | sort > geneSymbolToKgId.txt
join lynxExists.txt geneSymbolToKgId.txt | awk 'BEGIN {OFS="\t"} {print $2,$1}' | sort > knownToLynx.tab
hgLoadSqlTab -notOnServer $tempDb  knownToLynx $kent/src/hg/lib/knownTo.sql  knownToLynx.tab

# load malacards table
hgsql -e "select geneSymbol,kgId from kgXref" --skip-column-names $tempDb | awk '{if (NF == 2) print}' | sort > geneSymbolToKgId.txt
hgsql -e "select geneSymbol from malacards" --skip-column-names $db | sort > malacardExists.txt
join malacardExists.txt  geneSymbolToKgId.txt | awk 'BEGIN {OFS="\t"} {print $2, $1}' > knownToMalacard.txt
hgLoadSqlTab -notOnServer $tempDb  knownToMalacards $kent/src/hg/lib/knownTo.sql  knownToMalacard.txt

twoBitToFa -noMask /cluster/data/$db/$db.2bit -bed=ucscGenes.bed stdout | faFilter -uniq stdin  ucscGenes.fa
hgPepPred $tempDb generic knownGeneMrna ucscGenes.fa


hgMapToGene -tempDb=$tempDb $db gnfAtlas2 knownGene knownToGnfAtlas2 '-type=bed 12'
hgMapToGene -tempDb=$tempDb $db affyU133 knownGene knownToU133
hgMapToGene -tempDb=$tempDb $db affyU95 knownGene knownToU95

hgsql $tempDb -Ne "create view all_mrna as select * from $db.all_mrna"
hgsql $tempDb -Ne "create view ensGene as select * from $db.ensGene"
hgsql $tempDb -Ne "create view gtexGene as select * from $db.gtexGene"
hgsql $tempDb -Ne "create view gencodeAnnotV35 as select * from $db.gencodeAnnotV35"
hgsql $tempDb -Ne "create view gencodeAttrsV35 as select * from $db.gencodeAttrsV35"
hgsql $tempDb -Ne "create view gencodeGeneSourceV35 as select * from $db.gencodeGeneSourceV35"
hgsql $tempDb -Ne "create view gencodeTranscriptSourceV35 as select * from $db.gencodeTranscriptSourceV35"
hgsql $tempDb -Ne "create view gencodeToPdbV35 as select * from $db.gencodeToPdbV35"
hgsql $tempDb -Ne "create view gencodeToPubMedV35 as select * from $db.gencodeToPubMedV35"
hgsql $tempDb -Ne "create view gencodeToRefSeqV35 as select * from $db.gencodeToRefSeqV35"
hgsql $tempDb -Ne "create view gencodeTagV35 as select * from $db.gencodeTagV35"
hgsql $tempDb -Ne "create view gencodeTranscriptSupportV35 as select * from $db.gencodeTranscriptSupportV35"
hgsql $tempDb -Ne "create view gencodeToUniProtV35 as select * from $db.gencodeToUniProtV35"

bioCycDir=/hive/data/outside/bioCyc/190905/download/humancyc/21.0/data
mkdir $dir/bioCyc
cd $dir/bioCyc

grep -E -v "^#" $bioCycDir/genes.col  > genes.tab  
grep -E -v "^#" $bioCycDir/pathways.col  | awk -F'\t' '{if (140 == NF) { printf "%s\t\t\n", $0; } else { print $0}}' > pathways.tab

kgBioCyc1 -noEnsembl genes.tab pathways.tab $tempDb bioCycPathway.tab bioCycMapDesc.tab  
hgLoadSqlTab $tempDb bioCycPathway ~/kent/src/hg/lib/bioCycPathway.sql ./bioCycPathway.tab
hgLoadSqlTab $tempDb bioCycMapDesc ~/kent/src/hg/lib/bioCycMapDesc.sql ./bioCycMapDesc.tab

mkdir -p $dir/kegg
cd $dir/kegg

# Make the keggMapDesc table, which maps KEGG pathway IDs to descriptive names
cp /cluster/data/hg19/bed/ucsc.14.3/kegg/map_title.tab .
# wget --timestamping ftp://ftp.genome.jp/pub/kegg/pathway/map_title.tab
cat map_title.tab | sed -e 's/\t/\thsa\t/' > j.tmp
cut -f 2 j.tmp >j.hsa
cut -f 1,3 j.tmp >j.1
paste j.hsa j.1 |sed -e 's/\t//' > keggMapDesc.tab
rm j.hsa j.1 j.tmp
hgLoadSqlTab -notOnServer $tempDb keggMapDesc $kent/src/hg/lib/keggMapDesc.sql keggMapDesc.tab

# Following in two-step process, build/load a table that maps UCSC Gene IDs
# to LocusLink IDs and to KEGG pathways.  First, make a table that maps 
# LocusLink IDs to KEGG pathways from the downloaded data.  Store it temporarily
# in the keggPathway table, overloading the schema.
cp /cluster/data/hg19/bed/ucsc.14.3/kegg/hsa_pathway.list .

cat hsa_pathway.list| sed -e 's/path://'|sed -e 's/:/\t/' > j.tmp
hgLoadSqlTab -notOnServer $tempDb keggPathway $kent/src/hg/lib/keggPathway.sql j.tmp

# Next, use the temporary contents of the keggPathway table to join with
# knownToLocusLink, creating the real content of the keggPathway table.
# Load this data, erasing the old temporary content
hgsql $tempDb -B -N -e 'select distinct name, locusID, mapID from keggPathway p, knownToLocusLink l where p.locusID=l.value' > keggPathway.tab
hgLoadSqlTab -notOnServer $tempDb \
    keggPathway $kent/src/hg/lib/keggPathway.sql  keggPathway.tab

# Finally, update the knownToKeggEntrez table from the keggPathway table.
hgsql $tempDb -B -N -e 'select kgId, mapID, mapID, "+", locusID from keggPathway' |sort -u| sed -e 's/\t+\t/+/' > knownToKeggEntrez.tab
hgLoadSqlTab -notOnServer $tempDb knownToKeggEntrez $kent/src/hg/lib/knownToKeggEntrez.sql knownToKeggEntrez.tab
#hgsql $tempDb -e "delete k from knownToKeggEntrez k, kgXref x where k.name = x.kgID and x.geneSymbol = 'abParts'"

# Make spMrna table 
cd $dir
#hgsql $db -N -e "select spDisplayID,kgID from kgXref where spDisplayID != ''" > spMrna.tab;
hgsql $tempDb -N -e "select spDisplayID,kgID from kgXref where spDisplayID != ''" > spMrna.tab;
hgLoadSqlTab $tempDb spMrna $kent/src/hg/lib/spMrna.sql spMrna.tab


# Do CGAP tables 

mkdir -p $dir/cgap
cd $dir/cgap

wget --timestamping -O Hs_GeneData.dat "ftp://ftp1.nci.nih.gov/pub/CGAP/Hs_GeneData.dat"
hgCGAP Hs_GeneData.dat
    
cat cgapSEQUENCE.tab cgapSYMBOL.tab cgapALIAS.tab|sort -u > cgapAlias.tab
hgLoadSqlTab -notOnServer $tempDb cgapAlias $kent/src/hg/lib/cgapAlias.sql ./cgapAlias.tab

hgLoadSqlTab -notOnServer $tempDb cgapBiocPathway $kent/src/hg/lib/cgapBiocPathway.sql ./cgapBIOCARTA.tab

cat cgapBIOCARTAdesc.tab|sort -u > cgapBIOCARTAdescSorted.tab
hgLoadSqlTab -notOnServer $tempDb cgapBiocDesc $kent/src/hg/lib/cgapBiocDesc.sql cgapBIOCARTAdescSorted.tab

# MAKE FOLDUTR TABLES 
# First set up directory structure and extract UTR sequence on hgwdev
cd $dir
mkdir -p rnaStruct

cd rnaStruct
ln -s /cluster/data/$db/$db.2bit $tempDb.2bit
mkdir -p utr3/split utr5/split utr3/fold utr5/fold
# these commands take some significant time
utrFa -nibPath=`pwd` $tempDb knownGene utr3 utr3/utr.fa
utrFa -nibPath=`pwd` $tempDb knownGene utr5 utr5/utr.fa

# Split up files and make files that define job.
faSplit sequence utr3/utr.fa 10000 utr3/split/s
faSplit sequence utr5/utr.fa 10000 utr5/split/s
ls -1 utr3/split > utr3/in.lst
ls -1 utr5/split > utr5/in.lst
cd utr3
cat << _EOF_ > template
#LOOP
rnaFoldBig split/\$(path1) fold
#ENDLOOP
_EOF_
gensub2 in.lst single template jobList
cp template ../utr5
cd ../utr5

gensub2 in.lst single template jobList

# Do cluster runs for UTRs
ssh $cpuFarm "cd $dir/rnaStruct/utr3; para make jobList"
ssh $cpuFarm "cd $dir/rnaStruct/utr5; para make jobList"

ssh $cpuFarm "cd $dir/rnaStruct/utr3; para time"
ssh $cpuFarm "cd $dir/rnaStruct/utr5; para time"

# Load database
    cd $dir/rnaStruct/utr5
    hgLoadRnaFold $tempDb foldUtr5 fold
    cd ../utr3
    hgLoadRnaFold -warnEmpty $tempDb foldUtr3 fold

# Clean up
    rm -r split fold err batch.bak
    cd ../utr5
    rm -r split fold err batch.bak

hgKgGetText $tempDb tempSearch.txt
sort tempSearch.txt > tempSearch2.txt
tawk '{split($2,a,"."); printf "%s\t", $1;for(ii = 1; ii <= a[2]; ii++) printf "%s ",a[1] "." ii; printf "\n" }' txToAcc.tab | sort > tempSearch3.txt
join tempSearch2.txt tempSearch3.txt | sort > knownGene.txt
ixIxx knownGene.txt knownGene${GENCODE_VERSION}.ix knownGene${GENCODE_VERSION}.ixx
 rm -rf /gbdb/$db/knownGene${GENCODE_VERSION}.ix /gbdb/$db/knownGene${GENCODE_VERSION}.ixx
ln -s `pwd`/knownGene${GENCODE_VERSION}.ix  /gbdb/$db/knownGene${GENCODE_VERSION}.ix
ln -s `pwd`/knownGene${GENCODE_VERSION}.ixx /gbdb/$db/knownGene${GENCODE_VERSION}.ixx  
tawk '{print $5}' knownCanonical.tab | sort > knownCanonicalId.txt
join knownCanonicalId.txt knownGene.txt | join -v 1 /dev/stdin pseudo.txt > knownGeneFast.txt
ixIxx knownGeneFast.txt knownGeneFast${GENCODE_VERSION}.ix knownGeneFast${GENCODE_VERSION}.ixx
 rm -rf /gbdb/$db/knownGeneFast${GENCODE_VERSION}.ix /gbdb/$db/knownGeneFast${GENCODE_VERSION}.ixx
ln -s $dir/knownGeneFast${GENCODE_VERSION}.ix  /gbdb/$db/knownGeneFast${GENCODE_VERSION}.ix
ln -s $dir/knownGeneFast${GENCODE_VERSION}.ixx /gbdb/$db/knownGeneFast${GENCODE_VERSION}.ixx  

#zcat gencode${GENCODE_VERSION}.bed.gz > ucscGenes.bed
#jtwoBitToFa -noMask /cluster/data/$db/$db.2bit -bed=ucscGenes.bed stdout | faFilter -uniq stdin  ucscGenes.fa
#jhgPepPred $tempDb generic knownGeneMrna ucscGenes.fa
bedToPsl /cluster/data/$db/chrom.sizes ucscGenes.bed ucscGenes.psl
pslRecalcMatch ucscGenes.psl /cluster/data/$db/$db.2bit ucscGenes.fa kgTargetAli.psl
# should be zero
awk '$11 != $1 + $3+$4' kgTargetAli.psl
hgLoadPsl $tempDb kgTargetAli.psl

cd $dir
# Make PCR target for UCSC Genes, Part 1.
# 1. Get a set of IDs that consist of the UCSC Gene accession concatenated with the
#    gene symbol, e.g. uc010nxr.1__DDX11L1
hgsql $tempDb -N -e 'select kgId,geneSymbol from kgXref' \
    | perl -wpe 's/^(\S+)\t(\S+)/$1\t${1}__$2/ || die;' \
      | sort -u > idSub.txt 
# 2. Get a file of per-transcript fasta sequences that contain the sequences of each UCSC Genes transcript, with this new ID in the place of the UCSC Genes accession.   Convert that file to TwoBit format and soft-link it into /gbdb/hg38/targetDb/ 
awk '{if (!found[$4]) print; found[$4]=1 }' ucscGenes.bed > nodups.bed
subColumn 4 nodups.bed idSub.txt ucscGenesIdSubbed.bed 
sequenceForBed -keepName -db=$db -bedIn=ucscGenesIdSubbed.bed -fastaOut=stdout  | faToTwoBit stdin ${db}KgSeq${curVer}.2bit
mkdir -p /gbdb/$db/targetDb/ 
rm -f /gbdb/$db/targetDb/${db}KgSeq${curVer}.2bit
ln -s $dir/${db}KgSeq${curVer}.2bit /gbdb/$db/targetDb/
# Load the table kgTargetAli, which shows where in the genome these targets are.
#cut -f 1-10 knownGene.gp | genePredToFakePsl $tempDb stdin kgTargetAli.psl /dev/null
hgLoadPsl $tempDb kgTargetAli.psl

# 3. Ask cluster-admin to start an untranslated, -stepSize=5 gfServer on       
# /gbdb/$db/targetDb/${db}KgSeq${curVer}.2bit 

# 4. On hgwdev, insert new records into blatServers and targetDb, using the 
# host (field 2) and port (field 3) specified by cluster-admin.  Identify the
# blatServer by the keyword "$db"Kg with the version number appended
#Response : On bla1c
#Starting untrans gfServer for mm39KgSeq13 on port 17921

# untrans gfServer for mm39KgSeq13 on host blat1b, port 17921
hgsql hgcentraltest -e \
      'INSERT into blatServers values ("mm39KgSeq13", "blat1c", 17921, 0, 1,"");'
hgsql hgcentraltest -e \
            'INSERT into targetDb values("mm39KgSeq13", "GENCODE Genes", \
                     "mm39", "kgTargetAli", "", "", \
                              "/gbdb/mm39/targetDb/mm39KgSeq13.2bit", 1, now(), "");'

for i in  $tempFa $xdbFa $ratFa $fishFa $flyFa $wormFa $yeastFa
do
if test ! -f $i
then echo $i not found
fi
done

rm -rf   $dir/hgNearBlastp
mkdir  $dir/hgNearBlastp
cd $dir/hgNearBlastp
tcsh
cat << _EOF_ > config.ra
# Latest human vs. other Gene Sorter orgs:
# mouse, rat, zebrafish, worm, yeast, fly

targetGenesetPrefix known
targetDb $tempDb
queryDbs $xdb $ratDb $fishDb $flyDb $wormDb $yeastDb

${tempDb}Fa $tempFa
${xdb}Fa $xdbFa
${ratDb}Fa $ratFa
${fishDb}Fa $fishFa
${flyDb}Fa $flyFa
${wormDb}Fa $wormFa
${yeastDb}Fa $yeastFa

buildDir $dir/hgNearBlastp
scratchDir $scratchDir/brHgNearBlastp
_EOF_

# exit tcsh

rm -rf  $scratchDir/brHgNearBlastp
doHgNearBlastp.pl -noLoad -clusterHub=ku -distrHost=hgwdev -dbHost=hgwdev -workhorse=hgwdev config.ra >  do.log  2>&1 &

# Load self
cd $dir/hgNearBlastp/run.$tempDb.$tempDb
# builds knownBlastTab
./loadPairwise.csh

# Load mouse and rat
cd $dir/hgNearBlastp/run.$tempDb.$xdb
hgLoadBlastTab $tempDb $xBlastTab -maxPer=1 out/*.tab
cd $dir/hgNearBlastp/run.$tempDb.$ratDb
hgLoadBlastTab $tempDb $rnBlastTab -maxPer=1 out/*.tab

# Remove non-syntenic hits for mouse and rat
# Takes a few minutes
mkdir -p /gbdb/$tempDb/liftOver
rm -f /gbdb/$tempDb/liftOver/${tempDb}To$RatDb.over.chain.gz /gbdb/$tempDb/liftOver/${tempDb}To$Xdb.over.chain.gz
ln -s $genomes/$db/bed/liftOver/${db}To$RatDb.over.chain.gz \
    /gbdb/$tempDb/liftOver/${tempDb}To$RatDb.over.chain.gz
ln -s $genomes/$db/bed/liftOver/${db}To${Xdb}.over.chain.gz \
    /gbdb/$tempDb/liftOver/${tempDb}To$Xdb.over.chain.gz

# delete non-syntenic genes from rat and mouse blastp tables
cd $dir/hgNearBlastp
synBlastp.csh $tempDb $xdb
# old number of unique query values: 94781
# old number of unique target values 27519
# new number of unique query values: 87746
# new number of unique target values 26162

synBlastp.csh $tempDb $ratDb knownGene ensGene
# old number of unique query values: 93733
# old number of unique target values 19909
# new number of unique query values: 87149
# new number of unique target values 19034

# Make reciprocal best subset for the blastp pairs that are too
# Far for synteny to help

# Us vs. fish
cd $dir/hgNearBlastp
export aToB=run.$db.$fishDb
export bToA=run.$fishDb.$db
cat $aToB/out/*.tab > $aToB/all.tab
cat $bToA/out/*.tab > $bToA/all.tab
blastRecipBest $aToB/all.tab $bToA/all.tab $aToB/recipBest.tab $bToA/recipBest.tab
hgLoadBlastTab $tempDb drBlastTab $aToB/recipBest.tab

# Us vs. fly
cd $dir/hgNearBlastp
export aToB=run.$db.$flyDb
export bToA=run.$flyDb.$db
cat $aToB/out/*.tab > $aToB/all.tab
cat $bToA/out/*.tab > $bToA/all.tab
blastRecipBest $aToB/all.tab $bToA/all.tab $aToB/recipBest.tab $bToA/recipBest.tab
hgLoadBlastTab $tempDb dmBlastTab $aToB/recipBest.tab

# Us vs. worm
cd $dir/hgNearBlastp
export aToB=run.$db.$wormDb
export bToA=run.$wormDb.$db
cat $aToB/out/*.tab > $aToB/all.tab
cat $bToA/out/*.tab > $bToA/all.tab
blastRecipBest $aToB/all.tab $bToA/all.tab $aToB/recipBest.tab $bToA/recipBest.tab
hgLoadBlastTab $tempDb ceBlastTab $aToB/recipBest.tab

# Us vs. yeast
cd $dir/hgNearBlastp
export aToB=run.$db.$yeastDb
export bToA=run.$yeastDb.$db
cat $aToB/out/*.tab > $aToB/all.tab
cat $bToA/out/*.tab > $bToA/all.tab
blastRecipBest $aToB/all.tab $bToA/all.tab $aToB/recipBest.tab $bToA/recipBest.tab
hgLoadBlastTab $tempDb scBlastTab $aToB/recipBest.tab

# Clean up
cd $dir/hgNearBlastp
cat run.$tempDb.$tempDb/out/*.tab | gzip -c > run.$tempDb.$tempDb/all.tab.gz
gzip run.*/all.tab

hgLoadNetDist $genomes/hg19/p2p/hprd/hprd.pathLengths $tempDb humanHprdP2P \
    -sqlRemap="select distinct value, name from knownToHprd"
hgLoadNetDist $genomes/hg19/p2p/vidal/humanVidal.pathLengths $tempDb humanVidalP2P -sqlRemap="select distinct locusLinkID, kgID from hgFixed.refLink,kgXref where hgFixed.refLink.mrnaAcc = kgXref.refSeq"
hgLoadNetDist $genomes/hg19/p2p/wanker/humanWanker.pathLengths $tempDb humanWankerP2P -sqlRemap="select distinct locusLinkID, kgID from hgFixed.refLink,kgXref where hgFixed.refLink.mrnaAcc = kgXref.refSeq"

mkdir $dir/wikipedia
cd $dir/wikipedia
hgsql $tempDb -e "select geneSymbol,name from knownGene g, kgXref x where g.name=x.kgId " | sort > $tempDb.symbolToId.txt
join -t $'\t'   /hive/groups/browser/wikipediaScrape/symbolToPage.txt $tempDb.symbolToId.txt | tawk '{print $3,$2}' | sort | uniq > $tempDb.idToPage.txt
hgLoadSqlTab $tempDb knownToWikipedia $HOME/kent/src/hg/lib/knownTo.sql $tempDb.idToPage.txt

# make views for all the tables in the specific database
hgsql knownGeneV35 -Ne "show full tables" | grep -v VIEW | grep -v history | grep -v masterGeneTrack | grep -v chromInfo | awk '{print $1}' | sort > v35.tables.txt
hgsql knownGeneV35 -Ne "show full tables" | grep -v VIEW | grep -v history | grep -v masterGeneTrack | grep -v chromInfo | awk '{print "show tables like \""$1"\";"}' > showTables.txt
hgsql knownGeneV35 -Ne "show full tables" | grep -v VIEW | grep -v history | grep -v masterGeneTrack | grep -v chromInfo | awk '{print "create view "$1" as select * from knownGeneV35."$1";"}' > makeViews.txt

# Regenerate ccdsKgMap table
$kent/src/hg/makeDb/genbank/bin/x86_64/mkCcdsGeneMap  -db=$tempDb -loadDb $db.ccdsGene knownGene ccdsKgMap

mkdir -p $dir/pfam
cd $dir/pfam
rm -rf  splitProt
mkdir  splitProt
faSplit sequence $dir/ucscGenes.faa 10000 splitProt/
mkdir -p result
ls -1 splitProt > prot.list
# /hive/data/outside/pfam/hmmpfam -E 0.1 /hive/data/outside/pfam/current/Pfam_fs \
cat << '_EOF_' > doPfam
#!/bin/csh -ef  
/hive/data/outside/pfam/Pfam29.0/PfamScan/hmmer-3.1b2-linux-intel-x86_64/binaries/hmmsearch   --domtblout /scratch/tmp/pfam.$2.pf --noali -o /dev/null -E 0.1 /hive/data/outside/pfam/Pfam29.0/Pfam-A.hmm     splitProt/$1 
mv /scratch/tmp/pfam.$2.pf $3
_EOF_
    # << happy emacs
chmod +x doPfam
cat << '_EOF_' > template
#LOOP
doPfam $(path1) $(root1) {check out line+ result/$(root1).pf}
#ENDLOOP
_EOF_
gensub2 prot.list single template jobList

ssh $cpuFarm "cd $dir/pfam; para make jobList"
ssh $cpuFarm "cd $dir/pfam; para time > run.time"
cat run.time

#Completed: 9428 of 9428 jobs
#CPU time in finished jobs:    2252658s   37544.30m   625.74h   26.07d  0.071 y
#IO & Wait Time:                476422s    7940.37m   132.34h    5.51d  0.015 y
#Average job time:                 289s       4.82m     0.08h    0.00d
##Longest finished job:            2943s      49.05m     0.82h    0.03d
#Submission to last job:          8377s     139.62m     2.33h    0.10d

# Make up pfamDesc.tab by converting pfam to a ra file first
cat << '_EOF_' > makePfamRa.awk
/^NAME/ {print}
/^ACC/ {print}
/^DESC/ {print}
/^TC/ {print $1,$3; printf("\n");}
_EOF_
awk -f makePfamRa.awk  /hive/data/outside/pfam/Pfam29.0/Pfam-A.hmm  > pfamDesc.ra
raToTab -cols=ACC,NAME,DESC,TC pfamDesc.ra stdout |  awk -F '\t' '{printf("%s\t%s\t%s\t%g\n", $1, $2, $3, $4);}' | sort > pfamDesc.tab

# Convert output to tab-separated file. 
cd $dir/pfam
catDir result | sed '/^#/d' > allResults.tab
awk 'BEGIN {OFS="\t"} { print $5,$1,$18-1,$19,$4,$14}' allResults.tab | sort > allUcscPfam.tab
join  -t $'\t' -j 1  allUcscPfam.tab pfamDesc.tab | tawk '{if ($6 > $9) print $2, $3, $4, $5, $6, $1}' > ucscPfam.tab
cd $dir

# Convert output to knownToPfam table
tawk '{print $1, gensub(/\.[0-9]+/, "", "g", $6)}' pfam/ucscPfam.tab | sort -u > knownToPfam.tab
hgLoadSqlTab -notOnServer $tempDb knownToPfam $kent/src/hg/lib/knownTo.sql knownToPfam.tab
tawk '{print gensub(/\.[0-9]+/, "", "g", $1), $2, $3}' pfam/pfamDesc.tab| hgLoadSqlTab -notOnServer $tempDb pfamDesc $kent/src/hg/lib/pfamDesc.sql stdin

cd $dir/pfam
genePredToFakePsl $tempDb knownGene knownGene.psl cdsOut.tab
sort cdsOut.tab | sed 's/\.\./   /' > sortCdsOut.tab
sort ucscPfam.tab> sortPfam.tab
awk '{print $10, $11}' knownGene.psl > gene.sizes
join sortCdsOut.tab sortPfam.tab |  awk '{print $1, $2 - 1 + 3 * $4, $2 - 1 + 3 * $5, $6}' | bedToPsl gene.sizes stdin domainToGene.psl
pslMap domainToGene.psl knownGene.psl stdout | pslToBed stdin stdout | bedOrBlocks -useName stdin domainToGenome.bed 
hgLoadBed $tempDb ucscGenePfam domainToGenome.bed

# Do scop run. Takes about 6 hours
#mkdir /hive/data/outside/scop/1.75
#cd /hive/data/outside/scop/1.75
#wget "ftp://license:SlithyToves@supfam.org/models/hmmlib_1.75.gz"
#gunzip hmmlib_1.75.gz
#wget "ftp://license:SlithyToves@supfam.org/models/model.tab.gz"
#gunzip model.tab.gz

mkdir -p $dir/scop
cd $dir/scop
rm -rf result
mkdir  result
ls -1 ../pfam/splitProt > prot.list
cat << '_EOF_' > doScop
#!/bin/csh -ef
/hive/data/outside/pfam/Pfam29.0/PfamScan/hmmer-3.1b2-linux-intel-x86_64/binaries/hmmsearch   --domtblout /scratch/tmp/scop.$2.pf --noali -o /dev/null -E 0.1 /hive/data/outside/scop/1.75/hmmlib_1.75  ../pfam/splitProt/$1
mv /scratch/tmp/scop.$2.pf $3
_EOF_
    # << happy emacs
chmod +x doScop
cat << '_EOF_' > template
#LOOP
doScop $(path1) $(root1) {check out line+ result/$(root1).pf}
#ENDLOOP
_EOF_
    # << happy emacs
gensub2 prot.list single template jobList

ssh $cpuFarm "cd $dir/scop; para make jobList"
ssh $cpuFarm "cd $dir/scop; para time > run.time"
cat run.time

# Completed: 9428 of 9428 jobs
# CPU time in finished jobs:    2122993s   35383.22m   589.72h   24.57d  0.067 y
# IO & Wait Time:                479507s    7991.78m   133.20h    5.55d  0.015 y
# Average job time:                 276s       4.60m     0.08h    0.00d
# Longest finished job:            2498s      41.63m     0.69h    0.03d
# Submission to last job:          8193s     136.55m     2.28h    0.09d

# Convert scop output to tab-separated files
cd $dir
catDir scop/result | sed '/^#/d' | awk 'BEGIN {OFS="\t"} {if ($7 <= 0.0001) print $1,$18-1,$19,$4, $7,$8}' | sort > scopPlusScore.tab
sort -k 2 /hive/data/outside/scop/1.75/model.tab > scop.model.tab
scopCollapse scopPlusScore.tab scop.model.tab ucscScop.tab \
	scopDesc.tab knownToSuper.tab
hgLoadSqlTab -notOnServer $tempDb scopDesc $kent/src/hg/lib/scopDesc.sql scopDesc.tab
hgLoadSqlTab $tempDb knownToSuper $kent/src/hg/lib/knownToSuper.sql knownToSuper.tab
#hgsql $tempDb -e "delete k from knownToSuper k, kgXref x where k.gene = x.kgID and x.geneSymbol = 'abParts'"


hgMapToGene -geneTableType=genePred -tempDb=$tempDb $db affyU133 knownGene knownToU133
hgMapToGene -geneTableType=genePred -tempDb=$tempDb $db affyU95 knownGene knownToU95
    mkdir hprd
    cd hprd
    wget "http://www.hprd.org/edownload/HPRD_FLAT_FILES_041310"
    tar xvf HPRD_FLAT_FILES_041310
    knownToHprd $tempDb FLAT_FILES_072010/HPRD_ID_MAPPINGS.txt

    time hgExpDistance $tempDb hgFixed.gnfHumanU95MedianRatio \
	    hgFixed.gnfHumanU95Exps gnfU95Distance  -lookup=knownToU95
    time hgExpDistance $tempDb hgFixed.gnfHumanAtlas2MedianRatio \
	hgFixed.gnfHumanAtlas2MedianExps gnfAtlas2Distance \
	-lookup=knownToGnfAtlas2

# Build knownToMupit

mkdir mupit
cd mupit

# mupit-pdbids.txt was emailed from Kyle Moad (kmoad@insilico.us.com)
# wc -l 
cp /hive/data/outside/mupit/mupit-pdbids.txt .
# get knownGene IDs and associated PDB IDS
# the extDb{Ref} parts come from hg/hgGene/domains.c:domainsPrint()
hgsql -Ne "select kgID, extAcc1 from $db.kgXref x \
    inner join sp180404.extDbRef sp on x.spID = sp.acc \
    inner join sp180404.extDb e on sp.extDb=e.id \
    where x.spID != '' and e.val='PDB' order by kgID" \
    > $db.knownToPdb.txt;
# filter out pdbIds not found in mupit
cat mupit-pdbids.txt | tr '[a-z]' '[A-Z]' | \
    grep -Fwf - $db.knownToPdb.txt >  knownToMupit.txt;
# check that it filtered correctly:
# cut -f2 $db.knownToMuipit.txt | sort -u | wc -l;
# load new table for hgGene/hgc
hgLoadSqlTab $db knownToMupit ~/kent/src/hg/lib/knownTo.sql knownToMupit.txt

#myGene2
mkdir $dir/myGene2
cd $dir/myGene2

# copy list of genes from https://mygene2.org/MyGene2/genes 
awk '{print $1}' | sort > genes.lst
hgsql hg38 -Ne "select geneSymbol, kgId from kgXref" | sort > ids.txt
join -t $'\t' genes.lst  ids.txt | tawk '{print $2,$1}' | sort > knownToMyGene2.txt
hgLoadSqlTab $db knownToMyGene2 ~/kent/src/hg/lib/knownTo.sql knownToMyGene2.txt

# make knownToNextProt
mkdir -p $dir/nextProt
cd $dir/nextProt

wget "ftp://ftp.nextprot.org/pub/current_release/ac_lists/nextprot_ac_list_all.txt"
awk '{print $0, $0}' nextprot_ac_list_all.txt | sed 's/NX_//' | sort > displayIdToNextProt.txt
hgsql -e "select spID,kgId from kgXref" --skip-column-names $tempDb | awk '{if (NF == 2) print}' | sort > displayIdToKgId.txt
join displayIdToKgId.txt displayIdToNextProt.txt | awk 'BEGIN {OFS="\t"} {print $2,$3}' > knownToNextProt.tab
hgLoadSqlTab -notOnServer $tempDb  knownToNextProt $kent/src/hg/lib/knownTo.sql  knownToNextProt.tab

# this should be done AFTER moving the new tables into hg38
hgKgGetText $tempDb tempSearch.txt
sort tempSearch.txt > tempSearch2.txt
tawk '{split($2,a,"."); printf "%s\t", $1;for(ii = 1; ii <= a[2]; ii++) printf "%s ",a[1] "." ii; printf "\n" }' txToAcc.tab | sort > tempSearch3.txt
tawk '{split($2,a,"."); printf "%s\t%s ", $1,a[1];for(ii = 1; ii <= a[2]; ii++) printf "%s ",a[1] "." ii; printf "\n" }' knownAttrs.tab | sort > tempSearch4.txt
join tempSearch2.txt tempSearch3.txt | join /dev/stdin tempSearch4.txt | sort > gencode$GENCODE_VERSION.txt
ixIxx gencode$GENCODE_VERSION.txt gencode$GENCODE_VERSION.ix gencode$GENCODE_VERSION.ixx
 rm -rf /gbdb/$tempDb/gencode/gencode$GENCODE_VERSION.ix /gbdb/$tempDb/gencode/gencode$GENCODE_VERSION.ixx
ln -s $dir/gencode$GENCODE_VERSION.ix  /gbdb/$tempDb/gencode/gencode$GENCODE_VERSION.ix
ln -s $dir/gencode$GENCODE_VERSION.ixx /gbdb/$tempDb/gencode/gencode$GENCODE_VERSION.ixx  



# This doc assumes that the gencode* tables have been built on $db
db=hg38
GENCODE_VERSION=V35
dir=/hive/data/genomes/$db/bed/gencode$GENCODE_VERSION/build
genomes=/hive/data/genomes
tempDb=knownGeneV35
kent=$HOME/kent
spDb=sp180404
cpuFarm=ku
export curVer=13

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
grep basic tags.txt | tawk '{print $1, 1, "basic"}' > basic.txt
tawk '{print $5,0,"canonical"}'  knownCanonical.tab | sort > canonical.txt
tawk '{print $4,2,"all"}' gencodeAnnot$GENCODE_VERSION.bgpInput | sort > all.txt
sort -k1,1 -k2,2n basic.txt canonical.txt all.txt | tawk '{if ($1 != last) {print last,buff; buff=$3}else {buff=buff "," $3} last=$1} END {print last,buff}' | tail -n +2  > tier.txt
join -t $'\t'   join6 tier.txt > join7
cut -f 2- -d $'\t' join7 | sort -k1,1 -k2,2n > bgpInput.txt

bedToBigBed -extraIndex=name -type=bed12+16 -tab -as=$HOME/kent/src/hg/lib/gencodeBGP.as bgpInput.txt /cluster/data/$db/chrom.sizes $db.gencode$GENCODE_VERSION.bb

ln -s `pwd`/$db.gencode$GENCODE_VERSION.bb /gbdb/$db/gencode/gencode$GENCODE_VERSION.bb

hgsql $tempDb -e "select * from knownToMrna" | tail -n +2 | tawk '{if ($1 != last) {print last, count, buffer; count=1; buffer=$2} else {count++;buffer=$2","buffer} last=$1}' | tail -n +2 | sort > tmp1
hgsql $tempDb  -e "select * from knownToMrnaSingle" | tail -n +2 | sort > tmp2
join  tmp2 tmp1 > knownGene.ev

txGeneAlias $db $spDb kgXref.tab knownGene.ev oldToNew.tab foo.alias foo.protAlias
tawk '{split($2,a,"."); for(ii = 1; ii <= a[2]; ii++) print $1,a[1] "." ii }' txToAcc.tab >> foo.alias
sort foo.alias | uniq > ucscGenes.alias
sort foo.protAlias | uniq > ucscGenes.protAlias
rm foo.alias foo.protAlias
hgLoadSqlTab -notOnServer $tempDb kgAlias $kent/src/hg/lib/kgAlias.sql ucscGenes.alias
hgLoadSqlTab -notOnServer $tempDb kgProtAlias $kent/src/hg/lib/kgProtAlias.sql ucscGenes.protAlias

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
ln -s $dir/knownGene${GENCODE_VERSION}.ix  /gbdb/$db/knownGene${GENCODE_VERSION}.ix
ln -s $dir/knownGene${GENCODE_VERSION}.ixx /gbdb/$db/knownGene${GENCODE_VERSION}.ixx  

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
cut -f 1-10 knownGene.gp | genePredToFakePsl $tempDb stdin kgTargetAli.psl /dev/null
hgLoadPsl $tempDb kgTargetAli.psl

# 3. Ask cluster-admin to start an untranslated, -stepSize=5 gfServer on       
# /gbdb/$db/targetDb/${db}KgSeq${curVer}.2bit 

# 4. On hgwdev, insert new records into blatServers and targetDb, using the 
# host (field 2) and port (field 3) specified by cluster-admin.  Identify the
# blatServer by the keyword "$db"Kg with the version number appended
# untrans gfServer for hg38KgSeq12 on host blat1b, port 17897
hgsql hgcentraltest -e \
      'INSERT into blatServers values ("hg38KgSeq13", "blat1b", 1909, 0, 1);'
hgsql hgcentraltest -e \
            'INSERT into targetDb values("hg38KgSeq13", "GENCODE Genes", \
                     "hg38", "knownGeneV35.kgTargetAli", "", "", \
                              "/gbdb/hg38/targetDb/hg38KgSeq13.2bit", 1, now(), "");'

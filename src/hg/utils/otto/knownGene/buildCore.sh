# Start by extracting the models from the wgEncodeGencode tables
hgsql -e "select * from wgEncodeGencodeComp$GENCODE_VERSION" --skip-column-names $db | cut -f 2-16 |  genePredToBed stdin tmp
hgsql -e "select * from wgEncodeGencodePseudoGene$GENCODE_VERSION" --skip-column-names $db | cut -f 2-16 |  genePredToBed stdin tmp2
sort -k1,1 -k2,2n tmp tmp2 | gzip -c > gencode${GENCODE_VERSION}.bed.gz
rm tmp tmp2

# get current list of ids
zcat gencode${GENCODE_VERSION}.bed.gz |  awk '{print $4}' | sort > newGencodeName.txt

# grab ENST to UC map from the previous set
hgsql $oldDb -Ne "select name,alignId from knownGene" | sort > EnstToUC.txt

# get lastId from last run of the geneset   (human V36)
# lastId 5075761
kgAllocId EnstToUC.txt newGencodeName.txt 5075761 stdout | sort -u >  txToAcc.tab
# lastId 5078938

echo "create database $tempDb" | hgsql ""

# makes knownGene and knownGeneExt
makeGencodeKnownGene -justKnown $db $tempDb $GENCODE_VERSION txToAcc.tab

hgLoadSqlTab -notOnServer $tempDb knownGene $kent/src/hg/lib/knownGene.sql knownGene.gp
#hgsql $db -Ne "create view $tempDb.all_mrna as select * from all_mrna"
hgsql $db -Ne "create view $tempDb.chromInfo as select * from chromInfo"
hgsql $db -Ne "create view $tempDb.trackDb as select * from trackDb"
hgLoadGenePred -genePredExt $tempDb  knownGeneExt knownGeneExt.gp
hgMapToGene -geneTableType=genePred  -type=psl -all -tempDb=$tempDb $db all_mrna knownGene knownToMrna
hgMapToGene -geneTableType=genePred  -tempDb=$tempDb $db refGene knownGene knownToRefSeq
hgMapToGene -geneTableType=genePred  -type=psl -tempDb=$tempDb $db all_mrna knownGene knownToMrnaSingle

# makes kgXref.tab  knownCanonical.tab knownIsoforms.tab kgColor.tab  
makeGencodeKnownGene $db $tempDb $GENCODE_VERSION txToAcc.tab

hgLoadSqlTab -notOnServer $tempDb kgXref $kent/src/hg/lib/kgXref.sql kgXref.tab
hgLoadSqlTab -notOnServer $tempDb knownCanonical $kent/src/hg/lib/knownCanonical.sql knownCanonical.tab
sort knownIsoforms.tab | uniq | hgLoadSqlTab -notOnServer $tempDb knownIsoforms $kent/src/hg/lib/knownIsoforms.sql stdin

# kgColor.tab is "old style" colors
cat  << __EOF__  > colors.sed
s/coding/12\t12\t120/
s/nonCoding/0\t100\t0/
s/pseudo/255\t51\t255/
s/problem/254\t0\t0/
__EOF__

hgsql $db -Ne "select * from wgEncodeGencodeAttrs$GENCODE_VERSION" | tawk '{print $5,$13}' | sed -f colors.sed > colors.txt
hgLoadSqlTab -notOnServer $tempDb kgColor $kent/src/hg/lib/kgColor.sql colors.txt

genePredToProt knownGeneExt.gp /cluster/data/$db/$db.2bit tmp.faa
faFilter -uniq tmp.faa ucscGenes.faa
hgPepPred $tempDb generic knownGenePep ucscGenes.faa

hgsql -e "select * from wgEncodeGencodeComp$GENCODE_VERSION" --skip-column-names $db | cut -f 2-16 |  tawk '{print $1,$13,$14,$8,$15}' | sort | uniq > knownCds.tab
hgLoadSqlTab -notOnServer $tempDb knownCds $kent/src/hg/lib/knownCds.sql knownCds.tab

hgsql -e "select * from wgEncodeGencodeTag$GENCODE_VERSION" --skip-column-names $db |  sort | uniq  > knownToTag.tab
hgLoadSqlTab -notOnServer $tempDb knownToTag $kent/src/hg/lib/knownTo.sql knownToTag.tab

hgsql $tempDb -Ne "select k.name, g.geneId, g.geneStatus, g.geneType,g.transcriptName,g.transcriptType,g.transcriptStatus, g.havanaGeneId,  g.ccdsId, g.level, g.transcriptClass from knownGene k, $db.wgEncodeGencodeAttrs$GENCODE_VERSION g where k.name=g.transcriptId" | sort | uniq > knownAttrs.tab

hgLoadSqlTab -notOnServer $tempDb knownAttrs $kent/src/hg/lib/knownAttrs.sql knownAttrs.tab

hgsql $tempDb -e "select * from knownToMrna" | tail -n +2 | tawk '{if ($1 != last) {print last, count, buffer; count=1; buffer=$2} else {count++;buffer=$2","buffer} last=$1}' | tail -n +2 | sort > tmp1
hgsql $tempDb  -e "select * from knownToMrnaSingle" | tail -n +2 | sort > tmp2
join  tmp2 tmp1 > knownGene.ev
touch oldToNew.tab
txGeneAlias $db $spDb kgXref.tab knownGene.ev oldToNew.tab foo.alias foo.protAlias
tawk '{split($2,a,"."); for(ii = 1; ii <= a[2]; ii++) print $1,a[1] "." ii }' txToAcc.tab >> foo.alias
tawk '{split($1,a,"."); for(ii = 1; ii <= a[2] - 1; ii++) print $1,a[1] "." ii }' txToAcc.tab >> foo.alias
sort foo.alias | uniq > ucscGenes.alias
sort foo.protAlias | uniq > ucscGenes.protAlias
rm foo.alias foo.protAlias
hgLoadSqlTab -notOnServer $tempDb kgAlias $kent/src/hg/lib/kgAlias.sql ucscGenes.alias
hgLoadSqlTab -notOnServer $tempDb kgProtAlias $kent/src/hg/lib/kgProtAlias.sql ucscGenes.protAlias
hgsql $tempDb -N -e 'select kgXref.kgID, spID, alias from kgXref, kgAlias where kgXref.kgID=kgAlias.kgID' > kgSpAlias_0.tmp

zcat gencode${GENCODE_VERSION}.bed.gz > ucscGenes.bed

txBedToGraph ucscGenes.bed ucscGenes ucscGenes.txg
txgAnalyze ucscGenes.txg $genomes/$db/$db.2bit stdout | sort | uniq | bedClip stdin /cluster/data/mm39/chrom.sizes  ucscSplice.bed
hgLoadBed $tempDb knownAlt ucscSplice.bed

txGeneExplainUpdate2 $oldGeneBed ucscGenes.bed kgOldToNew.tab
hgLoadSqlTab -notOnServer $tempDb kg${lastVer}ToKg${curVer} $kent/src/hg/lib/kg1ToKg2.sql kgOldToNew.tab


hgsql $tempDb -Ne "select kgId, geneSymbol, spID from kgXref" > geneNames.txt
genePredToBigGenePred -colors=colors.txt -geneNames=geneNames.txt -known -cds=knownCds.tab   knownGene.gp  stdout | sort -k1,1 -k2,2n >  gencodeAnnot$GENCODE_VERSION.bgpInput

tawk '{print $4,$0}' gencodeAnnot$GENCODE_VERSION.bgpInput | sort > join1
hgsql $db -Ne "select transcriptId, transcriptClass from wgEncodeGencodeAttrs$GENCODE_VERSION" |  sed 's/problem/nonCoding/'| sort > attrs.txt
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
tawk '{print $4,2,"all"}' gencodeAnnot$GENCODE_VERSION.bgpInput | sort > all.txt
sort -k1,1 -k2,2n basic.txt canonical.txt all.txt | tawk '{if ($1 != last) {print last,buff; buff=$3}else {buff=buff "," $3} last=$1} END {print last,buff}' | tail -n +2  > tier.txt
join -t $'\t'   join6 tier.txt > join7
cut -f 2- -d $'\t' join7 | sort -k1,1 -k2,2n > bgpInput.txt

bedToBigBed -extraIndex=name -type=bed12+16 -tab -as=$HOME/kent/src/hg/lib/gencodeBGP.as bgpInput.txt /cluster/data/$db/chrom.sizes $db.gencode$GENCODE_VERSION.bb

ln -s `pwd`/$db.gencode$GENCODE_VERSION.bb /gbdb/$db/gencode/gencode$GENCODE_VERSION.bb


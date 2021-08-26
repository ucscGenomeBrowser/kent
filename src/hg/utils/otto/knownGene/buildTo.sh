#!/bin/sh -ex

{
# knownToLocusLink
#hgsql --skip-column-names -e "select mrnaAcc,locusLinkId from hgFixed.refLink" $db > refToLl.txt
hgsql --skip-column-names -e "select mrnaAcc,locusLinkId from ncbiRefSeqLink where mrnaAcc != ''" $db > refToLl.txt
hgMapToGene -geneTableType=genePred -tempDb=$tempDb $db ncbiRefSeq knownGene knownToLocusLink -lookup=refToLl.txt
rm refToLl.txt

if test "$gtexGeneMode" != ""
then
    hgMapToGene -geneTableType=genePred $db -tempDb=$tempDb -all -type=genePred $gtexGeneMode knownGene knownToGtex
fi

# knownToEnsembl and knownToGencode${GENCODE_VERSION}
awk '{OFS="\t"} {print $4,$4}' ucscGenes.bed | sort | uniq > knownToEnsembl.tab
cp knownToEnsembl.tab knownToGencode${GENCODE_VERSION}.tab
hgLoadSqlTab -notOnServer $tempDb  knownToEnsembl  $kent/src/hg/lib/knownTo.sql  knownToEnsembl.tab
hgLoadSqlTab -notOnServer $tempDb  knownToGencode${GENCODE_VERSION}  $kent/src/hg/lib/knownTo.sql  knownToGencode${GENCODE_VERSION}.tab

# make knownToLynx
# wget "http://lynx.ci.uchicago.edu/downloads/LYNX_GENES.tab"
# awk '{print $2}' LYNX_GENES.tab | sort > lynxExists.txt
# hgsql -e "select geneSymbol,kgId from kgXref" --skip-column-names $tempDb | awk '{if (NF == 2) print}' | sort > geneSymbolToKgId.txt
# join lynxExists.txt geneSymbolToKgId.txt | awk 'BEGIN {OFS="\t"} {print $2,$1}' | sort > knownToLynx.tab
# hgLoadSqlTab -notOnServer $tempDb  knownToLynx $kent/src/hg/lib/knownTo.sql  knownToLynx.tab
# 
# rm lynxExists.txt geneSymbolToKgId.txt

# load malacards table
if test "$malacardTable" != ""
then
    hgsql -e "select geneSymbol,kgId from kgXref" --skip-column-names $tempDb | awk '{if (NF == 2) print}' | sort > geneSymbolToKgId.txt
    hgsql -e "select geneSymbol from malacards" --skip-column-names $db | sort > malacardExists.txt
    join malacardExists.txt  geneSymbolToKgId.txt | awk 'BEGIN {OFS="\t"} {print $2, $1}' > knownToMalacard.txt
    hgLoadSqlTab -notOnServer $tempDb  knownToMalacards $kent/src/hg/lib/knownTo.sql  knownToMalacard.txt
    rm geneSymbolToKgId.txt malacardExists.txt knownToMalacard.txt
fi

#knownToVisiGene
knownToVisiGene $tempDb -probesDb=$db

hgsql $tempDb -e "select geneSymbol,name from knownGene g, kgXref x where g.name=x.kgId " | sort > $tempDb.symbolToId.txt
join -t $'\t'   /hive/groups/browser/wikipediaScrape/symbolToPage.txt $tempDb.symbolToId.txt | tawk '{print $3,$2}' | sort | uniq > $tempDb.idToPage.txt
hgLoadSqlTab $tempDb knownToWikipedia $HOME/kent/src/hg/lib/knownTo.sql $tempDb.idToPage.txt

echo "BuildKnownTo successfully finished"
} > doKnownTo.log < /dev/null 2>&1

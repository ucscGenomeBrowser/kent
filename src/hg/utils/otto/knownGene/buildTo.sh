#!/bin/sh -ex

{
. ./buildEnv.sh

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
#wget "http://lynx.ci.uchicago.edu/downloads/LYNX_GENES.tab"
#awk '{print $2}' LYNX_GENES.tab | sort > lynxExists.txt
#hgsql -e "select geneSymbol,kgId from kgXref" --skip-column-names $tempDb | awk '{if (NF == 2) print}' | sort > geneSymbolToKgId.txt
#join lynxExists.txt geneSymbolToKgId.txt | awk 'BEGIN {OFS="\t"} {print $2,$1}' | sort > knownToLynx.tab
#hgLoadSqlTab -notOnServer $tempDb  knownToLynx $kent/src/hg/lib/knownTo.sql  knownToLynx.tab

#rm lynxExists.txt geneSymbolToKgId.txt

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


if test "$gnfU133TableLookup" != ""
then
    hgMapToGene -geneTableType=genePred -tempDb=$tempDb $db affyU133 knownGene $gnfU133TableLookup
fi

if test "$gnfAtlasTableLookup" != ""
then
    hgMapToGene -geneTableType=genePred -tempDb=$tempDb $db gnfAtlas2 knownGene $gnfAtlasTableLookup '-type=bed 12'

    if test  "$gnfAtlasTablesFixed" != ""
    then
        time hgExpDistance $tempDb $gnfAtlasTablesFixed gnfAtlas2Distance \
                        -lookup=$gnfAtlasTableLookup
    fi
fi

if test "$gnfU95TableLookup" != ""
then
    hgMapToGene -geneTableType=genePred -tempDb=$tempDb $db affyU95 knownGene $gnfU95TableLookup
    if test  "$gnfU95TablesFixed" != ""
    then
        time hgExpDistance $tempDb $gnfU95TablesFixed gnfU95Distance  -lookup=$gnfU95TableLookup
    fi
fi

if test "$hprd_file" != ""
then
    wget "$hprd_website/$hprd_tar"
    tar xvf $hprd_tar
    knownToHprd $tempDb $hprd_file
    hgLoadNetDist $genomes/hg19/p2p/hprd/hprd.pathLengths $tempDb humanHprdP2P \
        -sqlRemap="select distinct value, name from knownToHprd"

    # these should be under a different test, but...
    hgLoadNetDist $genomes/hg19/p2p/vidal/humanVidal.pathLengths $tempDb humanVidalP2P -sqlRemap="select distinct locusLinkID, kgID from hgFixed.refLink,kgXref where hgFixed.refLink.mrnaAcc = kgXref.refSeq"
    hgLoadNetDist $genomes/hg19/p2p/wanker/humanWanker.pathLengths $tempDb humanWankerP2P -sqlRemap="select distinct locusLinkID, kgID from hgFixed.refLink,kgXref where hgFixed.refLink.mrnaAcc = kgXref.refSeq"

fi

# mupit-pdbids.txt was emailed from Kyle Moad (kmoad@insilico.us.com)
# wc -l 
cp /hive/data/outside/mupit/mupit-pdbids.txt .
# get knownGene IDs and associated PDB IDS
# the extDb{Ref} parts come from hg/hgGene/domains.c:domainsPrint()
hgsql -Ne "select kgID, extAcc1 from $tempDb.kgXref x \
    inner join sp180404.extDbRef sp on x.spID = sp.acc \
    inner join sp180404.extDb e on sp.extDb=e.id \
    where x.spID != '' and e.val='PDB' order by kgID" \
    > $tempDb.knownToPdb.txt;
# filter out pdbIds not found in mupit
cat mupit-pdbids.txt | tr '[a-z]' '[A-Z]' | \
    grep -Fwf - $tempDb.knownToPdb.txt >  knownToMupit.txt;
# check that it filtered correctly:
# cut -f2 $db.knownToMuipit.txt | sort -u | wc -l;
# load new table for hgGene/hgc
hgLoadSqlTab $tempDb knownToMupit ~/kent/src/hg/lib/knownTo.sql knownToMupit.txt

# make knownToNextProt
wget "ftp://ftp.nextprot.org/pub/current_release/ac_lists/nextprot_ac_list_all.txt"
awk '{print $0, $0}' nextprot_ac_list_all.txt | sed 's/NX_//' | sort > displayIdToNextProt.txt
hgsql -e "select spID,kgId from kgXref" --skip-column-names $tempDb | awk '{if (NF == 2) print}' | sort > displayIdToKgId.txt
join displayIdToKgId.txt displayIdToNextProt.txt | awk 'BEGIN {OFS="\t"} {print $2,$3}' > knownToNextProt.tab
hgLoadSqlTab -notOnServer $tempDb  knownToNextProt $kent/src/hg/lib/knownTo.sql  knownToNextProt.tab

hgMapToGene -geneTableType=genePred -tempDb=$tempDb $db HInvGeneMrna knownGene knownToHInv


echo "BuildKnownTo successfully finished"
} > doKnownTo.log < /dev/null 2>&1

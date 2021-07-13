#!/bin/bash
set -beEu -x -o pipefail

#	Do not modify this script, modify the source tree copy:
#	kent/src/hg/utils/otto/sarscov2phylo/updateCombinedTree.sh

usage() {
    echo "usage: $0 prevDate problematicSitesVcf"
    echo "This assumes that ncbi.latest and cogUk.latest links/directories have been updated."
}

if [ $# != 2 ]; then
  usage
  exit 1
fi

prevDate=$1
problematicSitesVcf=$2

ottoDir=/hive/data/outside/otto/sarscov2phylo
ncbiDir=$ottoDir/ncbi.latest
cogUkDir=$ottoDir/cogUk.latest
cncbDir=$ottoDir/cncb.latest
gisaidDir=/hive/users/angie/gisaid
minReal=20000
ref2bit=/hive/data/genomes/wuhCor1/wuhCor1.2bit
epiToPublic=$gisaidDir/epiToPublicAndDate.latest
scriptDir=$(dirname "${BASH_SOURCE[0]}")
source $scriptDir/util.sh

today=$(date +%F)
y=$(date +%Y)
m=$(date +%m)
d=$(date +%d)

cd $ottoDir/$today

prevProtobufMasked=$ottoDir/$prevDate/gisaidAndPublic.$prevDate.masked.pb

usherDir=~angie/github/usher
usher=$usherDir/build/usher
matUtils=$usherDir/build/matUtils

renaming=oldAndNewNames

if [ ! -s new.masked.vcf.gz ]; then

# Make lists of sequences already in the tree.
$matUtils extract -i $prevProtobufMasked -u prevNames

# Before updating the tree with new sequences, update the names used in the tree:
# * Sequences that are already in the tree with EPI_ IDs, but that have been mapped to public IDs
# * COG-UK sequences that are in GenBank.  Per Sam Nicholls the isolate alone is enough to identify
#   them -- no need to match country & year which are sometimes incorrect at first.
grep COG-UK/ $ncbiDir/ncbi_dataset.plusBioSample.tsv \
| tawk '{print $6, $4 "/" $6 "/" $3 "|" $1 "|" $3;}' \
| sed -re 's@COG-UK/@@g; s/United Kingdom://; s/(\/[0-9]{4})(-[0-9]+)*/\1/; s/ //g;' \
| sort \
    > cogUkInGenBankIsolateToNewName
fastaNames $cogUkDir/cog_all.fasta.xz > cogUk.names
grep -Fwf <(cut -f 1 cogUkInGenBankIsolateToNewName) cogUk.names \
    > cogUkInGenBank
set +o pipefail
# From 2021-05-04 on there should be no more unrenamed COG-UK/ in prevNames, but doesn't hurt
# to check.
grep COG-UK/ prevNames \
| awk -F\| '{print $1 "\t" $0;}' \
| sed -re 's@COG-UK/@@;' \
| sort \
    > cogUkInGenBankIsolateToPrevName
set -o pipefail
join -t$'\t' cogUkInGenBankIsolateToPrevName cogUkInGenBankIsolateToNewName \
| cut -f 2,3 \
    > prevTree.renaming
# Look for COG-UK isolates in prevNames that have just been added to GenBank and need to be renamed.
# Unfortunately for now we are not getting those from $epiToPublic.
grep -Fwf <(cut -f 1 cogUkInGenBankIsolateToNewName) prevNames \
| awk -F\| '$3 == ""' \
| awk -F/ '{print $2 "\t" $0;}' \
| sort \
    > cogUkInGenBankIsolateToPrevName
join -t$'\t' cogUkInGenBankIsolateToPrevName cogUkInGenBankIsolateToNewName \
| cut -f 2,3 \
    >> prevTree.renaming
# Look for names with EPI_IDs that have just today been mapped to public sequences.
grep EPI_ISL_ prevNames \
| awk -F\| '{print $2 "\t" $0;}' \
| sort \
    > epiToPrevName
set +o pipefail
grep -Fwf <(cut -f 1 epiToPrevName) $epiToPublic \
| grep -v COG-UK/ \
| tawk '{if ($4 != "" && $3 == $2) { print $1, $2 "|" $4; } else if ($4 != "") { print $1, $3 "|" $2 "|" $4; }}' \
| sort \
    > epiToNewName
set -o pipefail
# Argh, missing sequences in COG-UK metadata can mean that a sequence may have been added to the
# tree both with and without EPI ID... so renaming makes a name conflict.
# If there are any of those then prune the sequences with EPI_ID or longer dates, so renaming doesn't
# cause conflict.
comm -12 <(cut -f 2 epiToNewName | sort) <(sort prevNames) > epiToNewNameAlreadyInTree
join -t$'\t' epiToPrevName <(grep -vFwf epiToNewNameAlreadyInTree epiToNewName) \
| cut -f 2,3 \
    >> prevTree.renaming
cp /dev/null dupsToRemove
if [ -s epiToNewNameAlreadyInTree ]; then
    set +o pipefail
    grep -Fwf <(cut -d\| -f 1 epiToNewNameAlreadyInTree) prevNames \
    | grep EPI_ISL \
    | cat \
        >> dupsToRemove
    set -o pipefail
fi
# Don't rename if the final name is already in the tree; remove the dup with the old name.
set +o pipefail
cut -f 2 prevTree.renaming \
| grep -Fwf - prevNames | cat \
    > alreadyThere
grep -Fwf alreadyThere prevTree.renaming | cut -f 1 \
    >> dupsToRemove
# And finally, sometimes there are duplicates due to country or date being corrected in COG-UK
# metadata.
grep -vFwf dupsToRemove prevTree.renaming \
| cut -f 2 | sort | uniq -c | awk '$1 > 1 {print $2;}' \
| cut -d/ -f 2 \
    > cogUkIsolateStillDup
grep -Fwf cogUkIsolateStillDup $cogUkDir/cog_metadata.csv \
| awk -F, '{print $1 "|" $5;}' \
    > cogDupsLatestMetadata
grep -Fwf cogUkIsolateStillDup prevNames \
| grep -vFwf dupsToRemove \
| grep -vFwf cogDupsLatestMetadata \
| cat \
    >> dupsToRemove
set -o pipefail
startingProtobuf=$prevProtobufMasked
if [ -s dupsToRemove ]; then
    startingProtobuf=prevDupTrimmed.pb
    $matUtils extract -i $prevProtobufMasked -p -s dupsToRemove -o $startingProtobuf
fi

if [ -s prevTree.renaming ]; then
    $matUtils mask -r prevTree.renaming -i $startingProtobuf -o prevRenamed.pb >& rename.out
    $matUtils extract -i prevRenamed.pb -u prevNames
else
    ln -sf $startingProtobuf prevRenamed.pb
fi

# OK, now that the tree names are updated, figure out which seqs are already in there and
# which need to be added.
awk -F\| '{if ($3 == "") { print $1; } else { print $2; } }' prevNames \
| grep -E '^[A-Z]{2}[0-9]{6}\.[0-9]' > prevGbAcc
grep -E '^(England|Northern|Scotland|Wales)' prevNames \
| cut -d\| -f1 > prevCogUk
awk -F\| '{if ($3 == "") { print $1; } else { print $2; } }' prevNames \
| grep -E '^EPI_ISL_' > prevGisaid
# Add public sequences that have been mapped to GISAID sequences to prevGisaid.
grep -Fwf prevGbAcc $epiToPublic | cut -f 1 >> prevGisaid
grep -Fwf prevCogUk $epiToPublic | cut -f 1 >> prevGisaid
wc -l prev*

# Get new GenBank sequences with at least $minReal non-N bases.
# Exclude seqs in the tree with EPI IDs that that have been mapped in the very latest $epiToPublic.
set +o pipefail
egrep $'\t''[A-Z][A-Z][0-9]{6}\.[0-9]+' $epiToPublic \
| grep -Fwf prevGisaid - \
| grep -vFwf prevGbAcc \
| cat \
    >> prevGbAcc
set -o pipefail
xzcat $ncbiDir/genbank.fa.xz \
| faSomeRecords -exclude stdin prevGbAcc newGenBank.fa
faSize -veryDetailed newGenBank.fa \
| tawk '$4 < '$minReal' {print $1;}' \
    > gbTooSmall
# NCBI also includes NC_045512 in the download, but that's our reference, so... exclude that too.
set +o pipefail
fastaNames newGenBank.fa | grep NC_045512 >> gbTooSmall
set -o pipefail
faSomeRecords -exclude newGenBank.fa gbTooSmall newGenBank.filtered.fa
faSize newGenBank.filtered.fa

# Get new COG-UK sequences with at least $minReal non-N bases.
# Also exclude cog_all.fasta sequences not found in cog_metadata.csv.
comm -23 <(fastaNames $cogUkDir/cog_all.fasta.xz | sort) \
    <(cut -d, -f1 $cogUkDir/cog_metadata.csv | sort) \
    > cogFaNotMeta
# Also exclude COG-UK sequences that have been added to GenBank (cogUkInGenBank, see above).
xzcat $cogUkDir/cog_all.fasta.xz \
| faSomeRecords -exclude stdin <(cat prevCogUk cogFaNotMeta cogUkInGenBank) newCogUk.fa
faSize -veryDetailed newCogUk.fa \
| tawk '$4 < '$minReal' {print $1;}' \
    > cogUkTooSmall
faSomeRecords -exclude newCogUk.fa cogUkTooSmall newCogUk.filtered.fa
faSize newCogUk.filtered.fa

# Get new GISAID sequences with at least $minReal non-N bases.
xzcat $gisaidDir/gisaid_fullNames_$today.fa.xz \
| sed -re 's/^>.*\|(EPI_ISL_[0-9]+)\|.*/>\1/' \
| faSomeRecords -exclude stdin prevGisaid newGisaid.fa
faSize -veryDetailed newGisaid.fa \
| tawk '$4 < '$minReal' {print $1;}' \
    > gisaidTooSmall
faSomeRecords -exclude newGisaid.fa gisaidTooSmall newGisaid.filtered.fa
faSize newGisaid.filtered.fa

# Exclude public-mapped sequences from newGisaid:
set +o pipefail
fastaNames newGisaid.filtered.fa \
| grep -Fwf - $epiToPublic \
| cut -f 1 \
    > newGisaid.public.names
set -o pipefail
if [ -s newGisaid.public.names ]; then
    faSomeRecords -exclude newGisaid.filtered.fa newGisaid.public.names tmp
    mv tmp newGisaid.filtered.fa
    faSize newGisaid.filtered.fa
fi
cat new*.filtered.fa > new.fa
faSize new.fa

# Use Rob's script that aligns each sequence to NC_045512.2 and concatenates the results
# as a multifasta alignment (from which we can extract VCF with SNVs):
#conda install -c bioconda mafft
alignedFa=new.aligned.fa
rm -f $alignedFa
export TMPDIR=/dev/shm
time bash ~angie/github/sarscov2phylo/scripts/global_profile_alignment.sh \
  -i new.fa \
  -o $alignedFa \
  -t 50
faSize $alignedFa

# Now make a renaming that keeps all the prevNames and adds full names for the new seqs.
tawk '{print $1, $1;}' prevNames > $renaming
if [ -s newCogUk.filtered.fa ]; then
    # Sometimes all of the new COG-UK sequences are missing from cog_metadata.csv -- complained.
    set +o pipefail
    fastaNames newCogUk.filtered.fa \
    | grep -Fwf - $cogUkDir/cog_metadata.csv \
    | awk -F, '{print $1 "\t" $1 "|" $5;}' \
        >> $renaming
    set -o pipefail
fi
if [ -s newGenBank.filtered.fa ]; then
    # Special renaming for COG-UK sequences: strip COG-UK/, add back country and year
    set +o pipefail
    fastaNames newGenBank.filtered.fa \
    | grep COG-UK/ \
    | sed -re 's/[ |].*//' \
    | grep -Fwf - $ncbiDir/ncbi_dataset.plusBioSample.tsv \
    | tawk '{print $1, $4 "/" $6 "/" $3 "|" $1 "|" $3;}' \
    | sed -re 's@COG-UK/@@g; s/United Kingdom://; s/(\/[0-9]{4})(-[0-9]+)*/\1/; s/ //g;' \
        >> $renaming
    fastaNames newGenBank.filtered.fa \
    | grep -v COG-UK/ \
    | sed -re 's/[ |].*//' \
    | grep -Fwf - $ncbiDir/ncbi_dataset.plusBioSample.tsv \
    | tawk '{ if ($3 == "") { $3 = "?"; }
              if ($6 != "") { print $1 "\t" $6 "|" $1 "|" $3; }
              else { print $1 "\t" $1 "|" $3; } }' \
    | cleanGenbank \
    | sed -re 's/ /_/g' \
        >> $renaming
    set -o pipefail
fi
if [ -s newGisaid.filtered.fa ]; then
    zcat $gisaidDir/metadata_batch_$today.tsv.gz \
    | grep -Fwf <(fastaNames newGisaid.filtered.fa) \
    | tawk '{print $3 "\t" $1 "|" $3 "|" $5;}' \
        >> $renaming
fi
wc -l $renaming

# Make masked VCF
tawk '{ if ($1 ~ /^#/) { print; } else if ($7 == "mask") { $1 = "NC_045512v2"; print; } }' \
    $problematicSitesVcf > mask.vcf
# Add masked VCF to previous protobuf
time cat <(twoBitToFa $ref2bit stdout) $alignedFa \
| faToVcf -maxDiff=1000 -excludeFile=../tooManyEpps.ids -verbose=2 stdin stdout \
| vcfRenameAndPrune stdin $renaming stdout \
| vcfFilter -excludeVcf=mask.vcf stdin \
| gzip -c \
    > new.masked.vcf.gz

fi # if [ ! -s new.masked.vcf.gz ]

time $usher -u -T 80 \
    -A \
    -e 5 \
    -v new.masked.vcf.gz \
    -i prevRenamed.pb \
    -o gisaidAndPublic.$today.masked.preTrim.pb \
    >& usher.addNew.log
mv uncondensed-final-tree.nh gisaidAndPublic.$today.preTrim.nwk

# Exclude sequences with a very high number of EPPs from future runs
grep ^Current usher.addNew.log \
| awk '$16 >= 10 {print $8;}' \
| awk -F\| '{ if ($3 == "") { print $1; } else { print $2; } }' \
    >> ../tooManyEpps.ids

# Prune samples with too many private mutations and internal branches that are too long.
$matUtils extract -i gisaidAndPublic.$today.masked.preTrim.pb \
    -a 20 \
    -b 30 \
    -O -o gisaidAndPublic.$today.masked.pb

# Metadata for hgPhyloPlace:
# Header names same as nextmeta (with strain first) so hgPhyloPlace recognizes them:
echo -e "strain\tgenbank_accession\tdate\tcountry\thost\tcompleteness\tlength\tNextstrain_clade\tpangolin_lineage" \
    > gisaidAndPublic.$today.metadata.tsv
# It's not always possible to recreate both old and new names correctly from metadata,
# so make a file to translate accession or COG-UK to the name used in VCF, tree and protobufs.
cut -f 2 $renaming \
| awk -F\| '{ if ($3 == "") { print $1 "\t" $0; } else { print $2 "\t" $0; } }' \
| sort \
    > idToName
# NCBI metadata for COG-UK: strip COG-UK/ & United Kingdom:, add country & year to name
grep COG-UK/ $ncbiDir/ncbi_dataset.plusBioSample.tsv \
| tawk '$8 >= '$minReal' {print $1, $3, $4, $5, $4 "/" $6 "/" $3 "|" $1 "|" $3, $8;}' \
| sed -re 's@COG-UK/@@g; s/United Kingdom://g;  s/(\/[0-9]{4})(-[0-9]+)*/\1/;
           s@Northern Ireland/@NorthernIreland/@;' \
    > tmp
# NCBI metadata for non-COG-UK (strip colon-separated location after country if present):
grep -v COG-UK/ $ncbiDir/ncbi_dataset.plusBioSample.tsv \
| tawk '$8 >= '$minReal' { print $1, $3, $4, $5, $6, $8; }' \
| sed -re 's@\t([A-Za-z -]+):[A-Za-z0-9 .,()_/-]+\t@\t\1\t@;' \
| perl -wpe '@w = split("\t"); $w[4] =~ s/ /_/g; $_ = join("\t", @w);' \
| cleanGenbank \
| sort tmp - > gb.metadata
if [ -e $ncbiDir/lineage_report.csv ]; then
    echo Getting GenBank Pangolin lineages from $ncbiDir/lineage_report.csv
    tail -n+2  $ncbiDir/lineage_report.csv \
    | sed -re 's/^([A-Z][A-Z][0-9]{6}\.[0-9]+)[^,]*/\1/;' \
    | awk -F, '$2 != "" && $2 != "None" {print $1 "\t" $2;}' \
    | sort \
        > gbToLineage
else
    echo Getting GenBank Pangolin lineages from $prevMeta
    zcat $prevMeta \
    | tail -n+2 \
    | tawk '$2 != "" && $8 != "" { print $2, $8; }' \
    | sort \
        > gbToLineage
fi
wc -l gbToLineage
if [ -e $ncbiDir/nextclade.tsv ]; then
    sort $ncbiDir/nextclade.tsv > gbToNextclade
else
    touch gbToNextclade
fi
wc -l gbToNextclade
join -t$'\t' -a 1 gb.metadata gbToNextclade \
| join -t$'\t' -a 1 - gbToLineage \
| tawk '{ if ($2 == "") { $2 = "?"; }
          print $1, $1, $2, $3, $4, "", $6, $7, $8; }' \
| join -t$'\t' -o 1.2,2.2,2.3,2.4,2.5,2.6,2.7,2.8,2.9 idToName - \
    >> gisaidAndPublic.$today.metadata.tsv
# COG-UK metadata:
if [ -e $cogUkDir/nextclade.tsv ]; then
    sort $cogUkDir/nextclade.tsv > cogUkToNextclade
else
    touch cogUkToNextclade
fi
#*** Could also add sequence length to metadata from faSizes output...
tail -n+2 $cogUkDir/cog_metadata.csv \
| awk -F, -v 'OFS=\t' '{print $1, "", $5, $3, "", "", "", $7; }' \
| sed -re 's/UK-ENG/England/; s/UK-NIR/Northern Ireland/; s/UK-SCT/Scotland/; s/UK-WLS/Wales/;' \
| sort \
| join -t$'\t' -a 1 -o 1.1,1.2,1.3,1.4,1.5,1.6,1.7,2.2,1.8 - cogUkToNextclade \
| join -t$'\t' -o 1.2,2.2,2.3,2.4,2.5,2.6,2.7,2.8,2.9 idToName - \
    >> gisaidAndPublic.$today.metadata.tsv
# CNCB metadata:
tail -n+2 $cncbDir/cncb.metadata.tsv \
| tawk '{ if ($3 != "GISAID" && $3 != "GenBank" && $3 != "Genbank") {
            print $2, "", $10, $11, $9, $5, $6} }' \
| sed -re 's@\t([A-Za-z -]+)( / [A-Za-z -'"'"']+)+\t@\t\1\t@;  s/Sapiens/sapiens/;' \
| sort \
| join -t$'\t' -a 1 -o 1.1,1.2,1.3,1.4,1.5,1.6,1.7,2.2 - $cncbDir/nextclade.tsv \
| join -t$'\t' -a 1 -o 1.1,1.2,1.3,1.4,1.5,1.6,1.7,1.8,2.2 - $cncbDir/pangolin.tsv \
| join -t$'\t' -o 1.2,2.2,2.3,2.4,2.5,2.6,2.7,2.8,2.9 idToName - \
    >> gisaidAndPublic.$today.metadata.tsv
wc -l gisaidAndPublic.$today.metadata.tsv
zcat $gisaidDir/metadata_batch_$today.tsv.gz \
| grep -Fwf <(cut -f 2 $renaming | grep EPI_ISL | cut -d\| -f 2) \
| tawk '{print $1 "|" $3 "|" $5, "", $5, $7, $15, $13, $14, $18, $19;}' \
    >> gisaidAndPublic.$today.metadata.tsv
wc -l gisaidAndPublic.$today.metadata.tsv
gzip gisaidAndPublic.$today.metadata.tsv

# version/description files
cncbDate=$(ls -l $cncbDir | sed -re 's/.*cncb\.([0-9]{4}-[0-9][0-9]-[0-9][0-9]).*/\1/')
echo "sarscov2phylo release 13-11-20; GISAID, NCBI and COG-UK sequences downloaded $today; CNCB sequences downloaded $cncbDate" \
    > version.plusGisaid.txt
$matUtils extract -i gisaidAndPublic.$today.masked.pb -u samples.$today
sampleCountComma=$(echo $(wc -l < samples.$today) \
                   | sed -re 's/([0-9]+)([0-9]{3})$/\1,\2/; s/([0-9]+)([0-9]{3},[0-9]{3})$/\1,\2/;')
echo "$sampleCountComma genomes from GISAID, GenBank, COG-UK and CNCB ($today); sarscov2phylo 13-11-20 tree with newer sequences added by UShER" \
    > hgPhyloPlace.plusGisaid.description.txt

# Add nextclade annotations to protobuf
zcat gisaidAndPublic.$today.metadata.tsv.gz \
| tail -n+2 | tawk '$8 != "" {print $8, $1;}' \
| subColumn -miss=/dev/null 1 stdin ../nextcladeToShort cladeToName

time $matUtils annotate -T 50 \
    -l \
    -i gisaidAndPublic.$today.masked.pb \
    -c cladeToName \
    -u mutations.nextclade \
    -D details.nextclade \
    -o gisaidAndPublic.$today.masked.nextclade.pb \
    >& annotate.nextclade.out

# Add pangolin lineage annotations to protobuf.  Use pangoLEARN training samples;
# convert EPI IDs to public to match tree IDs.
tail -n+2 ~angie/github/pango-designation/lineages.metadata.csv \
| grep -vFwf $ottoDir/clades.blackList \
| awk -F, '{print $9 "\t" $2;}' \
| sed -re 's/B\.1\.1\.464\.1/AW.1/;  s/B\.1\.526\.[0-9]+/B.1.526/;' \
| sort > epiExemplarToLineage
subColumn -miss=/dev/null 1 epiExemplarToLineage \
    <(cut -f 1,2 $epiToPublic) stdout \
| sort > idExemplarToLineage
grep -Fwf <(cut -f 1 idExemplarToLineage) samples.$today \
| awk -F\| '{ if ($3 == "") { print $1 "\t" $0 } else { print $2 "\t" $0; } }' \
| sort > idExemplarToName
join -t$'\t' idExemplarToName idExemplarToLineage \
| tawk '{print $3, $2;}' \
| sort > lineageToName
# $epiToPublic maps some cogUkInGenBank EPI IDs to their COG-UK names not GenBank IDs unfortunately
# so some of those don't match between idExemplarToLineage and idExemplarToName.  Find missing
# sequences and try a different means of adding those.
comm -13 <( cut -f 1 idExemplarToName | sort) <(cut -f 1 idExemplarToLineage| sort) \
| grep -Fwf - ~angie/github/pango-designation/lineages.metadata.csv \
| sed -re 's/Northern_/Northern/;' \
| awk -F, '{print $1 "\t" $2;}' \
| sort > exemplarNameNotFoundToLineage
grep -Fwf <(cut -f 1 exemplarNameNotFoundToLineage) samples.$today \
| awk -F\| '{print $1 "\t" $0;}' \
| sort > exemplarNameNotFoundToFullName
join -t$'\t' exemplarNameNotFoundToLineage exemplarNameNotFoundToFullName \
| cut -f 2,3 \
| sort -u lineageToName - ../lineageToName.newLineages \
| sed -re 's/B\.1\.1\.464\.1/AW.1/;' \
> tmp
mv tmp lineageToName

# Yatish's suggestion: use pangolin/pangoLEARN assignments instead of lineages.csv
zcat gisaidAndPublic.$today.metadata.tsv.gz \
| tail -n+2 | tawk '$9 != "" && $9 != "None" {print $9, $1;}' \
| grep -vFwf $ottoDir/clades.blackList \
    > lineageToName.assigned

time $matUtils annotate -T 50 \
    -i gisaidAndPublic.$today.masked.nextclade.pb \
    -c lineageToName \
    -u mutations.pangolin \
    -D details.pangolin \
    -o gisaidAndPublic.$today.masked.nextclade.pangolin.pb \
    >& annotate.pangolin.out

mv gisaidAndPublic.$today.masked{,.unannotated}.pb
ln gisaidAndPublic.$today.masked.nextclade.pangolin.pb gisaidAndPublic.$today.masked.pb

# EPI_ISL_ ID to public sequence name mapping, so if users upload EPI_ISL IDs for which we have
# public names & IDs, we can match them.
cut -f 1,3 $epiToPublic > epiToPublic.latest

# Update links to latest public+GISAID protobuf and metadata in hgwdev cgi-bin directories
for dir in /usr/local/apache/cgi-bin{-angie,-beta,}/hgPhyloPlaceData/wuhCor1; do
    ln -sf `pwd`/gisaidAndPublic.$today.masked.pb $dir/public.plusGisaid.latest.masked.pb
    ln -sf `pwd`/gisaidAndPublic.$today.metadata.tsv.gz \
        $dir/public.plusGisaid.latest.metadata.tsv.gz
    ln -sf `pwd`/hgPhyloPlace.plusGisaid.description.txt $dir/public.plusGisaid.latest.version.txt
    ln -sf `pwd`/epiToPublic.latest $dir/
done

# Extract public samples from tree
$matUtils extract -i gisaidAndPublic.$today.masked.pb -u newNames
grep -v EPI_ISL_ newNames > newPublicNames
$matUtils extract -i gisaidAndPublic.$today.masked.pb \
    -s newPublicNames \
    -O -o public-$today.all.masked.pb
# Extract Newick and VCF from public-only tree
$matUtils extract -i public-$today.all.masked.pb \
    -t public-$today.all.nwk \
    -v public-$today.all.masked.vcf
gzip -f public-$today.all.masked.vcf
zcat gisaidAndPublic.$today.metadata.tsv.gz \
| grep -v EPI_ISL_ \
| gzip -c \
    > public-$today.metadata.tsv.gz

grep -v EPI_ISL_ cladeToName > cladeToPublicName
grep -v EPI_ISL_ lineageToName > lineageToPublicName

# Add nextclade annotations to public protobuf
time $matUtils annotate -T 50 \
    -l \
    -i public-$today.all.masked.pb \
    -c cladeToPublicName \
    -u mutations.nextclade.public \
    -D details.nextclade.public \
    -o public-$today.all.masked.nextclade.pb \
    >& annotate.nextclade.public.out

# Add pangolin lineage annotations to public protobuf
time $matUtils annotate -T 50 \
    -i public-$today.all.masked.nextclade.pb \
    -c lineageToPublicName \
    -u mutations.pangolin.public \
    -D details.pangolin.public \
    -o public-$today.all.masked.nextclade.pangolin.pb \
    >& annotate.pangolin.public.out

# Not all the Pangolin lineages can be assigned nodes so for now just use nextclade
rm public-$today.all.masked.pb
ln public-$today.all.masked.nextclade.pb public-$today.all.masked.pb

cncbDate=$(ls -l $cncbDir | sed -re 's/.*cncb\.([0-9]{4}-[0-9][0-9]-[0-9][0-9]).*/\1/')
echo "sarscov2phylo release 13-11-20; NCBI and COG-UK sequences downloaded $today; CNCB sequences downloaded $cncbDate" \
    > version.txt

$matUtils extract -i public-$today.all.masked.pb -u samples.public.$today
sampleCountComma=$(echo $(wc -l < samples.public.$today) \
                   | sed -re 's/([0-9]+)([0-9]{3})$/\1,\2/; s/([0-9]+)([0-9]{3},[0-9]{3})$/\1,\2/;')
echo "$sampleCountComma genomes from GenBank, COG-UK and CNCB ($today); sarscov2phylo 13-11-20 tree with newer sequences added by UShER" \
    > hgPhyloPlace.description.txt


# Link to public trees download directory hierarchy
archiveRoot=/hive/users/angie/publicTrees
archive=$archiveRoot/$y/$m/$d
mkdir -p $archive
gzip -c public-$today.all.nwk > $archive/public-$today.all.nwk.gz
ln `pwd`/public-$today.all.masked.{pb,vcf.gz} $archive/
gzip -c public-$today.all.masked.pb > $archive/public-$today.all.masked.pb.gz
ln `pwd`/public-$today.metadata.tsv.gz $archive/
gzip -c public-$today.all.masked.nextclade.pangolin.pb \
    > $archive/public-$today.all.masked.nextclade.pangolin.pb.gz
gzip -c cladeToPublicName > $archive/cladeToPublicName.gz
gzip -c lineageToPublicName > $archive/lineageToPublicName.gz
ln `pwd`/hgPhyloPlace.description.txt $archive/public-$today.version.txt

# Update 'latest' in $archiveRoot
ln -f $archive/public-$today.all.nwk.gz $archiveRoot/public-latest.all.nwk.gz
ln -f $archive/public-$today.all.masked.pb $archiveRoot/public-latest.all.masked.pb
ln -f $archive/public-$today.all.masked.pb.gz $archiveRoot/public-latest.all.masked.pb.gz
ln -f $archive/public-$today.all.masked.vcf.gz $archiveRoot/public-latest.all.masked.vcf.gz
ln -f $archive/public-$today.metadata.tsv.gz $archiveRoot/public-latest.metadata.tsv.gz
ln -f $archive/public-$today.version.txt $archiveRoot/public-latest.version.txt

# Update hgdownload-test link for archive
mkdir -p /usr/local/apache/htdocs-hgdownload/goldenPath/wuhCor1/UShER_SARS-CoV-2/$y/$m
ln -sf $archive /usr/local/apache/htdocs-hgdownload/goldenPath/wuhCor1/UShER_SARS-CoV-2/$y/$m

# Update links to latest public protobuf and metadata in hgwdev cgi-bin directories
for dir in /usr/local/apache/cgi-bin{-angie,-beta,}/hgPhyloPlaceData/wuhCor1; do
    ln -sf `pwd`/public-$today.all.masked.pb $dir/public-latest.all.masked.pb
    ln -sf `pwd`/public-$today.metadata.tsv.gz $dir/public-latest.metadata.tsv.gz
    ln -sf `pwd`/hgPhyloPlace.description.txt $dir/public-latest.version.txt
done

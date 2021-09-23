#!/bin/bash
set -beEu -x -o pipefail

#	Do not modify this script, modify the source tree copy:
#	kent/src/hg/utils/otto/sarscov2phylo/updateCombinedTree.sh

usage() {
    echo "usage: $0 prevDate today problematicSitesVcf"
    echo "This assumes that ncbi.latest and cogUk.latest links/directories have been updated."
}

if [ $# != 3 ]; then
  usage
  exit 1
fi

prevDate=$1
today=$2
problematicSitesVcf=$3

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

mkdir -p $ottoDir/$today
cd $ottoDir/$today

prevProtobufMasked=$ottoDir/$prevDate/gisaidAndPublic.$prevDate.masked.pb
prevMeta=$ottoDir/$prevDate/public-$prevDate.metadata.tsv.gz

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
#*** With horrible hack for time being, to mask 21987 (because it messes up the Delta branch)...
#*** TODO: make hgPhyloPlace handle protobufs that don't have all of the latest problematic sites
#*** masked more gracefully, and update the Problematic Sites track.
time cat <(twoBitToFa $ref2bit stdout) $alignedFa \
| faToVcf -maxDiff=1000 \
    -excludeFile=<(cat ../tooManyEpps.ids ../badBranchSeed.ids) \
    -verbose=2 stdin stdout \
| vcfRenameAndPrune stdin $renaming stdout \
| vcfFilter -excludeVcf=mask.vcf stdin \
| tawk '$2 != 21987' \
| gzip -c \
    > new.masked.vcf.gz

fi # if [ ! -s new.masked.vcf.gz ]

time $usher -u -T 80 \
    -A \
    -e 5 \
    --max-parsimony-per-sample 20 \
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
    -b 30 \
    -O -o gisaidAndPublic.$today.masked.pb

$matUtils extract -i gisaidAndPublic.$today.masked.pb -u samples.$today

# Metadata for hgPhyloPlace:
# Header names same as nextmeta (with strain first) so hgPhyloPlace recognizes them:
echo -e "strain\tgenbank_accession\tdate\tcountry\thost\tcompleteness\tlength\tNextstrain_clade\tpangolin_lineage" \
    > gisaidAndPublic.$today.metadata.tsv
# It's not always possible to recreate both old and new names correctly from metadata,
# so make a file to translate accession or COG-UK to the name used in VCF, tree and protobufs.
awk -F\| '{ if ($3 == "") { print $1 "\t" $0; } else { print $2 "\t" $0; } }' samples.$today \
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
| sed -re 's@\t([A-Za-z -]+):[^\t]+\t@\t\1\t@;' \
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
| grep -Fwf <(grep EPI_ISL samples.$today | cut -d\| -f 2) \
| tawk '{print $1 "|" $3 "|" $5, "", $5, $7, $15, $13, $14, $18, $19;}' \
    >> gisaidAndPublic.$today.metadata.tsv
wc -l gisaidAndPublic.$today.metadata.tsv
gzip -f gisaidAndPublic.$today.metadata.tsv

# version/description files
cncbDate=$(ls -l $cncbDir | sed -re 's/.*cncb\.([0-9]{4}-[0-9][0-9]-[0-9][0-9]).*/\1/')
echo "sarscov2phylo release 13-11-20; GISAID, NCBI and COG-UK sequences downloaded $today; CNCB sequences downloaded $cncbDate" \
    > version.plusGisaid.txt
sampleCountComma=$(echo $(wc -l < samples.$today) \
                   | sed -re 's/([0-9]+)([0-9]{3})$/\1,\2/; s/([0-9]+)([0-9]{3},[0-9]{3})$/\1,\2/;')
echo "$sampleCountComma genomes from GISAID, GenBank, COG-UK and CNCB ($today); sarscov2phylo 13-11-20 tree with newer sequences added by UShER" \
    > hgPhyloPlace.plusGisaid.description.txt

# Add nextclade annotations to protobuf
if [ -s $ottoDir/$prevDate/cladeToName ]; then
    # Use yesterday's clade assignments to annotate clades on today's tree
    time $matUtils annotate -T 50 \
        -l \
        -i gisaidAndPublic.$today.masked.pb \
        -c $ottoDir/$prevDate/cladeToName \
        -f 0.95 \
        -D details.nextclade \
        -o gisaidAndPublic.$today.masked.nextclade.pb \
        >& annotate.nextclade
else
    time $matUtils annotate -T 50 \
        -l \
        -i gisaidAndPublic.$today.masked.pb \
        -P ../nextstrain.clade-paths.tsv \
        -o gisaidAndPublic.$today.masked.nextclade.pb
fi

# Add pangolin lineage annotations to protobuf.
if [ -s $ottoDir/$prevDate/lineageToName ]; then
    time $matUtils annotate -T 50 \
        -i gisaidAndPublic.$today.masked.nextclade.pb \
        -c $ottoDir/$prevDate/lineageToName \
        -f 0.95 \
        -D details.pango \
        -o gisaidAndPublic.$today.masked.nextclade.pangolin.pb \
        >& annotate.pango
else
    time $matUtils annotate -T 50 \
        -i gisaidAndPublic.$today.masked.nextclade.pb \
        -P ../pango.clade-paths.tsv \
        -o gisaidAndPublic.$today.masked.nextclade.pangolin.pb
fi

# Replace protobuf with annotated protobuf.
mv gisaidAndPublic.$today.masked{,.unannotated}.pb
ln -f gisaidAndPublic.$today.masked.nextclade.pangolin.pb gisaidAndPublic.$today.masked.pb

# Save clade & lineage annotations for use tomorrow.
$matUtils summary -i gisaidAndPublic.$today.masked.pb -C sample-clades
tail -n+2 sample-clades \
| tawk '{print $2, $1;}' \
| sort > cladeToName
tail -n+2 sample-clades \
| tawk '{print $3, $1;}' \
| sort > lineageToName

# Add clade & lineage from tree to metadata.
zcat gisaidAndPublic.$today.metadata.tsv.gz \
| tail -n+2 \
| sort > tmp1
tail -n+2 sample-clades \
| sort > tmp2
paste <(zcat gisaidAndPublic.$today.metadata.tsv.gz | head -1) \
      <(echo -e "Nextstrain_clade_usher\tpango_lineage_usher") \
    > gisaidAndPublic.$today.metadata.tsv
join -t$'\t' tmp1 tmp2 \
    >> gisaidAndPublic.$today.metadata.tsv
gzip -f gisaidAndPublic.$today.metadata.tsv
rm tmp1 tmp2

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

# Make Taxodium-formatted protobuf for display
zcat /hive/data/genomes/wuhCor1/goldenPath/bigZips/genes/ncbiGenes.gtf.gz > ncbiGenes.gtf
zcat /hive/data/genomes/wuhCor1/wuhCor1.fa.gz > wuhCor1.fa
zcat gisaidAndPublic.$today.metadata.tsv.gz > metadata.tmp.tsv
time $matUtils extract -i gisaidAndPublic.$today.masked.pb \
    -f wuhCor1.fa \
    -g ncbiGenes.gtf \
    -M metadata.tmp.tsv \
    --write-taxodium gisaidAndPublic.$today.masked.taxodium.pb
rm metadata.tmp.tsv wuhCor1.fa
gzip -f gisaidAndPublic.$today.masked.taxodium.pb

$scriptDir/extractPublicTree.sh $today

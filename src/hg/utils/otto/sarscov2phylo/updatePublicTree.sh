#!/bin/bash
set -beEu -x -o pipefail

#	Do not modify this script, modify the source tree copy:
#	kent/src/hg/utils/otto/sarscov2phylo/updatePublicTree.sh

usage() {
    echo "usage: $0 prevDate problematicSitesVcf"
    echo "This assumes that ncbi.latest and cogUk.latest links/directories have been updated."
}

if [ $# != 2 ]; then
  usage
  exit 1
fi

ottoDir=/hive/data/outside/otto/sarscov2phylo
prevDate=$1
problematicSitesVcf=$2
prevVcf=$ottoDir/$prevDate/public-$prevDate.all.vcf.gz
prevProtobufMasked=$ottoDir/$prevDate/public-$prevDate.all.masked.pb
prevProtobufUnmasked=$ottoDir/$prevDate/public-$prevDate.all.notMasked.pb
prevMeta=$ottoDir/$prevDate/public-$prevDate.metadata.tsv.gz

echo "prevVcf=$prevVcf"
echo "prevProtobufMasked=$prevProtobufMasked"
echo "prevProtobufUnmasked=$prevProtobufUnmasked"
echo "prevMeta=$prevMeta"
echo "problematicSitesVcf=$problematicSitesVcf"

ncbiDir=$ottoDir/ncbi.latest
cogUkDir=$ottoDir/cogUk.latest
cncbDir=$ottoDir/cncb.latest
today=$(date +%F)
y=$(date +%Y)
m=$(date +%m)
d=$(date +%d)

minReal=20000
ref2bit=/hive/data/genomes/wuhCor1/wuhCor1.2bit

usherDir=~angie/github/usher
usher=$usherDir/build/usher
matUtils=$usherDir/build/matUtils
find_parsimonious_assignments=~angie/github/strain_phylogenetics/build/find_parsimonious_assignments

scriptDir=$(dirname "${BASH_SOURCE[0]}")

source $scriptDir/util.sh

# Before we get started, make sure cog_metadata has the columns we're expecting:
expectedHeaderStart='sequence_name,country,adm1,pillar_2,sample_date,epi_week,lineage,'
cogUkMetHeader=$(head -1 $cogUkDir/cog_metadata.csv | grep ^$expectedHeaderStart)
# The grep will fail if they change one of the first 7 fields.
# If it fails, update cog_metadata.csv column indices below and in getCogUk.sh too.

mkdir -p $ottoDir/$today
cd $ottoDir/$today

# Disregard pipefail just for this pipe because head prevents the cat from completing:
set +o pipefail
xcat $prevVcf | head | grep ^#CHROM | sed -re 's/\t/\n/g;' | tail -n+10 > prevNames
set -o pipefail
awk -F\| '{if ($3 == "") { print $1; } else { print $2; } }' prevNames \
| grep -E '^[A-Z]{2}[0-9]{6}\.[0-9]' > prevGbAcc
awk -F\| '{if ($3 == "") { print $1; } else { print $2; } }' prevNames \
| grep -E '^(England|Northern|Scotland|Wales)' > prevCogUk

# Get new GenBank sequences with at least $minReal non-N bases.
xzcat $ncbiDir/genbank.fa.xz \
| faSomeRecords -exclude stdin prevGbAcc newGenBank.fa
faSize -veryDetailed newGenBank.fa \
| tawk '$4 < '$minReal' {print $1;}' \
    > gbTooSmall
# NCBI also includes NC_045512 in the download, but that's our reference, so... exclude that too.
fastaNames newGenBank.fa | grep NC_045512 >> gbTooSmall
faSomeRecords -exclude newGenBank.fa gbTooSmall newGenBank.filtered.fa
faSize newGenBank.filtered.fa

# Get new COG-UK sequences with at least $minReal non-N bases.
xzcat $cogUkDir/cog_all.fasta.xz \
| faSomeRecords -exclude stdin prevCogUk newCogUk.fa
faSize -veryDetailed newCogUk.fa \
| tawk '$4 < '$minReal' {print $1;}' \
    > cogUkTooSmall
faSomeRecords -exclude newCogUk.fa cogUkTooSmall newCogUk.filtered.fa
faSize newCogUk.filtered.fa

cat newGenBank.filtered.fa newCogUk.filtered.fa > new.fa

# Some days there are no new public sequences; just link to yesterday's results.
# Temporarily disable pipefail because fastaNames uses grep which fails when there are no sequences.
set +o pipefail
newSeqCount=$(fastaNames new.fa | wc -l)
set -o pipefail
if [ $newSeqCount == 0 ] ; then
    ln -sf $prevVcf public-$today.all.vcf.gz
    ln -sf $prevProtobufMasked public-$today.all.masked.pb
    ln -sf $prevProtobufUnmasked public-$today.all.notMasked.pb
    ln -sf $prevMeta public-$today.metadata.tsv.gz
    echo "No new sequences; linking to yesterday's results."
    exit 0
fi

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

# Now make a renaming that keeps all the prevNames and adds full names (with shortened dates)
# for the new seqs.
renaming=oldAndNewNames
tawk '{print $1, $1;}' prevNames > $renaming
if (( $(fastaNames newCogUk.filtered.fa | wc -l) > 0 )) ; then
    # This grep failed 2021-03-14, crashing the script.  I complained to COG-UK; tolerate.
    set +o pipefail
    fastaNames newCogUk.filtered.fa \
    | grep -Fwf - $cogUkDir/cog_metadata.csv \
    | awk -F, '{print $1 "\t" $1 "|" $5;}' \
    | sed -re 's/20([0-9][0-9])(-[0-9-]+)?$/\1\2/;' \
        >> $renaming
    set -o pipefail
fi
if (( $(fastaNames newGenBank.filtered.fa | wc -l) > 0 )) ; then
    fastaNames newGenBank.filtered.fa \
    | sed -re 's/[ |].*//' \
    | grep -Fwf - $ncbiDir/ncbi_dataset.plusBioSample.tsv \
    | tawk '{ if ($3 == "") { $3 = "?"; }
              if ($6 != "") { print $1 "\t" $6 "|" $1 "|" $3; }
              else { print $1 "\t" $1 "|" $3; } }' \
    | cleanGenbank \
    | sed -re 's/ /_/g' \
    | sed -re 's/20([0-9][0-9])(-[0-9-]+)?$/\1\2/;' \
        >> $renaming
fi
wc -l $renaming

# Make VCF -- first without masking, for hgTracks
time cat <(twoBitToFa $ref2bit stdout) $alignedFa \
| faToVcf stdin stdout \
| vcfRenameAndPrune stdin $renaming stdout \
| gzip -c \
    > new.vcf.gz
# Use usher to add new public sequences to tree of mapped subset and infer missing/ambig bases.
time $usher -u -T 50 \
    -v new.vcf.gz \
    -i $prevProtobufUnmasked \
    -o public-$today.all.notMasked.pb \
    >& usher.addNewUnmasked.log
#~10 hours for 56k seqs, ~72m for 6k
$matUtils extract -i public-$today.all.notMasked.pb -v public-$today.all.vcf
ls -l public-$today.all.vcf
wc -l public-$today.all.vcf
bgzip -f public-$today.all.vcf
tabix -p vcf public-$today.all.vcf.gz

# Then with masking to collapse the tree & make protobuf for usher/hgPhyloPlace:
tawk '{ if ($1 ~ /^#/) { print; } else if ($7 == "mask") { $1 = "NC_045512v2"; print; } }' \
    $problematicSitesVcf > mask.vcf
time vcfFilter -excludeVcf=mask.vcf new.vcf.gz \
| gzip -c \
    > new.masked.vcf.gz
time $usher -u -T 50 \
    -v new.masked.vcf.gz \
    -i $prevProtobufMasked \
    -o public-$today.all.masked.pb \
    >& usher.addNew.log
mv uncondensed-final-tree.nh public-$today.all.nwk
# Masked VCF that goes with the masked protobuf, for public distribution for folks who want
# to run UShER and also have the VCF.
$matUtils extract -i public-$today.all.masked.pb -v public-$today.all.masked.vcf
gzip public-$today.all.masked.vcf

# Make allele-frequency-filtered versions
# Disregard pipefail just for this pipe because head prevents the cat from completing:
set +o pipefail
sampleCount=$(zcat public-$today.all.vcf.gz | head | grep ^#CHROM | sed -re 's/\t/\n/g' \
              | tail -n+10 | wc -l)
set -o pipefail
minAc001=$(( (($sampleCount + 999) / 1000) ))
vcfFilter -minAc=$minAc001 -rename public-$today.all.vcf.gz \
    > public-$today.all.minAf.001.vcf
wc -l public-$today.all.minAf.001.vcf
bgzip -f public-$today.all.minAf.001.vcf
tabix -p vcf public-$today.all.minAf.001.vcf.gz

minAc01=$(( (($sampleCount + 99) / 100) ))
vcfFilter -minAc=$minAc01 -rename public-$today.all.minAf.001.vcf.gz \
    > public-$today.all.minAf.01.vcf
wc -l public-$today.all.minAf.01.vcf
bgzip -f public-$today.all.minAf.01.vcf
tabix -p vcf public-$today.all.minAf.01.vcf.gz

# Parsimony scores on collapsed tree
time $find_parsimonious_assignments --tree public-$today.all.nwk \
    --vcf <(gunzip -c public-$today.all.vcf.gz) \
| tail -n+2 \
| sed -re 's/^[A-Z]([0-9]+)[A-Z,]+.*parsimony_score=([0-9]+).*/\1\t\2/;' \
| tawk '{print "NC_045512v2", $1-1, $1, $2;}' \
| sort -k2n,2n \
    > public-$today.all.parsimony.bg
bedGraphToBigWig public-$today.all.parsimony.bg /hive/data/genomes/wuhCor1/chrom.sizes \
    public-$today.all.parsimony.bw

# Metadata for hgPhyloPlace:
# Header names same as nextmeta (with strain first) so hgPhyloPlace recognizes them:
echo -e "strain\tgenbank_accession\tdate\tcountry\thost\tcompleteness\tlength\tNextstrain_clade\tpangolin_lineage" \
    > public-$today.metadata.tsv
# It's not always possible to recreate both old and new names correctly from metadata,
# so make a file to translate accession or COG-UK to the name used in VCF, tree and protobufs.
cut -f 2 $renaming \
| awk -F\| '{ if ($3 == "") { print $1 "\t" $0; } else { print $2 "\t" $0; } }' \
| sort \
    > idToName
# NCBI metadata (strip colon-separated location after country if present):
tawk '$8 >= 20000 { print $1, $3, $4, $5, $6, $8; }' \
    $ncbiDir/ncbi_dataset.plusBioSample.tsv \
| sed -re 's/\t([A-Za-z -]+):[A-Za-z0-9 ,()_-]+\t/\t\1\t/;' \
| perl -wpe '@w = split("\t"); $w[4] =~ s/ /_/g; $_ = join("\t", @w);' \
| cleanGenbank \
| sort > gb.metadata
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
    >> public-$today.metadata.tsv
# COG-UK metadata:
if [ -e $cogUkDir/nextclade.tsv ]; then
    sort $cogUkDir/nextclade.tsv > cogUkToNextclade
else
    touch cogUkToNextclade
fi
#*** Could also add sequence length to metadata from faSizes output...
tail -n+2 $cogUkDir/cog_metadata.csv \
| awk -F, -v 'OFS=\t' '{print $1, "", $5, $2, "", "", "", $7; }' \
| sort \
| join -t$'\t' -a 1 -o 1.1,1.2,1.3,1.4,1.5,1.6,1.7,2.2,1.8 - cogUkToNextclade \
| join -t$'\t' -o 1.2,2.2,2.3,2.4,2.5,2.6,2.7,2.8,2.9 idToName - \
    >> public-$today.metadata.tsv
# CNCB metadata:
tail -n+2 $cncbDir/cncb.metadata.tsv \
| tawk '{ if ($3 != "GISAID" && $3 != "GenBank" && $3 != "Genbank") {
            print $2, "", $10, $11, $9, $5, $6} }' \
| sed -re 's@\t([A-Za-z -]+)( / [A-Za-z -'"'"']+)+\t@\t\1\t@;  s/Sapiens/sapiens/;' \
| sort \
| join -t$'\t' -a 1 -o 1.1,1.2,1.3,1.4,1.5,1.6,1.7,2.2 - $cncbDir/nextclade.tsv \
| join -t$'\t' -a 1 -o 1.1,1.2,1.3,1.4,1.5,1.6,1.7,1.8,2.2 - $cncbDir/pangolin.tsv \
| join -t$'\t' -o 1.2,2.2,2.3,2.4,2.5,2.6,2.7,2.8,2.9 idToName - \
    >> public-$today.metadata.tsv
wc -l public-$today.metadata.tsv
gzip -f public-$today.metadata.tsv

# Lineage colors:
zcat public-$today.metadata.tsv.gz \
| tail -n+2 \
| tawk '$8 != "" || $9 != "" { print $1, $8, $9, ""; }' \
| sort -u > sampleToLineage
wc -l sampleToLineage
~/kent/src/hg/utils/otto/sarscov2phylo/cladeLineageColors.pl sampleToLineage
rm gisaidColors
mv lineageColors public-$today.lineageColors
mv nextstrainColors public-$today.nextstrainColors
gzip -f public-$today.lineageColors public-$today.nextstrainColors

#***TODO: make acknowledgements.tsv.gz! (see publicCredits.sh)

ncbiDate=$(ls -l $ncbiDir | sed -re 's/.*ncbi\.([0-9]{4}-[0-9][0-9]-[0-9][0-9]).*/\1/')
cogUkDate=$(ls -l $cogUkDir | sed -re 's/.*cogUk\.([0-9]{4}-[0-9][0-9]-[0-9][0-9]).*/\1/')
cncbDate=$(ls -l $cncbDir | sed -re 's/.*cncb\.([0-9]{4}-[0-9][0-9]-[0-9][0-9]).*/\1/')
if [ $ncbiDate == $cogUkDate ]; then
    echo "sarscov2phylo release 13-11-20; NCBI and COG-UK sequences downloaded $ncbiDate; CNCB sequences downloaded $cncbDate" \
        > version.txt
else
    echo "sarscov2phylo release 13-11-20; NCBI sequences downloaded $ncbiDate; COG-UK sequences downloaded $cogUkDate; CNCB sequences downloaded $cncbDate" \
        > version.txt
fi

sampleCountComma=$(echo $sampleCount \
                   | sed -re 's/([0-9]+)([0-9]{3})$/\1,\2/; s/([0-9]+)([0-9]{3},[0-9]{3})$/\1,\2/;')
echo "$sampleCountComma genomes from GenBank, COG-UK and CNCB ($today); sarscov2phylo 13-11-20 tree with newer sequences added by UShER" \
    > hgPhyloPlace.description.txt

cp -p public-$today.all.masked.pb{,.bak}

# Add nextclade annotations to protobuf
zcat public-$today.metadata.tsv.gz \
| tail -n+2 | tawk '$8 != "" {print $8, $1;}' \
| sed -re 's/^20E \(EU1\)/20E.EU1/;' \
    > cladeToPublicName
time $matUtils annotate -T 50 \
    -l \
    -i public-$today.all.masked.pb \
    -c cladeToPublicName \
    -o public-$today.all.masked.nextclade.pb \
    >& annotate.nextclade.out

# Add pangolin lineage annotations to protobuf
zcat public-$today.metadata.tsv.gz \
| tail -n+2 | tawk '$9 != "" {print $9, $1;}' \
    > lineageToPublicName
time $matUtils annotate -T 50 \
    -i public-$today.all.masked.nextclade.pb \
    -c lineageToPublicName \
    -o public-$today.all.masked.nextclade.pangolin.pb \
    >& annotate.pangolin.out

# Not all the Pangolin lineages can be assigned nodes so for now just use nextclade
cp -p public-$today.all.masked.nextclade.pb public-$today.all.masked.pb

# Divide up allele-frequency-filtered versions into quarterly versions
set +o pipefail
zcat public-$today.all.vcf.gz | head | grep ^#CHROM | sed -re 's/\t/\n/g' | tail -n+10 \
| awk -F\| '{if ($3 == "") { print $2 "\t" $0; } else { print $3 "\t" $0;} }' \
| egrep '^[0-9][0-9]-[0-9][0-9]' \
| sed -re 's/^([0-9][0-9])-([0-9][0-9])(-[0-9][0-9])?(.*)/\1\t\2\4/;' \
| tawk '($1 == 19 && $2 == 12) || ($1 > 19 && $2 > 0)' \
| sort > samplesByDate
set -o pipefail

tawk '$1 < 20 || ($1 == 20 && $2 <= 3) { print $3, $3; }' samplesByDate \
    > 20Q1.renaming
tawk '$1 == 20 && $2 >= 4 && $2 <= 6 { print $3, $3; }' samplesByDate \
    > 20Q2.renaming
tawk '$1 == 20 && $2 >= 7 && $2 <= 9 { print $3, $3; }' samplesByDate \
    > 20Q3.renaming
tawk '$1 == 20 && $2 >= 10 { print $3, $3; }' samplesByDate \
    > 20Q4.renaming
tawk '$1 == 21 && $2 <= 3{ print $3, $3; }' samplesByDate \
    > 21Q1.renaming

for q in 20Q1 20Q2 20Q3 20Q4 21Q1; do
    for af in 01 001; do
        vcfRenameAndPrune public-$today.all.minAf.$af.vcf.gz $q.renaming \
            public-$today.$q.minAf.$af.vcf
        wc -l public-$today.$q.minAf.$af.vcf
        bgzip public-$today.$q.minAf.$af.vcf
        tabix -p vcf public-$today.$q.minAf.$af.vcf.gz
    done
done

# Update gbdb links -- not every day, too much churn for getting releases out and the
# tracks are getting unmanageably large for VCF.
if false; then
for f in public-$today.all{,.minAf*}.vcf.gz ; do
    t=$(echo $f | sed -re "s/-$today//;")
    ln -sf `pwd`/$f /gbdb/wuhCor1/sarsCov2PhyloPub/$t
    ln -sf `pwd`/$f.tbi /gbdb/wuhCor1/sarsCov2PhyloPub/$t.tbi
done
ln -sf `pwd`/public-$today.all.nwk /gbdb/wuhCor1/sarsCov2PhyloPub/public.all.nwk
ln -sf `pwd`/public-$today.all.parsimony.bw \
    /gbdb/wuhCor1/sarsCov2PhyloPub/public.all.parsimony.bw
ln -sf `pwd`/public-$today.lineageColors.gz \
    /gbdb/wuhCor1/sarsCov2PhyloPub/public.all.lineageColors.gz
ln -sf `pwd`/public-$today.nextstrainColors.gz \
    /gbdb/wuhCor1/sarsCov2PhyloPub/public.all.nextstrainColors.gz
ln -sf `pwd`/version.txt /gbdb/wuhCor1/sarsCov2PhyloPub/public.all.version.txt
for q in 20Q1 20Q2 20Q3 20Q4 21Q1; do
    for af in 01 001; do
        ln -sf `pwd`/public-$today.$q.minAf.$af.vcf.gz \
            /gbdb/wuhCor1/sarsCov2PhyloPub/public.$q.minAf.$af.vcf.gz
    done
done
fi

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

# Update 'latest' protobuf, metadata and desc in and cgi-bin{,-angie}/hgPhyloPlaceData/wuhCor1/
for dir in /usr/local/apache/cgi-bin{-angie,-beta,}/hgPhyloPlaceData/wuhCor1; do
    ln -sf `pwd`/public-$today.all.masked.pb $dir/public-latest.all.masked.pb
    ln -sf `pwd`/public-$today.metadata.tsv.gz $dir/public-latest.metadata.tsv.gz
    ln -sf `pwd`/hgPhyloPlace.description.txt $dir/public-latest.version.txt
done


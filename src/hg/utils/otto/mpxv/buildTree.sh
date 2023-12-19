#!/bin/bash
source ~/.bashrc
set -beEu -o pipefail

# Make masked VCF from nextalign'd INSDC sequences

mpxvScriptDir=$(dirname "${BASH_SOURCE[0]}")

today=$(date +%F)

mpxvDir=/hive/data/outside/otto/mpxv

# RefSeq assembly for our track hub
asmAcc=GCF_014621545.1
asmDir=$(echo $asmAcc \
    | sed -re 's@^(GC[AF])_([0-9]{3})([0-9]{3})([0-9]{3})\.([0-9]+)@\1/\2/\3/\4/\1_\2\3\4.\5@')

# NC_063383.1 reference
ref2bit=/hive/data/genomes/asmHubs/$asmDir/$asmAcc.2bit

# GenBank flat file for taxonium
gbff=$mpxvDir/GCF_014621545.1_ASM1462154v1_genomic.gbff

mpxvNcbiDir=$mpxvDir/ncbi/ncbi.latest
archiveRoot=/hive/users/angie/publicTreesHMPXV

usherDir=~angie/github/usher
usherSampled=$usherDir/build/usher-sampled
usher=$usherDir/build/usher
matUtils=$usherDir/build/matUtils

mkdir -p $mpxvDir/ncbi/ncbi.$today
$mpxvScriptDir/getNcbiMpxv.sh >& $mpxvDir/ncbi/ncbi.$today/getNcbiMpxv.log

buildDir=$mpxvDir/build/$today
mkdir -p $buildDir
cd $buildDir

# Use metadata to make a renaming file
perl -wne 'chomp; @w=split(/\t/);
    my ($acc, $iso, $loc, $date, $name) = ($w[0], $w[1], $w[3], $w[4], $w[11]);
    if ($iso eq "") { $iso = $name; }
    my $country = $loc;  $country =~ s/:.*//;
    my $COU = $country;  $COU =~ s/^(\w{3}).*/$1/;  $COU = uc($COU);
    if ($country eq "United Kingdom") { $COU = "UK"; }
    if ($iso !~ /$country/ && $iso !~ /\b$COU\b/) { $iso = "$country/$iso"; }
    my $year = $date;  $year =~ s/-.*//;
    if ($iso !~ /$year/) { $iso = "$iso/$year"; }
    my $fullName = "$iso|$acc|$date";  $fullName =~ s/ /_/g;
    print "$acc\t$fullName\n";' \
    $mpxvNcbiDir/metadata.2017outbreak.tsv > renaming.tsv

# Use Nextstrain's exclusion list too.
awk '{print $1;}' ~angie/github/monkeypox/config/exclude_accessions_mpxv.txt \
| grep -Fwf - <(cut -f 1 $mpxvNcbiDir/metadata.2017outbreak.tsv) \
    > exclude.ids

# This builds the whole tree from scratch!  Eventually we'll want to add only the new sequences
# to yesterday's tree.
time cat <(twoBitToFa $ref2bit stdout) <(xzcat $mpxvNcbiDir/nextalign.fa.xz) \
| faToVcf -maxDiff=100 -verbose=2 -excludeFile=exclude.ids -includeNoAltN stdin stdout \
| vcfRenameAndPrune stdin renaming.tsv stdout \
| vcfFilter -excludeVcf=$mpxvScriptDir/mask.vcf.gz stdin \
| pigz -p 8 \
    > all.masked.vcf.gz

echo '()' > emptyTree.nwk
time $usherSampled -T 64 -A -e 5 \
        -t emptyTree.nwk \
        -v all.masked.vcf.gz \
        -o mpxv.$today.masked.preOpt.pb\
        --optimization_radius 0 --batch_size_per_process 10 \
        > usher.addNew.log 2>usher-sampled.stderr

# Optimize:
time ~angie/github/usher_branch/build/matOptimize \
    -T 64 -r 20 -M 2 -S move_log.usher_branch \
    -i mpxv.$today.masked.preOpt.pb \
    -o mpxv.$today.masked.opt.pb \
    >& matOptimize.usher_branch.log
# It crashes when I add
#    -v all.masked.vcf.gz \
# -- bug Cheng later.

# Annotate root nodes for Nextstrain lineages.
join -t$'\t' <(sort renaming.tsv) <(cut -f 1,4 $mpxvNcbiDir/nextclade.tsv | sort) \
| tawk '{print $3, $2;}' | sort > lineageToName
$matUtils annotate -T 30 -i mpxv.$today.masked.opt.pb -c lineageToName \
    -o mpxv.$today.masked.pb \
    >& annotate.log

# Make metadata that uses same names as tree and includes nextclade lineage assignments.
echo -e "strain\tgenbank_accession\tdate\tcountry\tlocation\tlength\thost\tbioproject_accession\tbiosample_accession\tsra_accession\tauthors\tpublications\tNextstrain_lineage" \
    > mpxv.$today.metadata.tsv
join -t$'\t' <(sort renaming.tsv) <(cut -f 1,4 $mpxvNcbiDir/nextclade.tsv | sort) \
| join -t$'\t' -o 1.2,2.1,2.6,2.4,2.5,2.8,2.9,2.10,2.11,2.13,2.14,2.15,1.3 \
    - <(sort $mpxvNcbiDir/metadata.2017outbreak.tsv \
        | perl -F'/\t/' -walne '$F[3] =~ s/(: ?|$)/\t/;  print join("\t", @F);') \
    >> mpxv.$today.metadata.tsv
pigz -f -p 8 mpxv.$today.metadata.tsv

# Make a tree version description for hgPhyloPlace
$matUtils extract -i mpxv.$today.masked.pb -u samples.$today
sampleCount=$(wc -l < samples.$today)
# Sometimes NCBI download is incomplete; don't replace yesterday's tree in that case.
minSamples=3300
if (( $sampleCount < $minSamples )); then
    echo "*** Too few samples ($sampleCount)!  Expected at least $minSamples.  Halting. ***"
    exit 1
fi
sampleCountComma=$(echo $sampleCount \
                   | sed -re 's/([0-9]+)([0-9]{3})$/\1,\2/; s/([0-9]+)([0-9]{3},[0-9]{3})$/\1,\2/;')
echo "$sampleCountComma genomes from INSDC (GenBank/ENA/DDBJ) ($today)" \
    > hgPhyloPlace.description.txt

# Make a taxonium view
usher_to_taxonium --input mpxv.$today.masked.pb \
    --metadata mpxv.$today.metadata.tsv.gz \
    --genbank $gbff \
    --columns genbank_accession,location,date,authors,Nextstrain_lineage \
    --clade_types=pango \
    --output mpxv.$today.masked.taxonium.jsonl.gz \
    >& usher_to_taxonium.log

# Update links to latest protobuf and metadata in hgwdev cgi-bin directories
for dir in /usr/local/apache/cgi-bin{-angie,,-beta}/hgPhyloPlaceData/$asmAcc; do
    ln -sf $(pwd)/mpxv.$today.masked.pb $dir/mpxv.latest.pb
    ln -sf $(pwd)/mpxv.$today.metadata.tsv.gz $dir/mpxv.latest.metadata.tsv.gz
    ln -sf $(pwd)/hgPhyloPlace.description.txt $dir/mpxv.latest.version.txt
done

# Extract Newick and VCF for anyone who wants to download those instead of protobuf
$matUtils extract -i mpxv.$today.masked.pb \
    -t mpxv.$today.nwk \
    -v mpxv.$today.masked.vcf
pigz -p 8 -f mpxv.$today.nwk mpxv.$today.masked.vcf

# Link to public trees download directory hierarchy
read y m d < <(echo $today | sed -re 's/-/ /g')
archive=$archiveRoot/$y/$m/$d
mkdir -p $archive
ln -f $(pwd)/mpxv.$today.{nwk,masked.vcf,metadata.tsv,masked.taxonium.jsonl}.gz $archive/
gzip -c mpxv.$today.masked.pb > $archive/mpxv.$today.masked.pb.gz
ln -f $(pwd)/hgPhyloPlace.description.txt $archive/mpxv.$today.version.txt

# Update 'latest' in $archiveRoot
for f in $archive/mpxv.$today.*; do
    latestF=$(echo $(basename $f) | sed -re 's/'$today'/latest/')
    ln -f $f $archiveRoot/$latestF
done

# Update hgdownload-test link for archive
mkdir -p /usr/local/apache/htdocs-hgdownload/hubs/$asmDir/UShER_hMPXV/$y/$m
ln -sf $archive /usr/local/apache/htdocs-hgdownload/hubs/$asmDir/UShER_hMPXV/$y/$m
# rsync to hgdownload hubs dir
rsync -v -a -L --delete /usr/local/apache/htdocs-hgdownload/hubs/$asmDir/UShER_hMPXV/* \
    qateam@hgdownload.soe.ucsc.edu:/mirrordata/hubs/$asmDir/UShER_hMPXV/

set +o pipefail
grep 'Could not' annotate.log | cat
grep skipping annotate.log | cat
set -o pipefail

cat hgPhyloPlace.description.txt

zcat mpxv.$today.metadata.tsv.gz | tail -n+2 | cut -f 13 | sort | uniq -c

#!/bin/bash
source ~/.bashrc
conda activate viral_usher
set -beEu -o pipefail

# Use viral_usher to build a tree of INSDC measles sequences with nextclade annotations.

measlesScriptDir=$(dirname "${BASH_SOURCE[0]}")

today=$(date +%F)

measlesDir=/hive/data/outside/otto/measles
asmAcc=GCF_000854845.1
archiveRoot=/hive/users/angie/publicTreesMev

usherDir=~angie/github/usher
matUtils=$usherDir/build/matUtils

buildDir=$measlesDir/build/$today
mkdir -p $buildDir
cd $buildDir

# Build tree with viral_usher
configFile=$buildDir/viral_usher_measles.toml
viral_usher init \
            --taxonomy 3052345 \
            --refseq NC_001498.1 \
            --max_parsimony 350 \
            --max_branch_length 1000 \
            --nextclade_dataset nextstrain/measles/genome/WHO-2012 \
            --annotate_allele_frequency 0.99 \
            --annotate_mask_frequency 0.01 \
            --workdir $buildDir \
            --config $configFile
time viral_usher build --config $configFile >& viral_usher.log

# Use my preferred name for the final tree & metadata files
ln -f viz.pb.gz mev.$today.pb.gz
ln -f metadata.tsv.gz mev.$today.metadata.tsv.gz
ln -f tree.jsonl.gz mev.$today.jsonl.gz

# Make description file
$matUtils extract -i viz.pb.gz -u samples.$today >& tmp.log
sampleCountComma=$(wc -l < samples.$today \
                   | sed -re 's/([0-9]+)([0-9]{3})$/\1,\2/; s/([0-9]+)([0-9]{3},[0-9]{3})$/\1,\2/;')
echo "$sampleCountComma genomes from INSDC (GenBank/ENA/DDBJ) ($today)" \
     > hgPhyloPlace.description.txt

# Update links in /gbdb
dir=/gbdb/wuhCor1/hgPhyloPlaceData/$asmAcc
mkdir -p $dir
ln -sf $(pwd)/mev.$today.pb.gz $dir/mev.latest.pb.gz
ln -sf $(pwd)/mev.$today.metadata.tsv.gz $dir/mev.latest.metadata.tsv.gz
ln -sf $(pwd)/hgPhyloPlace.description.txt $dir/mev.latest.version.txt

# Extract Newick and VCF for anyone who wants to download those instead of protobuf
$matUtils extract -i mev.$today.pb.gz \
          -t mev.$today.nwk \
          -v mev.$today.vcf >& tmp.log
pigz -p 8 -f mev.$today.nwk mev.$today.vcf

# Link to public trees download directory hierarchy
for f in mev.$today.{pb,metadata.tsv,jsonl,nwk,vcf}.gz; do
    latestF=$(echo $(basename $f) | sed -re 's/'$today'/latest/')
    ln -f $(pwd)/$f $archiveRoot/$latestF
done

# Update hgdownload-test link for archive
asmDir=$(echo $asmAcc \
         | sed -re 's@^(GC[AF])_([0-9]{3})([0-9]{3})([0-9]{3})\.([0-9]+)@\1/\2/\3/\4/\1_\2\3\4.\5@')
mkdir -p /data/apache/htdocs-hgdownload/hubs/$asmDir/UShER_MeV
ln -sf $archiveRoot/* /data/apache/htdocs-hgdownload/hubs/$asmDir/UShER_MeV/
# rsync to hgdownload hubs dir
for h in hgdownload1 hgdownload3; do
    if rsync -a -L --delete /data/apache/htdocs-hgdownload/hubs/$asmDir/UShER_MeV/* \
             qateam@$h:/mirrordata/hubs/$asmDir/UShER_MeV/; then
        true
    else
        echo ""
        echo "*** rsync to $h failed; disk full? ***"
        echo ""
    fi
done

set +o pipefail
grep "Could not assign" annotate.log | cat
set -o pipefail

echo All done

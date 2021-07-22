#!/bin/bash
source ~/.bashrc
set -beEu -x -o pipefail

# Download SARS-CoV-2 GenBank FASTA and metadata using NCBI Datasets API.
# Use E-Utils to get SARS-CoV-2 metadata from BioSample.
# Use BioSample metadata to fill in gaps in GenBank metadata and report conflicting dates.
# Use enhanced metadata to rewrite FASTA headers for matching up with sequences in other databases.

scriptDir=$(dirname "${BASH_SOURCE[0]}")
source $scriptDir/util.sh

today=$(date +%F)

ottoDir=/hive/data/outside/otto/sarscov2phylo

mkdir -p $ottoDir/ncbi.$today
cd $ottoDir/ncbi.$today

datasets download virus genome taxon 2697049 \
    --exclude-cds \
    --exclude-protein \
    --exclude-gpff \
    --exclude-pdb \
    --filename ncbi_dataset.zip \
|& tail -50 \
    > datasets.log
rm -rf ncbi_dataset
unzip ncbi_dataset.zip
# Creates ./ncbi_dataset/

# This makes something just like ncbi.datasets.tsv from the /table/ API query:
jq -c -r '[.accession, .biosample, .isolate.collectionDate, .location.geographicLocation, .host.sciName, .isolate.name, .completeness, (.length|tostring)] | join("\t")' \
    ncbi_dataset/data/data_report.jsonl \
| sed -e 's/COMPLETE/complete/; s/PARTIAL/partial/;' \
| sort \
    > ncbi_dataset.tsv

# Use EUtils (esearch) to get all SARS-CoV-2 BioSample GI# IDs:
$scriptDir/searchAllSarsCov2BioSample.sh
sort all.biosample.gids.txt > all.biosample.gids.sorted.txt

# Copy yesterday's all.bioSample.txt so we don't have to refetch all the old stuff.
if [ -e ../ncbi.latest/all.bioSample.txt.xz ]; then
    xzcat ../ncbi.latest/all.bioSample.txt.xz > all.bioSample.txt
    grep ^Accession all.bioSample.txt | sed -re 's/^.*ID: //' | sort -u > ids.loaded
    comm -23 all.biosample.gids.sorted.txt ids.loaded > ids.notLoaded
elif [ -e ../ncbi.latest/all.bioSample.txt ]; then
    cp ../ncbi.latest/all.bioSample.txt all.bioSample.txt
    grep ^Accession all.bioSample.txt | sed -re 's/^.*ID: //' | sort -u > ids.loaded
    comm -23 all.biosample.gids.sorted.txt ids.loaded > ids.notLoaded
else
    cp -p all.biosample.gids.txt ids.notLoaded
fi
wc -l ids.notLoaded

# Use EUtils (efetch) to get BioSample records for the GI# IDs that we don't have yet:
time $scriptDir/bioSampleIdToText.sh < ids.notLoaded >> all.bioSample.txt

grep ^Accession all.bioSample.txt | sed -re 's/^.*ID: //' | sort > ids.loaded
comm -23 all.biosample.gids.sorted.txt ids.loaded > ids.notLoaded
if [ -s ids.notLoaded ]; then
    echo Retrying queries for `wc -l < ids.notLoaded` IDs
    $scriptDir/bioSampleIdToText.sh < ids.notLoaded >> all.bioSample.txt
    grep ^Accession all.bioSample.txt | sed -re 's/^.*ID: //' | sort > ids.loaded
    comm -23 all.biosample.gids.sorted.txt ids.loaded > ids.notLoaded
    if [ -s ids.notLoaded ]; then
        echo "Still have only $accCount accession lines after retrying; quitting."
        exit 1
    fi
fi

# Extract properties of interest into tab-sep text:
$scriptDir/bioSampleTextToTab.pl < all.bioSample.txt  > all.bioSample.tab

# Extract BioSample tab-sep lines just for BioSample accessions included in the ncbi_dataset data:
tawk '$2 != "" {print $2;}' ncbi_dataset.tsv \
| grep -Fwf - all.bioSample.tab \
    > gb.bioSample.tab

# Use BioSample metadata to fill in missing pieces of GenBank metadata and report conflicting
# sample collection dates:
$scriptDir/gbMetadataAddBioSample.pl gb.bioSample.tab ncbi_dataset.tsv \
    > ncbi_dataset.plusBioSample.tsv

# Make a file for joining collection date with ID:
tawk '$3 != "" {print $1, $3;}' ncbi_dataset.plusBioSample.tsv \
| sort > gbToDate

# Replace FASTA headers with reconstructed names from enhanced metadata.
time cleanGenbank < ncbi_dataset/data/genomic.fna \
| $scriptDir/fixNcbiFastaNames.pl ncbi_dataset.plusBioSample.tsv \
| xz -T 50 \
    > genbank.fa.xz

# Run pangolin and nextclade on sequences that are new since yesterday
export TMPDIR=/dev/shm
fastaNames genbank.fa.xz | awk '{print $1;}' | sed -re 's/\|.*//' | grep -vx pdb | sort > gb.names
splitDir=splitForNextclade
rm -rf $splitDir
mkdir $splitDir
if [ -e ../ncbi.latest/nextclade.tsv ]; then
    cp ../ncbi.latest/nextclade.tsv .
    cut -f 1 ../ncbi.latest/nextclade.tsv | sort > nextclade.prev.names
    comm -23 gb.names nextclade.prev.names > nextclade.names
    faSomeRecords <(xzcat genbank.fa.xz) nextclade.names nextclade.fa
    faSplit about nextclade.fa 30000000 $splitDir/chunk
else
    cp /dev/null nextclade.tsv
    faSplit about <(xzcat genbank.fa.xz) 30000000 $splitDir/chunk
fi
if (( $(ls -1 splitForNextclade | wc -l) > 0 )); then
    nDataDir=~angie/github/nextclade/data/sars-cov-2
    outDir=$(mktemp -d)
    outTsv=$(mktemp)
    for chunkFa in $splitDir/chunk*.fa; do
        nextclade -j 50 -i $chunkFa \
            --input-root-seq $nDataDir/reference.fasta \
            --input-tree $nDataDir/tree.json \
            --input-qc-config $nDataDir/qc.json \
            --output-dir $outDir \
            --output-tsv $outTsv >& nextclade.log
        cut -f 1,2 $outTsv | tail -n+2 >> nextclade.tsv
        rm $outTsv
    done
    rm -rf $outDir
fi
wc -l nextclade.tsv
rm -rf $splitDir nextclade.fa

conda activate pangolin
runPangolin() {
    fa=$1
    out=$fa.pangolin.csv
    logfile=$(mktemp)
    pangolin $fa --outfile $out > $logfile 2>&1
    rm $logfile
}
export -f runPangolin
if [ -e ../ncbi.latest/lineage_report.csv ]; then
    cp ../ncbi.latest/lineage_report.csv linRepYesterday
    tail -n+2 linRepYesterday | sed -re 's/^([A-Z]+[0-9]+\.[0-9]+).*/\1/' | sort \
        > pangolin.prev.names
    comm -23 gb.names pangolin.prev.names > pangolin.names
    faSomeRecords <(xzcat genbank.fa.xz) pangolin.names pangolin.fa
    pangolin pangolin.fa >& pangolin.log
    tail -n+2 lineage_report.csv >> linRepYesterday
    mv linRepYesterday lineage_report.csv
    rm -f pangolin.fa
else
    splitDir=splitForPangolin
    rm -rf $splitDir
    mkdir $splitDir
    faSplit about <(xzcat genbank.fa.xz) 30000000 $splitDir/chunk
    find $splitDir -name chunk\*.fa \
    | parallel -j 10 "runPangolin {}"
    head -1 $(ls -1 $splitDir/chunk*.csv | head -1) > lineage_report.csv
    for f in $splitDir/chunk*.csv; do
        tail -n+2 $f >> lineage_report.csv
    done
    rm -rf $splitDir
fi
wc -l lineage_report.csv

# It turns out that sometimes new sequences sneak into ncbi_dataset.tsv before they're included in
# genomic.fna.  Filter those out so we don't have missing pangolin and nextclade for just-added
# COG-UK sequences.  (https://github.com/theosanderson/taxodium/issues/10#issuecomment-876800176)
grep -Fwf gb.names ncbi_dataset.plusBioSample.tsv > tmp
wc -l tmp ncbi_dataset.plusBioSample.tsv
mv tmp ncbi_dataset.plusBioSample.tsv

rm -f $ottoDir/ncbi.latest
ln -s ncbi.$today $ottoDir/ncbi.latest

# Clean up
rm -r ncbi_dataset
nice xz all.bioSample.* &

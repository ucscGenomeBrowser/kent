#!/bin/bash
set -beEu -x -o pipefail

#	Do not modify this script, modify the source tree copy:
#	kent/src/hg/utils/otto/sarscov2phylo/updateCombinedTree.sh

usage() {
    echo "usage: $0 prevDate today problematicSitesVcf [baseProtobuf]"
    echo "This assumes that ncbi.latest and cogUk.latest links/directories have been updated."
}

if (( $# != 3 && $# != 4 )); then
  usage
  exit 1
fi

prevDate=$1
today=$2
problematicSitesVcf=$3
if [ $# == 4 ]; then
    baseProtobuf=$4
else
    baseProtobuf=
fi

ottoDir=/hive/data/outside/otto/sarscov2phylo
cncbDir=$ottoDir/cncb.latest
gisaidDir=/hive/users/angie/gisaid
epiToPublic=$gisaidDir/epiToPublicAndDate.latest
scriptDir=$(dirname "${BASH_SOURCE[0]}")
source $scriptDir/util.sh

mkdir -p $ottoDir/$today
cd $ottoDir/$today

usherDir=~angie/github/usher
usher=$usherDir/build/usher-sampled
matUtils=$usherDir/build/matUtils
matOptimize=$usherDir/build/matOptimize

if [ ! -s new.masked.vcf.gz ]; then
    $scriptDir/makeNewMaskedVcf.sh $prevDate $today $problematicSitesVcf $baseProtobuf
fi

if [ ! -s gisaidAndPublic.$today.masked.pb ]; then
    # $scriptDir/usherClusterRun.sh $today
    # Instead of the cluster, use Cheng's blazingly fast new usher-sampled:
    time $usher \
        -T 50 -A -e 5 \
        -i prevRenamed.pb \
        -v new.masked.vcf.gz \
        -o merged.pb \
        --optimization_radius 0 --batch_size_per_process 10 \
        > usher.addNew.log 2>usher-sampled.stderr
    # Branch-specific masking
    time $scriptDir/maskDelta.sh merged.pb merged.deltaMasked.pb
    # Prune samples with too many private mutations and internal branches that are too long.
    $matUtils extract -i merged.deltaMasked.pb \
        --max-parsimony 20 \
        --max-branch-length 60 \
        --max-path-length 225 \
        -O -o merged.deltaMasked.filtered.pb
    # matOptimize: used -r 8 -M2 until 2023-05-12, then switched to Cheng's recommended
    # -m 0.00000001 -M 4 (avoid identical-child-node problem in
    #  https://github.com/sars-cov-2-variants/lineage-proposals/issues/40)
    # The -M 4 allowed up to radius 32, and crazy things started happening all while I was
    # trying to get the tree cleaned up for pango-designation release 1.20 --> lineageTree.
    # After 2023-05-20, when I found that matOptimize had moved a big chunk of B.1 onto a
    # B.1.1.7 garbage branch, causing big trouble for lineageTree (23_05_18_updateLineageTreePb.txt).
    # After that I changed it back to -M 2 for my sanity. If the identical-child thing happens again,
    # then I'll probably just run matOptimize twice, with a small radius the second time.
    time $matOptimize \
        -T 80 -m 0.00000001 -M 2 -S move_log.filtered \
        -i merged.deltaMasked.filtered.pb \
        -o gisaidAndPublic.$today.masked.preTrim.pb \
        >& matOptimize.filtered.log

    # Fix grandparent-reversion nodes that cause some lineages to be incorrectly placed as
    # sublineages of siblings.
    $matUtils fix -i gisaidAndPublic.$today.masked.preTrim.pb -c 10 \
        -o gisaidAndPublic.$today.masked.preTrim.fix.pb

    # Again prune samples with too many private mutations and internal branches that are too long.
    $matUtils extract -i gisaidAndPublic.$today.masked.preTrim.fix.pb \
        --max-parsimony 20 \
        --max-branch-length 60 \
        --max-path-length 225 \
        -O -o gisaidAndPublic.$today.masked.pb
fi

# Exclude sequences with a very high number of EPPs from future runs
grep ^Current usher.addNew.log \
| awk '$16 >= 10 {print $8;}' \
| awk -F\| '{ if ($3 == "") { print $1; } else { print $2; } }' \
    > tooManyEpps.ids
cat tooManyEpps.ids >> ../tooManyEpps.ids

$matUtils extract -i gisaidAndPublic.$today.masked.pb -u samples.$today

$scriptDir/combineMetadata.sh $prevDate $today

# version/description files
cncbDate=$(ls -l $cncbDir | sed -re 's/.*cncb\.([0-9]{4}-[0-9][0-9]-[0-9][0-9]).*/\1/')
echo "sarscov2phylo release 13-11-20; GISAID, NCBI, COG-UK and CNCB sequences downloaded $today" \
    > version.plusGisaid.txt
sampleCountComma=$(echo $(wc -l < samples.$today) \
                   | sed -re 's/([0-9]+)([0-9]{3})$/\1,\2/; s/([0-9]+)([0-9]{3},[0-9]{3})$/\1,\2/;')
echo "$sampleCountComma genomes from GISAID, GenBank, COG-UK and CNCB ($today); sarscov2phylo 13-11-20 tree with newer sequences added by UShER" \
    > hgPhyloPlace.plusGisaid.description.txt

# Add nextclade annotations to protobuf (completely specified by nextstrain.clade-mutations.tsv)
time $matUtils annotate -T 50 \
    -l \
    -i gisaidAndPublic.$today.masked.pb \
    -P $ottoDir/$prevDate/cladeToPath \
    -M $scriptDir/nextstrain.clade-mutations.tsv \
    -D details.nextclade \
    -o gisaidAndPublic.$today.masked.nextclade.pb \
    >& annotate.nextclade

# Add pangolin lineage annotations to protobuf.
time $matUtils annotate -T 50 \
    -i gisaidAndPublic.$today.masked.nextclade.pb \
    -P $ottoDir/$prevDate/lineageToPath \
    -M $scriptDir/pango.clade-mutations.tsv \
    -c $ottoDir/$prevDate/lineageToName \
    -f 0.95 \
    -D details.pango \
    -o gisaidAndPublic.$today.masked.nextclade.pangolin.pb \
    >& annotate.pango

# Replace protobuf with annotated protobuf.
mv gisaidAndPublic.$today.masked.nextclade.pangolin.pb gisaidAndPublic.$today.masked.pb

# Save paths and annotations for use tomorrow.
$matUtils extract -i gisaidAndPublic.$today.masked.pb -C clade-paths
tail -n+2 clade-paths \
| grep -E '^[12]' \
| cut -f 1,3 > cladeToPath
tail -n+2 clade-paths \
| grep -E '^[A-Za-z]' \
| cut -f 1,3 > lineageToPath
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
| cut -f 1-9 \
| sort > tmp1
tail -n+2 sample-clades \
| sort > tmp2
paste <(zcat gisaidAndPublic.$today.metadata.tsv.gz | cut -f 1-9 | head -1) \
      <(echo -e "Nextstrain_clade_usher\tpango_lineage_usher") \
    > gisaidAndPublic.$today.metadata.tsv
join -t$'\t' tmp1 tmp2 \
    >> gisaidAndPublic.$today.metadata.tsv
pigz -p 8 -f gisaidAndPublic.$today.metadata.tsv
rm tmp1 tmp2

# EPI_ISL_ ID to public sequence name mapping, so if users upload EPI_ISL IDs for which we have
# public names & IDs, we can match them.
cut -f 1,3 $epiToPublic > epiToPublic.latest

# Memory-mapped hash tables for metadata and name lookup
tabToMmHash gisaidAndPublic.$today.metadata.tsv.gz gisaidAndPublic.$today.metadata.mmh
ln -sf $(pwd)/gisaidAndPublic.$today.metadata.mmh \
    /gbdb/wuhCor1/hgPhyloPlaceData/public.plusGisaid.latest.metadata.mmh
awk -F\| '{ print $0 "\t" $0;  print $1 "\t" $0; if ($3 != "") { print $2 "\t" $0; } }' \
    samples.$today \
| tawk '$1 != "RNA" && $1 !~ /\/RNA\// && $1 !~/^Germany\/Molecular_surveillance_of_SARS/ && \
        $1 !~ /^Iceland\/SARS-CoV-2_Iceland/' \
    > nameLookup.tab
cut -f 1,3 $epiToPublic \
| subColumn -skipMiss 2 stdin nameLookup.tab tmp.tab
cat tmp.tab >> nameLookup.tab
rm tmp.tab
tabToMmHash nameLookup.tab samples.$today.mmh
rm nameLookup.tab
ln -sf $(pwd)/samples.$today.mmh /gbdb/wuhCor1/hgPhyloPlaceData/public.plusGisaid.names.mmh

# Update links to latest public+GISAID protobuf and metadata in hgwdev cgi-bin directories
pigz -p 8 -c samples.$today > samples.$today.gz
for dir in /usr/local/apache/cgi-bin{-angie,-beta,}/hgPhyloPlaceData/wuhCor1; do
    ln -sf `pwd`/gisaidAndPublic.$today.masked.pb $dir/public.plusGisaid.latest.masked.pb
    ln -sf `pwd`/gisaidAndPublic.$today.metadata.tsv.gz \
        $dir/public.plusGisaid.latest.metadata.tsv.gz
    ln -sf `pwd`/hgPhyloPlace.plusGisaid.description.txt $dir/public.plusGisaid.latest.version.txt
    ln -sf `pwd`/epiToPublic.latest $dir/
    ln -sf `pwd`/samples.$today.gz $dir/public.plusGisaid.names.gz
done

# Make Taxonium v2 protobuf for display
usher_to_taxonium --input gisaidAndPublic.$today.masked.pb \
    --metadata gisaidAndPublic.$today.metadata.tsv.gz \
    --genbank ~angie/github/taxonium/taxoniumtools/test_data/hu1.gb \
    --columns genbank_accession,country,date,pangolin_lineage,pango_lineage_usher \
    --clade_types=nextstrain,pango \
    --name_internal_nodes \
    --title "$today tree with sequences from GISAID, INSDC, COG-UK and CNCB" \
    --output gisaidAndPublic.$today.masked.taxonium.jsonl.gz \
    >& utt.log &

$scriptDir/extractPublicTree.sh $today $prevDate

set +o pipefail
grep skipping annotate* | cat

# Clean up
nice xz -f new*fa &

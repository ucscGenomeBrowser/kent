#!/bin/bash
source ~/.bashrc
set -beEu -o pipefail

# Concatenate the segments of sequences in the 2024 H5N1 D1.1 outbreak and build a tree & metadata.

# References selected from per-segment trees:
# PB2	PQ664446.1	A/chicken/WA/24-030039-002/2024_H5N1|PQ664446.1|2024-10-11
# PB1	PQ798039.1	A/chicken/WA/24-032809-001-original/2024_H5N1|PQ798039.1|2024-11-05
# PA	PQ859340.1	A/chicken/LA/24-037003-001-original/2024_H5N1|PQ859340.1|2024-12-09
# HA	PQ663857.1	A/Guineafowl/WA/24-030328-001-original/2024_H5N1|PQ663857.1|2024-10-15
# NP	PQ585621.1	A/Washington/UW63494/2024_H5N1|PQ585621.1|2024-10-18
# NA	PQ664459.1	A/chicken/WA/24-031352-001-original/2024_H5N1|PQ664459.1|2024-10-21
# MP	PQ663860.1	A/Guineafowl/WA/24-030328-001-original/2024_H5N1|PQ663860.1|2024-10-15
# NS	PQ797957.1	A/chicken/CA/24-032296-001-original/2024_H5N1|PQ797957.1|2024-10-30

today=$1
if [[ $today == "" ]]; then
    today=$(date +%F)
fi

fluADir=/hive/data/outside/otto/fluA
fluANcbiDir=$fluADir/ncbi/ncbi.latest
fluAScriptDir=$(dirname "${BASH_SOURCE[0]}")

archiveRoot=/hive/users/angie/publicTreesFluA
downloadsRoot=/data/apache/htdocs-hgdownload/hubs

assemblyDir=/hive/data/outside/ncbi/genomes
asmAcc=GCF_000864105.1
asmDir=$(echo $asmAcc \
         | sed -re 's@^(GC[AF])_([0-9]{3})([0-9]{3})([0-9]{3})\.([0-9]+)@\1/\2/\3/\4/\1_\2\3\4.\5@')
assemblyReport=$assemblyDir/$asmDir*/$asmAcc*_assembly_report.txt

threads=16


# Assembly reports have segment numbers but not names, so map like this:
function segName {
    case $1 in
        1)
            echo PB2
            ;;
        2)
            echo PB1
            ;;
        3)
            echo PA
            ;;
        4)
            echo HA
            ;;
        5)
            echo NP
            ;;
        6)
            echo NA
            ;;
        7)
            echo MP
            ;;
        8)
            echo NS
            ;;
        *)
            echo ERROR
            ;;
    esac
}

# INSDC accession of basal sequence for each segment
function segRefAcc {
    case $1 in
        1)
            echo PQ664446.1
            ;;
        2)
            echo PQ798039.1
            ;;
        3)
            echo PQ859340.1
            ;;
        4)
            echo PQ663857.1
            ;;
        5)
            echo PQ585621.1
            ;;
        6)
            echo PQ664459.1
            ;;
        7)
            echo PQ663860.1
            ;;
        8)
            echo PQ797957.1
            ;;
        *)
            echo ERROR
            ;;
    esac
}


cd $fluADir/build/$today

# First, for each segment, find the node of the segment tree that corresponds to the outbreak.

# H5N1 asmAcc for all segments except 8:
for seg in 1 2 3 4 5 6 7; do
    segRef=$(grep NC_ $assemblyReport | tawk '$3 == '$seg' {print $7;}')
    if [[ ! -s sample-paths.$asmAcc.$segRef ]]; then
        $matUtils extract -i fluA.$asmAcc.$segRef.$today.pb -S sample-paths.$asmAcc.$segRef \
                  >& tmp.log
    fi
    # NOTE: for segments 1 (PB2), 2 (PB1), 3 (PA) and 4 (HA), we want to pick a node upstream of
    # the chosen segRefAcc (see ~angie/notes/25_02_07_fluA_H5N1_D1.1.txt).
    # 2025-04-18: also go one node back for segment 6 (NA) because of tree instability -- sometimes
    # branch is split.
    if (( $seg < 5 || $seg == 6)); then
        adjust="-1"
    else
        adjust=""
    fi
    cladeNode=$(grep $(segRefAcc $seg) sample-paths.$asmAcc.$segRef \
        | awk '{print $(NF'$adjust');}' \
        | sed -re 's/:.*//;')
    grep -w $cladeNode sample-paths.$asmAcc.$segRef \
    | cut -f 1 > samples.h5n1_D1.1_2024.$seg
done

# segment 8 (NS) ref is not in the H5N1 NS tree (reassortment since original H5N1 NS, too distant
# to align, it's in the H9N2 tree
asmAcc=GCF_000851145.1
seg=8
segRef=NC_004906.1
if [[ ! -s sample-paths.$asmAcc.$segRef ]]; then
    $matUtils extract -i fluA.$asmAcc.$segRef.$today.pb -S sample-paths.$asmAcc.$segRef \
              >& tmp.log
fi
cladeNode=$(grep $(segRefAcc $seg) sample-paths.$asmAcc.$segRef \
    | awk '{print $NF;}' \
    | sed -re 's/:.*//;')
grep -w $cladeNode sample-paths.$asmAcc.$segRef \
| cut -f 1 > samples.h5n1_D1.1_2024.$seg

# Use GenBank sequences found in the trees plus Andersen Lab assembled sequences.
cat samples.h5n1_D1.1_2024.* | grep -v \|SRR | cut -d\| -f 2 \
| grep -Fwf - $fluANcbiDir/metadata.tsv \
| grep -v 1969-12-31 \
| grep -v 1970-01-01 \
| cut -f 2,15 \
| tawk '{ if ($2 != "") { print $2; } else { print $1; } }' \
| sort -u \
| grep -Ff - $fluANcbiDir/metadata.tsv \
| cut -f 1,17 \
    > cladeAccToSeg
# Extract the sequences into per-segment fasta files... renamed from accession to tree name.
# joinSegments.py below will ignore the uniquifying INSDC accession part of names.  Remove the
# uniquifying segment name from Andersen Lab sequences.
for seg in 1 2 3 4 5 6 7 8; do
    tawk '$2 == '$seg' {print $1;}' cladeAccToSeg \
    | faSomeRecords <(xzcat $fluANcbiDir/genbank.fa.xz) stdin stdout \
    | faRenameRecords stdin renaming.tsv.gz h5n1_D1.1_2024.$seg.fa
    segName=$(segName $seg)
    set +o pipefail
    fastaNames $fluADir/andersen_lab.srrNotGb.renamed.fa \
    | grep _$segName \
    | grep -Fwf <(grep \|SRR samples.h5n1_D1.1_2024.* | cut -d\| -f 2) \
    | faSomeRecords $fluADir/andersen_lab.srrNotGb.renamed.fa stdin stdout \
    | sed -re '/^>/ s@_'$segName'/@/@;' \
          >> h5n1_D1.1_2024.$seg.fa
    set -o pipefail
    refAcc=$(segRefAcc $seg)
    nextclade run --input-ref $fluADir/h5n1_D1.1_2024/$refAcc.fa h5n1_D1.1_2024.$seg.fa \
              --excess-bandwidth 9 --terminal-bandwidth 100 --allowed-mismatches 4 \
              --gap-alignment-side right --min-seed-cover 0.1 \
              --output-fasta h5n1_D1.1_2024.$seg.aligned.fa \
              >& nextclade.$seg.log
done

$fluAScriptDir/joinSegments.py \
    --segments h5n1_D1.1_2024.{1,2,3,4,5,6,7,8}.aligned.fa \
    --output h5n1_D1.1_2024.aligned.fa \
    >& joinSegments.log

cat $fluADir/h5n1_D1.1_2024/concat.fa h5n1_D1.1_2024.aligned.fa \
| faToVcf -verbose=2 -includeNoAltN stdin stdout \
| pigz -p 8 \
    > h5n1_D1.1_2024.in.vcf.gz

$usher-sampled -T $threads -A -e 10 \
    -t emptyTree.nwk \
    -v h5n1_D1.1_2024.in.vcf.gz \
    -o h5n1_D1.1_2024.preOpt.pb.gz \
    --optimization_radius 0 --batch_size_per_process 100 \
    > usher.addNew.h5n1_D1.1_2024.log 2> usher-sampled.h5n1_D1.1_2024.stderr

# Optimize:
$matOptimize -T $threads -m 0.00000001 -M 1 -S move_log.h5n1_D1.1_2024 \
    -i h5n1_D1.1_2024.preOpt.pb.gz \
    -v h5n1_D1.1_2024.in.vcf.gz \
    -o h5n1_D1.1_2024.pb.opt.gz \
    >& matOptimize.h5n1_D1.1_2024.log
chmod 664 h5n1_D1.1_2024.pb*

# Collapse nodes and filter out extremely long branches that imply outside-of-outbreak sequences
$matUtils extract -i h5n1_D1.1_2024.pb.opt.gz \
    --max-branch-length 65 \
    -O -o h5n1_D1.1_2024.pb.gz

# Make a tree version description for hgPhyloPlace
$matUtils extract -i h5n1_D1.1_2024.pb.gz -u samples.h5n1_D1.1_2024 \
    >& tmp.log
awk -F\| '{if ($3 == "") { print $1; } else { print $2; }}' samples.h5n1_D1.1_2024 \
    > accs.h5n1_D1.1_2024.tsv
sampleCountComma=$(wc -l < samples.h5n1_D1.1_2024 \
    | sed -re 's/([0-9]+)([0-9]{3})$/\1,\2/; s/([0-9]+)([0-9]{3},[0-9]{3})$/\1,\2/;')
echo "$sampleCountComma genomes from INSDC (GenBank/ENA/DDBJ) or SRA ($today)" \
    > hgPhyloPlace.description.h5n1_D1.1_2024.txt

# Metadata (no need to add clade because the outbreak is all D1.1)
echo -e "strain\tdate\tcountry\tlocation\thost\tbioproject_accession\tbiosample_accession\tsra_accession\tauthors\tpublications" \
    > h5n1_D1.1_2024.metadata.tsv
# INSDC metadata (from MP segment which has most samples; remove uniquifying INSDC accessions)
grep -Fwf <(cut -d\| -f 2 samples.h5n1_D1.1_2024.7 | grep -v ^SRR) $fluANcbiDir/metadata.tsv \
| grep -v 1970-01-01 \
| sort \
| perl -F'/\t/' -walne '$F[3] =~ s/(: ?|$)/\t/;  print join("\t", @F);' \
| join -t$'\t' -o 1.2,2.6,2.4,2.5,2.9,2.10,2.11,2.12,2.14,2.15 \
    <(zcat renaming.tsv.gz | cut -d\| -f 1,3| sort) \
    - \
| sort -k1,1 -u \
| grep -Fwf samples.h5n1_D1.1_2024 \
    >> h5n1_D1.1_2024.metadata.tsv
# Add Andersen lab metadata (from HA segment but remove _HA from names)
grep -Fwf samples.h5n1_D1.1_2024.4 $fluADir/andersen_lab.srrNotGb.renamed.metadata.tsv \
| cut -f 1,3-5,7- \
| sed -re 's@_HA/@/@;' \
| grep -Fwf samples.h5n1_D1.1_2024 \
    >> h5n1_D1.1_2024.metadata.tsv
wc -l h5n1_D1.1_2024.metadata.tsv

# Add Bloom lab's Deep Mutational Scanning scores for HA and PB2
tail -n+2 h5n1_D1.1_2024.metadata.tsv \
| sort \
| join -t$'\t' -a 1 \
    -o 1.1,1.2,1.3,1.4,1.5,1.6,1.7,1.8,1.9,1.10,2.2,2.3,2.4,2.5,2.6,2.7,2.8,2.9,2.10,2.11 \
    - \
    <(zcat H5N1_HA_DMS_metadata.tsv.gz | tail -n+2 \
      | sed -re 's/\|[A-Z]{2}[0-9]{6}\.[0-9]+\|/|/; s@_HA/@/@;' | sort -u) \
| join -t$'\t' -a 1 \
    -o 1.1,1.2,1.3,1.4,1.5,1.6,1.7,1.8,1.9,1.10,1.11,1.12,1.13,1.14,1.15,1.16,1.17,1.18,1.19,1.20,2.2,2.3 \
    - \
    <(zcat PB2_DMS_metadata.tsv.gz | tail -n+2 \
      | sed -re 's/\|[A-Z]{2}[0-9]{6}\.[0-9]+\|/|/; s@_PB2/@/@;' | sort -u) \
    > tmp
oldFields=$(head -1 h5n1_D1.1_2024.metadata.tsv | sed -re 's/\t/\\t/g')
set +o pipefail
haFields=$(zcat H5N1_HA_DMS_metadata.tsv.gz | cut -f 2- | head -1 | sed -re 's/\t/\\t/g')
pb2Fields=$(zcat PB2_DMS_metadata.tsv.gz | cut -f 2-3 | head -1 | sed -re 's/\t/\\t/g')
set -o pipefail
echo -e "$oldFields\t$haFields\t$pb2Fields" > h5n1_D1.1_2024.metadata.tsv
cat tmp >> h5n1_D1.1_2024.metadata.tsv
rm tmp
pigz -f -p 8 h5n1_D1.1_2024.metadata.tsv

usher_to_taxonium --input h5n1_D1.1_2024.pb.gz \
    --metadata h5n1_D1.1_2024.metadata.tsv.gz \
    --columns host,country,location,date,authors,mouse_escape,ferret_escape,cell_entry,stability,sa26_increase,mouse_escape_mutations,ferret_escape_mutations,cell_entry_mutations,stability_mutations,sa26_increase_mutations,mutdiffsel,mutdiffsel_mutations \
    --genbank $fluADir/h5n1_D1.1_2024/concat.gbff \
    --name_internal_nodes \
    --title "2024 H5N1 D1.1 outbreak in USA, concatenated segments from INSDC and SRA ($today)" \
    --config_json $fluAScriptDir/concat.config.json \
    --chronumental \
    --chronumental_steps 500 \
    --chronumental_add_inferred_date chronumental_date \
    --output h5n1_D1.1_2024.jsonl.gz \
    >& utt.log

# Link to /gbdb/ location
dir=/gbdb/wuhCor1/hgPhyloPlaceData/influenzaA/h5n1_D1.1_2024
mkdir -p $dir
ln -sf $(pwd)/h5n1_D1.1_2024.pb.gz $dir/h5n1_D1.1_2024.latest.pb.gz
ln -sf $(pwd)/h5n1_D1.1_2024.metadata.tsv.gz $dir/h5n1_D1.1_2024.latest.metadata.tsv.gz
ln -sf $(pwd)/hgPhyloPlace.description.h5n1_D1.1_2024.txt \
    $dir/h5n1_D1.1_2024.latest.version.txt

# Extract Newick and VCF for anyone who wants to download those instead of protobuf
$matUtils extract -i h5n1_D1.1_2024.pb.gz \
    -t h5n1_D1.1_2024.nwk \
    -v h5n1_D1.1_2024.vcf >& tmp.log
pigz -p 8 -f h5n1_D1.1_2024.nwk h5n1_D1.1_2024.vcf

# Make a ref + all fasta download file for Delphy folks
cat $fluADir/h5n1_D1.1_2024/concat.fa h5n1_D1.1_2024.aligned.fa \
| pigz -p 8 \
    > h5n1_D1.1_2024.msa.fa.gz

# Update 'latest' in $archiveRoot
for e in jsonl.gz metadata.tsv.gz nwk.gz pb.gz vcf.gz msa.fa.gz ; do
    ln -sf $(pwd)/h5n1_D1.1_2024.$e $archiveRoot/h5n1_D1.1_2024.latest.$e
done
ln -sf $(pwd)/hgPhyloPlace.description.h5n1_D1.1_2024.txt \
    $archiveRoot/h5n1_D1.1_2024.latest.version.txt

# Update hgdownload-test link for archive (adding assembly/segRef hierarchy)
mkdir -p $downloadsRoot/$asmDir/UShER_h5n1_D1.1_2024
ln -sf $archiveRoot/h5n1_D1.1_2024.latest.* $downloadsRoot/$asmDir/UShER_h5n1_D1.1_2024/
# rsync to hgdownload hubs dir
for h in hgdownload1 hgdownload3; do
    if rsync -a -L --delete $downloadsRoot/$asmDir/UShER_h5n1_D1.1_2024 \
             qateam@$h:/mirrordata/hubs/$asmDir/ ; then
        true
    else
        echo ""
        echo "*** rsync to $h failed -- disk full ? ***"
        echo ""
    fi
done

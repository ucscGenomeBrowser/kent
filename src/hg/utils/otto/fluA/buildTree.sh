#!/bin/bash
source ~/.bashrc
set -beEu -o pipefail

# Align INSDC sequences to reference and build N*M trees where N = 8 (number of segments in the
# influenza genome) and M = 7 (number of RefSeq genome assemblies)

fluAScriptDir=$(dirname "${BASH_SOURCE[0]}")

if [[ $# > 0 ]]; then
    today=$1
else
    today=$(date +%F)
fi

fluADir=/hive/data/outside/otto/fluA
fluANcbiDir=$fluADir/ncbi/ncbi.latest

usherDir=~angie/github/usher
usherSampled=$usherDir/build/usher-sampled
usher=$usherDir/build/usher
matUtils=$usherDir/build/matUtils
matOptimize=$usherDir/build/matOptimize

minSize=800
threads=16

assemblyDir=/hive/data/outside/ncbi/genomes
asmHubDir=/hive/data/genomes/asmHubs/refseqBuild

archiveRoot=/hive/users/angie/publicTreesFluA
downloadsRoot=/data/apache/htdocs-hgdownload/hubs

# assembly              serotype    taxid    isolate
# GCF_000865085.1       H3N2        335341   A/New York/392/2004(H3N2)
# GCF_001343785.1       H1N1        641809   A/California/07/2009(H1N1)
# GCF_000865725.1       H1N1        211044   A/Puerto Rico/8/1934(H1N1)
# GCF_000928555.1       H7N9        1332244  A/Shanghai/02/2013(H7N9)
# GCF_000864105.1       H5N1        93838    A/goose/Guangdong/1/1996(H5N1)
# GCF_000866645.1       H2N2        488241   A/Korea/426/1968(H2N2)
# GCF_000851145.1       H9N2        130760   A/Hong Kong/1073/99(H9N2)

# segment  product / gene
#       1  polymerase / PB2
#       2  polymerase / PB1
#       3  polymerase / PA
#       4  hemagglutinin / HA         <-- the H
#       5  nucleocapsid protein / NP
#       6  neuraminidase / NA         <-- the N
#       7  matrix protein 1 / M1, " 2 / M2 [join(26..51,740..1007) -- intron??]
#       8  nonstructural protein 1 / NS1, / NEP (aka NS2) [join(27..56,529..864) -- intron??]


if [[ ! -d $fluADir/ncbi/ncbi.$today || ! -s $fluADir/ncbi/ncbi.$today/genbank.fa.xz ]]; then
    mkdir -p $fluADir/ncbi/ncbi.$today
    $fluAScriptDir/getNcbiFluA.sh >& $fluADir/ncbi/ncbi.$today/getNcbiFluA.log
fi

buildDir=$fluADir/build/$today
mkdir -p $buildDir
cd $buildDir

# Use metadata to make a renaming file.  Remove items with patent-prefix accessions and items
# that were cloned many times before sequencing.
# This is really not the same as RSV etc. due to inclusion of serotype.  Also attempting to
# take year from name when it's not in metadata.  Lots of Y2K bugs in input.
tawk '$7 >= '$minSize' && $1 !~ /^(E0|AX|BD|CQ|CS|DD|DI|DJ|DL|DM|FB|FW|FZ|GM|GN|HB|HC|HD|HH|HI|HV|HW|HZ|JA|JB|JC|JD|LF|LG|LP|LQ|LX|LY|LZ|MA|MB|MC|MD|MM|MP|MQ|MS|OF|OG|PA)/' $fluANcbiDir/metadata.tsv \
| grep -vE ' clone ?[0-9]+' \
| $fluAScriptDir/processMetadata.pl \
    > tweakedMetadata.tsv

sort tweakedMetadata.tsv \
| perl -wne 'chomp; @w=split(/\t/);
    my ($acc, $iso, $loc, $date, $str, $type) = ($w[0], $w[1], $w[3], $w[4], $w[14], $w[15]);
    if (! defined $str) { $str = ""; }
    if (! defined $type) { $type = ""; }
    my $name = "";
    if ($str) {
      if ($type && $str !~ /$type/) {
        $name = "${str}_$type";
      } else {
        $name = $str;
      }
    } elsif ($iso) {
      $name = $iso;
    }
    if (! $name && $type) {
      $name = $type;
    }
    $name =~ s/[,;]//g;
    $name =~ s/[() ]/_/g;
    $name =~ s/__/_/g;
    $name =~ s/_$//;
    my $year = $date;  $year =~ s/-.*//;
    if (!$year && $name) {
      if ($name =~ m@/(\d{4})(_+\w+)?$@) {
        $year = $1;
      } elsif ($name =~ m@/(\d{2})(_+\w+)?$@ && int($1) > 24) {
        $year = "19$1";
      }
    }
    my $year2 = $year;   $year2 =~ s/^\d{2}(\d{2})$/$1/;
    if ($name ne "" && $year ne "" && $name !~ /$year/ && $name !~ /\/$year2$/) {
      $name = "$name/$year";
    }
    if ($date eq "") {
      if ($year) {
        $date = $year;
      } else {
        $date = "?";
      }
    }
    my $fullName = $name ? "$name|$acc|$date" : "$acc|$date";
    $fullName =~ s/[ ,:()]/_/g;
    print "$acc\t$fullName\n";' \
| sed -re 's/[():'"'"'\\]/_/g; s/\[/_/g; s/\]/_/g;' \
| sed -re 's/_+\|/|/;' \
| sed -re 's/\t_/\t/;' \
| sed -re "s/'//g;" \
    > renaming.tsv

# Add h5nx from collab
tawk '{print $1, $1;}' $fluADir/h5nx.epiNoMatchRenamed.metadata.tsv >> renaming.tsv

# Add SRA assemblies
tawk '{print $1, $1;}' $fluADir/andersen_lab.srrNotGb.renamed.metadata.tsv >> renaming.tsv

# Tally up Bloom lab Deep Mutational Scanning (DMS) scores for H5N1 HA and pan-avian PB2
$fluAScriptDir/runDms.sh
$fluAScriptDir/runDmsPb2.sh

# This builds the whole tree from scratch!  Eventually we'll want to add only the new sequences
# to yesterday's tree.

echo '()' > emptyTree.nwk


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


for asmAcc in GCF_000864105.1 GCF_000865085.1 GCF_001343785.1 GCF_000865725.1 GCF_000928555.1 \
              GCF_000866645.1 GCF_000851145.1; do
    asmDir=$(echo $asmAcc \
        | sed -re 's@^(GC[AF])_([0-9]{3})([0-9]{3})([0-9]{3})\.([0-9]+)@\1/\2/\3/\4/\1_\2\3\4.\5@')
    assemblyReport=$assemblyDir/$asmDir*/$asmAcc*_assembly_report.txt
    strain=$(grep '^# Organism name' $assemblyReport \
             | sed -re 's/.* Influenza A virus \(//; s/\)\).*/)/;')
    segRefs=$(tawk '$8 == "Primary Assembly" && $7 != "na" {print $7;}' $assemblyReport)
    for segRef in $segRefs; do
        segment=$(grep -F $segRef $assemblyReport | cut -f 3)
        segName=$(segName $segment)
        echo "Starting $asmAcc $segRef $strain segment $segment ($segName)"

        # Run nextclade with RefSeq segment as reference.
        if [[ -s aligned.$asmAcc.$segRef.fa.xz ]]; then
            echo "aligned.$asmAcc.$segRef.fa.xz already exists -- skipping nextalign."
        elif [[ $asmAcc == GCF_000864105.1 ]]; then
            # H5N1: align seqs from collab and SRA assemblies in addition to INSDC
            cat $fluADir/h5nx.epiNoMatchRenamed.fa \
                $fluADir/andersen_lab.srrNotGb.renamed.fa \
                <(xzcat $fluADir/ncbi/ncbi.$today/genbank.fa.xz) \
            | nextclade run --input-dataset $fluADir/nextclade/$asmAcc/$segRef \
                --include-reference \
                --jobs $threads \
                --output-fasta aligned.$asmAcc.$segRef.fa.xz \
                >& nextalign.log
        else
            # Align SRA assemblies in addition to INSDC
            cat $fluADir/andersen_lab.srrNotGb.renamed.fa \
                <(xzcat $fluADir/ncbi/ncbi.$today/genbank.fa.xz) \
            | nextclade run --input-dataset $fluADir/nextclade/$asmAcc/$segRef \
                --include-reference \
                --jobs $threads \
                --output-fasta aligned.$asmAcc.$segRef.fa.xz \
                >& nextalign.log
        fi

        # Add -excludeFile=$fluAScriptDir/exclude.ids if we need to exclude any in the future.
        time faToVcf -verbose=2 -includeNoAltN \
            <(xzcat aligned.$asmAcc.$segRef.fa.xz) stdout \
        | vcfRenameAndPrune stdin renaming.tsv stdout \
        | pigz -p 8 \
            > all.$asmAcc.$segRef.vcf.gz

        time $usherSampled -T $threads -A -e 5 \
            -t emptyTree.nwk \
            -v all.$asmAcc.$segRef.vcf.gz \
            -o fluA.$asmAcc.$segRef.$today.preFilter.pb \
            --optimization_radius 0 --batch_size_per_process 100 \
            > usher.addNew.$asmAcc.$segRef.log 2>usher-sampled.$asmAcc.$segRef.stderr

        # Filter out branches that are so long they must lead to some other type
        $matUtils extract -i fluA.$asmAcc.$segRef.$today.preFilter.pb \
            --max-branch-length 250 \
            --max-parsimony 100 \
            -O -o fluA.$asmAcc.$segRef.$today.preOpt.pb >& tmp.log

        # Optimize:
        time $matOptimize -T $threads -m 0.00000001 -M 1 -S move_log.$asmAcc.$segRef \
            -i fluA.$asmAcc.$segRef.$today.preOpt.pb \
            -v all.$asmAcc.$segRef.vcf.gz \
            -o fluA.$asmAcc.$segRef.$today.pb \
            >& matOptimize.$asmAcc.$segRef.log
        chmod 664 fluA.$asmAcc.$segRef.$today.pb*

        # Make a tree version description for hgPhyloPlace
        $matUtils extract -i fluA.$asmAcc.$segRef.$today.pb -u samples.$asmAcc.$segRef.$today \
            >& tmp.log
        awk -F\| '{if ($3 == "") { print $1; } else { print $2; }}' samples.$asmAcc.$segRef.$today \
            > accs.$asmAcc.$segRef.tsv
                          
        sampleCountComma=$(wc -l < samples.$asmAcc.$segRef.$today \
            | sed -re 's/([0-9]+)([0-9]{3})$/\1,\2/; s/([0-9]+)([0-9]{3},[0-9]{3})$/\1,\2/;')
        if [[ $asmAcc == GCF_000864105.1 ]]; then
            echo "$sampleCountComma genomes from INSDC (GenBank/ENA/DDBJ) and/or GISAID ($today)" \
                > hgPhyloPlace.description.$asmAcc.$segRef.txt
        else
            echo "$sampleCountComma genomes from INSDC (GenBank/ENA/DDBJ) ($today)" \
                > hgPhyloPlace.description.$asmAcc.$segRef.txt
        fi

        # Depending on the segment RefSeq, maybe run nextclade
        # Note: nextclade has a dataset flu_h3n2_na but it does not assign clades.
        case $segRef in
        "NC_007366.1")
            nextcladeName=flu_h3n2_ha
            ;;
        "NC_026433.1")
            nextcladeName=flu_h1n1pdm_ha
            ;;
        "NC_026434.1")
            nextcladeName=flu_h1n1pdm_na
            ;;
        "NC_007362.1")
            nextcladeName=community/moncla-lab/iav-h5/ha/all-clades
            ;;
        *)
            nextcladeName=""
            nextcladeTaxCo=""
            ;;
        esac
        if [[ x$nextcladeName != x ]]; then
            nextclade dataset get --name $nextcladeName --output-zip $nextcladeName.zip
            (if [[ $segRef == "NC_007362.1" ]]; then
                 # Also run on collab's sequences and SRA assemblies
                 time cat <(faSomeRecords <(xzcat $fluADir/ncbi/ncbi.$today/genbank.fa.xz) \
                                accs.$asmAcc.$segRef.tsv stdout) \
                          $fluADir/h5nx.epiNoMatchRenamed.fa \                          $fluADir/andersen_lab.srrNotGb.renamed.fa
             else
                 time faSomeRecords <(xzcat $fluADir/ncbi/ncbi.$today/genbank.fa.xz) \
                         accs.$asmAcc.$segRef.tsv stdout
             fi) \
            | nextclade run \
                -D $nextcladeName.zip \
                -j $threads \
                --retry-reverse-complement true \
                --output-tsv nextclade.$asmAcc.$segRef.tsv \
                --output-columns-selection seqName,clade,totalSubstitutions,totalDeletions,totalInsertions,totalMissing,totalNonACGTNs,alignmentStart,alignmentEnd,substitutions,deletions,insertions,aaSubstitutions,aaDeletions,aaInsertions,missing,unknownAaRanges,nonACGTNs \
                >& nextclade.$asmAcc.$segRef.log
            nextcladeTaxCo=",Nextstrain_clade"
        fi

        # Make metadata that uses same names as tree
        echo -e "strain\tgenbank_accession\tdate\tcountry\tlocation\tlength\thost\tbioproject_accession\tbiosample_accession\tsra_accession\tauthors\tpublications\tserotype\tsegment" \
            > fluA.$asmAcc.$segRef.$today.metadata.tsv
        grep -Fwf accs.$asmAcc.$segRef.tsv tweakedMetadata.tsv \
        | sort \
        | perl -F'/\t/' -walne '$F[3] =~ s/(: ?|$)/\t/;  print join("\t", @F);' \
        | join -t$'\t' -o 1.2,2.1,2.6,2.4,2.5,2.8,2.9,2.10,2.11,2.12,2.14,2.15,2.17,2.18 \
            <(sort renaming.tsv) \
            - \
        | sort \
            >> fluA.$asmAcc.$segRef.$today.metadata.tsv

        # Add columns: GenoFlu genotype and segment type
        csvtk join -t --left-join fluA.$asmAcc.$segRef.$today.metadata.tsv $fluADir/genoflu/genoflu.tsv \
              > tmp
        mv tmp fluA.$asmAcc.$segRef.$today.metadata.tsv

        if [[ x$nextcladeName != x ]]; then
            # Add column: nextclade assignment
            cut -f 1,2 nextclade.$asmAcc.$segRef.tsv \
            | csvtk rename -t -f clade -n Nextstrain_clade \
            | csvtk replace -t -f seqName -p '(.*)' -r '{kv}' -k renaming.tsv \
            | csvtk join -t --left-join fluA.$asmAcc.$segRef.$today.metadata.tsv - \
                > tmp
            mv tmp fluA.$asmAcc.$segRef.$today.metadata.tsv
        fi

        if [[ $segRef == "NC_007362.1" ]]; then
            # Add columns: Bloom lab's H5N1 HA Deep Mutational Scanning scores
            csvtk join -t --left-join fluA.$asmAcc.$segRef.$today.metadata.tsv H5N1_HA_DMS_metadata.tsv \
                  > tmp
            mv tmp fluA.$asmAcc.$segRef.$today.metadata.tsv
        fi
        if [[ $segment == 1 ]]; then
            # Add columns: Bloom lab's Avian PB2 Differential selection to metadata for all PB2 (segment 1)
            csvtk join -t --left-join fluA.$asmAcc.$segRef.$today.metadata.tsv PB2_DMS_metadata.tsv \
                  > tmp
            mv tmp fluA.$asmAcc.$segRef.$today.metadata.tsv
        fi
        if [[ $asmAcc == GCF_000864105.1 ]]; then
            # Add rows: h5nx from collab
            grep -Fwf samples.$asmAcc.$segRef.$today $fluADir/h5nx.epiNoMatchRenamed.metadata.tsv \
            | tawk '{print $1, "", $3, $4, $5, "?", $6, "", "", "", $7, $8, $9, $10;}' \
                   > tmp
            # Proceed only if tmp is non-empty because csvtk join just barfs the second file if the first
            # file is empty, instead of doing a proper left join with empty!
            if [[ -s tmp ]]; then
                csvtk join -t --left-join tmp $fluADir/genoflu/genoflu.tsv \
                | sort \
                      > tmp2
                mv tmp2 tmp
                if [[ $segRef == "NC_007362.1" ]]; then
                    # Add clades and Bloom lab DMS scores
                    join -t$'\t' -a1 -o auto \
                         tmp <(cut -f 1,2 nextclade.$asmAcc.$segRef.tsv | sort) \
                    | join -t$'\t' -a1 -o auto \
                           - <(tail -n+2 H5N1_HA_DMS_metadata.tsv | sort) \
                           >> fluA.$asmAcc.$segRef.$today.metadata.tsv
                elif [[ $segment == 1 ]]; then
                    join -t$'\t' -a1 -o auto tmp <(tail -n+2 PB2_DMS_metadata.tsv | sort) \
                         >> fluA.$asmAcc.$segRef.$today.metadata.tsv
                else
                    cat tmp \
                        >> fluA.$asmAcc.$segRef.$today.metadata.tsv
                fi
            fi
            rm tmp
            # Add rows: SRA runs
            if [[ $segRef == "NC_007362.1" ]]; then
                grep -Fwf samples.$asmAcc.$segRef.$today \
                    $fluADir/andersen_lab.srrNotGb.renamed.metadata.tsv \
                | csvtk join -t --left-join - $fluADir/genoflu/genoflu.tsv \
                | sort \
                | join -t$'\t' -a1 -o auto \
                    - <(cut -f 1,2 nextclade.$asmAcc.$segRef.tsv | sort) \
                | join -t$'\t' -a1 -o auto \
                 - <(tail -n+2 H5N1_HA_DMS_metadata.tsv | sort) \
                    >> fluA.$asmAcc.$segRef.$today.metadata.tsv
            elif [[ $segRef == "NC_007357.1" ]]; then
                # Skip; SRA PB2 (segment 1) with Bloom lab DMS scores is added below
                true
            else
                set +o pipefail
                grep -Fwf samples.$asmAcc.$segRef.$today \
                    $fluADir/andersen_lab.srrNotGb.renamed.metadata.tsv \
                | cat \
                    > tmp
                set -o pipefail
                # Proceed only if tmp is non-empty because csvtk join just barfs the second file if the first
                # file is empty, instead of doing a proper left join with empty!
                if [[ -s tmp ]]; then
                    csvtk join -t --left-join tmp $fluADir/genoflu/genoflu.tsv \
                          >> fluA.$asmAcc.$segRef.$today.metadata.tsv
                fi
            fi
        fi
        if [[ $segment == 1 ]]; then
            # Add SRA PB2 metadata for all PB2 (segment 1) -- there was reassortment so when all
            # references are considered, GsGd (GCF_000864105.1 / NC_007357.1) is not the best match.
            grep -Fwf samples.$asmAcc.$segRef.$today \
                $fluADir/andersen_lab.srrNotGb.renamed.metadata.tsv \
            | csvtk join -t --left-join - $fluADir/genoflu/genoflu.tsv \
            | sort \
            | join -t$'\t' -a1 -o auto \
                - <(tail -n+2 PB2_DMS_metadata.tsv | sort) \
                >> fluA.$asmAcc.$segRef.$today.metadata.tsv
        fi
        wc -l fluA.$asmAcc.$segRef.$today.metadata.tsv
        pigz -f -p 8 fluA.$asmAcc.$segRef.$today.metadata.tsv

        # Make a taxonium view
        if [[ $asmAcc == GCF_000864105.1 ]]; then
            gisaid=" and/or GISAID"
        else
            gisaid=""
        fi
        title="Influenza A $strain segment $segment ($segName) $today tree with $sampleCountComma genomes from INSDC$gisaid"
        columns="genbank_accession,country,location,date,host,serotype,segment,genoflu_genotype,genoflu_segtype,authors,bioproject_accession$nextcladeTaxCo"
        config_json=""
        if [[ $segRef == "NC_007362.1" ]]; then
            columns="$columns,mouse_escape,ferret_escape,cell_entry,stability,sa26_increase"
            config_json="--config_json $fluAScriptDir/h5n1_ha.config.json"
        elif [[ $segment == 1 ]]; then
            columns="$columns,mutdiffsel,mutdiffsel_mutations,aa_substitution_count"
            config_json="--config_json $fluAScriptDir/pb2.config.json"
        fi
        if \
            usher_to_taxonium --input fluA.$asmAcc.$segRef.$today.pb \
                --metadata fluA.$asmAcc.$segRef.$today.metadata.tsv.gz \
                --columns $columns \
                --genbank $fluADir/$asmAcc/$segRef.gbff \
                --name_internal_nodes \
                $config_json \
                --title "$title" \
                --output fluA.$asmAcc.$segRef.$today.taxonium.jsonl.gz \
                >& utt.log; then
            true;
        else
            mv utt.log utt.$asmAcc.$segRef.fail.log
            echo "*** usher_to_taxonium failed, see utt.$asmAcc.$segRef.fail.log ***"
        fi

        pigz -f -p 8 samples.$asmAcc.$segRef.$today

        if [[ $asmAcc == GCF_000864105.1 ]]; then
            # Rename so that we omit collab-shared sequences from files named public,
            # and make truly public versions by pruning down the tree
            mv accs.$asmAcc.$segRef{,.plusGisaid}.tsv
            mv samples.$asmAcc.$segRef{,.plusGisaid}.$today.gz
            mv fluA.$asmAcc.$segRef{,.plusGisaid}.$today.pb
            mv fluA.$asmAcc.$segRef{,.plusGisaid}.$today.metadata.tsv.gz
            mv fluA.$asmAcc.$segRef{,.plusGisaid}.$today.taxonium.jsonl.gz
            mv hgPhyloPlace.description.$asmAcc.$segRef{,.plusGisaid}.txt
            grep -v ^EPI accs.$asmAcc.$segRef.plusGisaid.tsv > accs.$asmAcc.$segRef.tsv
            zcat samples.$asmAcc.$segRef.plusGisaid.$today.gz \
            | grep -Fwf accs.$asmAcc.$segRef.tsv \
                > samples.$asmAcc.$segRef.$today
            $matUtils extract -i fluA.$asmAcc.$segRef.plusGisaid.$today.pb \
                -s samples.$asmAcc.$segRef.$today \
                -o fluA.$asmAcc.$segRef.$today.pb \
                >& tmp.log
            pigz -f -p 8 samples.$asmAcc.$segRef.$today
            set +o pipefail
            zcat fluA.$asmAcc.$segRef.plusGisaid.$today.metadata.tsv.gz \
            | head -1 \
                > tmp
            set -o pipefail
            zcat fluA.$asmAcc.$segRef.plusGisaid.$today.metadata.tsv.gz \
            | grep -Fwf accs.$asmAcc.$segRef.tsv \
                >> tmp
            mv tmp fluA.$asmAcc.$segRef.$today.metadata.tsv
            pigz -f -p 8 fluA.$asmAcc.$segRef.$today.metadata.tsv
            sampleCountComma=$(zcat samples.$asmAcc.$segRef.$today.gz | wc -l \
                   | sed -re 's/([0-9]+)([0-9]{3})$/\1,\2/; s/([0-9]+)([0-9]{3},[0-9]{3})$/\1,\2/;')
            title="Influenza A $strain segment $segment ($segName) $today tree with $sampleCountComma genomes from INSDC"
            columns="genbank_accession,country,location,date,host,serotype,segment,genoflu_genotype,genoflu_segtype,authors,bioproject_accession$nextcladeTaxCo"
            config_json=""
            if [[ $segRef == "NC_007362.1" ]]; then
                columns="$columns,mouse_escape,ferret_escape,cell_entry,stability,sa26_increase"
                config_json="--config_json $fluAScriptDir/h5n1_ha.config.json"
            elif [[ $segRef == "NC_007357.1" ]]; then
                columns="$columns,mutdiffsel,mutdiffsel_mutations,aa_substitution_count"
                config_json="--config_json $fluAScriptDir/pb2.config.json"
            fi
            if \
                usher_to_taxonium --input fluA.$asmAcc.$segRef.$today.pb \
                    --metadata fluA.$asmAcc.$segRef.$today.metadata.tsv.gz \
                    --columns $columns \
                    --genbank $fluADir/$asmAcc/$segRef.gbff \
                    --name_internal_nodes \
                    $config_json \
                    --title "$title" \
                    --output fluA.$asmAcc.$segRef.$today.taxonium.jsonl.gz \
                    >& utt.log; then
                true;
            else
                mv utt.log utt.$asmAcc.$segRef.fail.log
                echo "*** usher_to_taxonium failed, see utt.$asmAcc.$segRef.fail.log ***"
            fi

            echo "$sampleCountComma genomes from INSDC (GenBank/ENA/DDBJ) ($today)" \
                > hgPhyloPlace.description.$asmAcc.$segRef.txt

            # Put .plusGisaid versions in non-hgdownload location
            dir=/gbdb/wuhCor1/hgPhyloPlaceData/influenzaA/$segRef
            mkdir -p $dir
            ln -sf $(pwd)/fluA.$asmAcc.$segRef.plusGisaid.$today.pb \
                     $dir/fluA.$asmAcc.$segRef.plusGisaid.latest.pb
            ln -sf $(pwd)/fluA.$asmAcc.$segRef.plusGisaid.$today.metadata.tsv.gz \
                     $dir/fluA.$asmAcc.$segRef.plusGisaid.latest.metadata.tsv.gz
            ln -sf $(pwd)/hgPhyloPlace.description.$asmAcc.$segRef.plusGisaid.txt \
                     $dir/fluA.$asmAcc.$segRef.plusGisaid.latest.version.txt
            ln -sf $(pwd)/samples.$asmAcc.$segRef.plusGisaid.$today.gz \
                     $dir/fluA.$asmAcc.$segRef.plusGisaid.latest.samples.gz
        fi

        # Link regular versions to non-hgdownload location too
        dir=/gbdb/wuhCor1/hgPhyloPlaceData/influenzaA/$segRef
        mkdir -p $dir
        ln -sf $(pwd)/fluA.$asmAcc.$segRef.$today.pb \
                 $dir/fluA.$asmAcc.$segRef.latest.pb
        ln -sf $(pwd)/fluA.$asmAcc.$segRef.$today.metadata.tsv.gz \
                 $dir/fluA.$asmAcc.$segRef.latest.metadata.tsv.gz
        ln -sf $(pwd)/hgPhyloPlace.description.$asmAcc.$segRef.txt \
                 $dir/fluA.$asmAcc.$segRef.latest.version.txt
        ln -sf $(pwd)/samples.$asmAcc.$segRef.$today.gz \
                 $dir/fluA.$asmAcc.$segRef.latest.samples.gz

        # Extract Newick and VCF for anyone who wants to download those instead of protobuf
        $matUtils extract -i fluA.$asmAcc.$segRef.$today.pb \
            -t fluA.$asmAcc.$segRef.$today.nwk \
            -v fluA.$asmAcc.$segRef.$today.vcf >& tmp.log
        pigz -p 8 -f fluA.$asmAcc.$segRef.$today.nwk fluA.$asmAcc.$segRef.$today.vcf

        # Link to public trees archive directory (no assembly/segRef hierarchy, just by date)
        read y m d < <(echo $today | sed -re 's/-/ /g')
        archive=$archiveRoot/$y/$m/$d
        mkdir -p $archive
        ln -f $(pwd)/fluA.$asmAcc.$segRef.$today.{nwk,vcf,metadata.tsv}.gz $archive/
        if [ -s fluA.$asmAcc.$segRef.$today.taxonium.jsonl.gz ]; then
            ln -f $(pwd)/fluA.$asmAcc.$segRef.$today.taxonium.jsonl.gz $archive/
        fi
        gzip -c fluA.$asmAcc.$segRef.$today.pb > $archive/fluA.$asmAcc.$segRef.$today.pb.gz
        ln -f $(pwd)/hgPhyloPlace.description.$asmAcc.$segRef.txt \
            $archive/fluA.$asmAcc.$segRef.$today.version.txt

        # Update 'latest' in $archiveRoot
        for f in $archive/fluA.$asmAcc.$segRef.$today.*; do
            latestF=$(echo $(basename $f) | sed -re 's/'$today'/latest/')
            ln -f $f $archiveRoot/$latestF
        done

        # Update hgdownload-test link for archive (adding assembly/segRef hierarchy)
        mkdir -p $downloadsRoot/$asmDir/UShER_$segRef/$y/$m/$d
        ln -sf $archive/*.$segRef.* $downloadsRoot/$asmDir/UShER_$segRef/$y/$m/$d/
        ln -sf $archiveRoot/*.$segRef.latest.* $downloadsRoot/$asmDir/UShER_$segRef/
        # rsync to hgdownload hubs dir
        for h in hgdownload1 hgdownload3; do
            if rsync -a -L --delete $downloadsRoot/$asmDir/UShER_$segRef \
                     qateam@$h:/mirrordata/hubs/$asmDir/; then
                true
            else
            echo ""
                echo "*** rsync to $h failed -- disk full ? ***"
            echo ""
            fi
        done
    done
done

$fluAScriptDir/runGenoFlu.sh >& genoflu.log

echo "Built the trees, cleaning up."
rm -f mutation-paths.txt *.pre*.pb final-tree.nh tmp.log
nice gzip -f *.log *.tsv move_log* *.stderr

echo "Building H5N1 outbreak trees"
$fluAScriptDir/buildConcatTree.sh $today
$fluAScriptDir/buildConcatTreeD1.1.sh $today
echo "All done!"

#!/bin/bash
source ~/.bashrc
set -beEu -o pipefail

#*** until working:
set -x

# Align INSDC sequences to reference and build N*M trees where N = 8 (number of segments in the
# influenza genome) and M = 7 (number of RefSeq genome assemblies)

fluAScriptDir=$(dirname "${BASH_SOURCE[0]}")

#***today=$(date +%F)
today=2023-07-10

fluADir=/hive/data/outside/otto/fluA
fluANcbiDir=$fluADir/ncbi/ncbi.latest

usherDir=~angie/github/usher
usherSampled=$usherDir/build/usher-sampled
usher=$usherDir/build/usher
matUtils=$usherDir/build/matUtils
matOptimize=$usherDir/build/matOptimize

minSize=800
threads=64

assemblyDir=/hive/data/outside/ncbi/genomes
asmHubDir=/hive/data/genomes/asmHubs/refseqBuild

archiveRoot=/hive/users/angie/publicTreesFluA

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


# --> For H3N2 only we only need GCF_000865085.1 segment 4 NC_007366.1 and segment 6 NC_007368.1


if [[ ! -d $fluADir/ncbi/ncbi.$today || ! -s $fluADir/ncbi/ncbi.$today/genbank.fa.xz ]]; then
    mkdir -p $fluADir/ncbi/ncbi.$today
    $fluAScriptDir/getNcbiFluA.sh >& $fluADir/ncbi/ncbi.$today/getNcbiFluA.log
fi

buildDir=$fluADir/build/$today
mkdir -p $buildDir
cd $buildDir

# Use metadata to make a renaming file
tawk '$7 >= '$minSize $fluANcbiDir/metadata.tsv \
    > tweakedMetadata.tsv
cut -f 1,12 tweakedMetadata.tsv \
| trimWordy.pl \
    > accToIdFromTitle.tsv

#*** If this is really the same as RSV then it should become a real script.
#*** --> not quite -- often isolate is a step down from the name in the title, so favor the title
#*** name if it contains a slash.
#*** Also copied the paren-stripping seds from dengue.
#*** Also added the stripping of _ after tab.
join -t$'\t' <(sort tweakedMetadata.tsv) accToIdFromTitle.tsv \
| perl -wne 'chomp; @w=split(/\t/);
    my ($acc, $iso, $loc, $date, $str, $tid) = ($w[0], $w[1], $w[3], $w[4], $w[14], $w[15]);
    if (! defined $str) { $str = ""; }
    my $name = $str ? $str : ($tid && $tid =~ m@/@) ? $tid : $iso ? $iso : $tid ? $tid : "";
    my $country = $loc;  $country =~ s/:.*//;
    my $COU = $country;  $COU =~ s/^(\w{3}).*/$1/;  $COU = uc($COU);
    if ($country eq "United Kingdom") { $COU = "UK"; }
    if ($name !~ /$country/ && $name !~ /\b$COU\b/ && $name ne "") { $name = "$country/$name"; }
    $name =~ s/[,;]//g;
    my $year = $date;  $year =~ s/-.*//;
    my $year2 = $year;   $year2 =~ s/^\d{2}(\d{2})$/$1/;
    if ($name ne "" && $name !~ /$year/ && $name !~ /\/$year2$/) { $name = "$name/$year"; }
    if ($date eq "") { $date = "?"; }
    my $fullName = $name ? "$name|$acc|$date" : "$acc|$date";
    $fullName =~ s/ /_/g;
    print "$acc\t$fullName\n";' \
| sed -re 's/[():'"'"']/_/g; s/\[/_/g; s/\]/_/g;' \
| sed -re 's/_+\|/|/;' \
| sed -re 's/\t_/\t/;' \
| sed -re "s/'//g;" \
    > renaming.tsv

# This builds the whole tree from scratch!  Eventually we'll want to add only the new sequences
# to yesterday's tree.

echo '()' > emptyTree.nwk

for asmAcc in GCF_000865085.1 GCF_001343785.1 GCF_000865725.1 GCF_000928555.1 GCF_000864105.1 \
              GCF_000866645.1 GCF_000851145.1; do
#*** for asmAcc in GCF_000865085.1 ; do
    asmDir=$(echo $asmAcc \
        | sed -re 's@^(GC[AF])_([0-9]{3})([0-9]{3})([0-9]{3})\.([0-9]+)@\1/\2/\3/\4/\1_\2\3\4.\5@')
    assemblyReport=$assemblyDir/$asmDir*/$asmAcc*_assembly_report.txt
    strain=$(grep '^# Organism name' $assemblyReport \
             | sed -re 's/.* Influenza A virus \(//; s/\)\).*/)/;')
    segRefs=$(tawk '$8 == "Primary Assembly" && $7 != "na" {print $7;}' $assemblyReport)

    for segRef in $segRefs; do
#***    for segRef in NC_007366.1 NC_007368.1 ; do
        segment=$(grep -F $segRef $assemblyReport | cut -f 1)
        echo "Starting $asmAcc $segRef $strain segment $segment"

        # Run nextalign with RefSeq.
        if [[ -s aligned.$asmAcc.$segRef.fa.xz ]]; then
            echo "aligned.$asmAcc.$segRef.fa.xz already exists -- skipping nextalign."
        else
            nextalign run --input-ref ../../$asmAcc/$segRef.fa \
                --include-reference \
                --jobs $threads \
                --output-fasta aligned.$asmAcc.$segRef.fa.xz \
                $fluADir/ncbi/ncbi.$today/genbank.fa.xz \
                >& nextalign.log
        fi

        # Add -excludeFile=$fluAScriptDir/exclude.ids if we need to exclude any in the future.
        time faToVcf -verbose=2 -includeRef -includeNoAltN \
            <(xzcat aligned.$asmAcc.$segRef.fa.xz) stdout \
        | vcfRenameAndPrune stdin renaming.tsv stdout \
        | pigz -p 8 \
            > all.$asmAcc.$segRef.vcf.gz

        time $usherSampled -T $threads -A -e 5 \
            -t emptyTree.nwk \
            -v all.$asmAcc.$segRef.vcf.gz \
            -o fluA.$asmAcc.$segRef.$today.preFilter.pb \
            --optimization_radius 0 --batch_size_per_process 10 \
            > usher.addNew.$asmAcc.$segRef.log 2>usher-sampled.$asmAcc.$segRef.stderr

        # Filter out branches that are so long they must lead to some other type
        $matUtils extract -i fluA.$asmAcc.$segRef.$today.preFilter.pb \
            --max-branch-length 1000 \
            -O -o fluA.$asmAcc.$segRef.$today.preOpt.pb >& tmp.log

        # Optimize:
        time $matOptimize -T $threads -m 0.00000001 -M 1 -S move_log.$asmAcc.$segRef \
            -i fluA.$asmAcc.$segRef.$today.preOpt.pb \
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
        echo "$sampleCountComma genomes from INSDC (GenBank/ENA/DDBJ) ($today)" \
            > hgPhyloPlace.description.$asmAcc.$segRef.txt

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
        *)
            nextcladeName=""
            nextcladeTaxCo=""
            ;;
        esac
        if [[ x$nextcladeName != x ]]; then
            nextclade dataset get --name $nextcladeName --output-zip $nextcladeName.zip
            time nextclade run \
                -D $nextcladeName.zip \
                -j $threads \
                --retry-reverse-complement true \
                --output-tsv nextclade.$asmAcc.$segRef.tsv \
                <(faSomeRecords <(xzcat $fluADir/ncbi/ncbi.$today/genbank.fa.xz) \
                      accs.$asmAcc.$segRef.tsv stdout) \
                >& nextclade.$asmAcc.$segRef.log
            nextcladeTaxCo=",Nextstrain_clade"
        fi

        # Make metadata that uses same names as tree
        echo -e "strain\tgenbank_accession\tdate\tcountry\tlocation\tlength\thost\tbioproject_accession\tbiosample_accession\tsra_accession\tauthors\tpublications" \
            > fluA.$asmAcc.$segRef.$today.metadata.tsv
        grep -Fwf accs.$asmAcc.$segRef.tsv $fluANcbiDir/metadata.tsv \
        | sort \
        | perl -F'/\t/' -walne '$F[3] =~ s/(: ?|$)/\t/;  print join("\t", @F);' \
        | join -t$'\t' -o 1.2,2.1,2.6,2.4,2.5,2.8,2.9,2.10,2.11,2.12,2.14,2.15 \
            <(sort renaming.tsv) \
            - \
        | sort \
            >> fluA.$asmAcc.$segRef.$today.metadata.tsv

        if [[ x$nextcladeName != x ]]; then
            tail -n+2 fluA.$asmAcc.$segRef.$today.metadata.tsv \
            | join -t$'\t' -a1 -o 1.1,1.2,1.3,1.4,1.5,1.6,1.7,1.8,1.9,1.10,1.11,1.12,2.2 - \
                <(join -o 1.2,2.2 -t$'\t' renaming.tsv \
                    <(cut -f 1,2 nextclade.$asmAcc.$segRef.tsv | sort) \
                  | sort) \
            > tmp
            echo -e "strain\tgenbank_accession\tdate\tcountry\tlocation\tlength\thost\tbioproject_accession\tbiosample_accession\tsra_accession\tauthors\tpublications\tNextstrain_clade" \
                > fluA.$asmAcc.$segRef.$today.metadata.tsv
            cat tmp >> fluA.$asmAcc.$segRef.$today.metadata.tsv
            rm tmp
            
        fi
        wc -l fluA.$asmAcc.$segRef.$today.metadata.tsv
        pigz -f -p 8 fluA.$asmAcc.$segRef.$today.metadata.tsv

        # Make a taxonium view
        if \
        usher_to_taxonium --input fluA.$asmAcc.$segRef.$today.pb \
            --metadata fluA.$asmAcc.$segRef.$today.metadata.tsv.gz \
            --columns genbank_accession,country,location,date,host,authors$nextcladeTaxCo \
            --genbank $fluADir/$asmAcc/$segRef.gbff \
            --name_internal_nodes \
            --title "Influenza A $strain segment $segment $today tree with $sampleCountComma genomes from INSDC" \
            --output fluA.$asmAcc.$segRef.$today.taxonium.jsonl.gz \
            >& utt.log; then
            true;
        else
            mv utt.log utt.$asmAcc.$segRef.fail.log
            echo "*** usher_to_taxonium failed, see utt.$asmAcc.$segRef.fail.log ***"
        fi

        # Update links to latest protobuf and metadata in hgwdev cgi-bin directories
#***        for dir in /usr/local/apache/cgi-bin{-angie,,-beta}/hgPhyloPlaceData/$asmAcc; do
        for dir in /usr/local/apache/cgi-bin-angie/hgPhyloPlaceData/$asmAcc; do
            mkdir -p /usr/local/apache/cgi-bin-angie/hgPhyloPlaceData/$asmAcc
            ln -sf $(pwd)/fluA.$asmAcc.$segRef.$today.pb $dir/fluA.$asmAcc.$segRef.latest.pb
            ln -sf $(pwd)/fluA.$asmAcc.$segRef.$today.metadata.tsv.gz $dir/fluA.$asmAcc.$segRef.latest.metadata.tsv.gz
            ln -sf $(pwd)/hgPhyloPlace.description.$asmAcc.$segRef.txt $dir/fluA.$asmAcc.$segRef.latest.version.txt
        done

    # Extract Newick and VCF for anyone who wants to download those instead of protobuf
    $matUtils extract -i fluA.$asmAcc.$segRef.$today.pb \
        -t fluA.$asmAcc.$segRef.$today.nwk \
        -v fluA.$asmAcc.$segRef.$today.vcf >& tmp.log
    pigz -p 8 -f fluA.$asmAcc.$segRef.$today.nwk fluA.$asmAcc.$segRef.$today.vcf

    # Link to public trees download directory hierarchy
    read y m d < <(echo $today | sed -re 's/-/ /g')
    archive=$archiveRoot/$y/$m/$d
    mkdir -p $archive
    ln -f $(pwd)/fluA.$asmAcc.$segRef.$today.{nwk,vcf,metadata.tsv}.gz $archive/
    if [ -s fluA.$asmAcc.$segRef.$today.taxonium.jsonl.gz ]; then
        ln -f $(pwd)/fluA.$asmAcc.$segRef.$today.taxonium.jsonl.gz $archive/
    fi
    gzip -c fluA.$asmAcc.$segRef.$today.pb > $archive/fluA.$asmAcc.$segRef.$today.pb.gz
    ln -f $(pwd)/hgPhyloPlace.description.$asmAcc.$segRef.txt $archive/fluA.$asmAcc.$segRef.$today.version.txt

    # Update 'latest' in $archiveRoot
    for f in $archive/fluA.$asmAcc.$segRef.$today.*; do
        latestF=$(echo $(basename $f) | sed -re 's/'$today'/latest/')
        ln -f $f $archiveRoot/$latestF
    done

    # Update hgdownload-test link for archive
    mkdir -p /usr/local/apache/htdocs-hgdownload/hubs/$asmDir/UShER_$segRef/$y/$m
    ln -sf $archive /usr/local/apache/htdocs-hgdownload/hubs/$asmDir/UShER_$segRef/$y/$m
    ln -sf $archiveRoot/*.latest.* /usr/local/apache/htdocs-hgdownload/hubs/$asmDir/UShER_$segRef/
    # rsync to hgdownload hubs dir
#***    rsync -v -a -L --delete /usr/local/apache/htdocs-hgdownload/hubs/$asmDir/UShER_$segRef/* \
#***        qateam@hgdownload.soe.ucsc.edu:/mirrordata/hubs/$asmDir/UShER_$segRef/

   done
done


rm -f mutation-paths.txt *.pre*.pb final-tree.nh
nice gzip -f *.log *.tsv move_log* *.stderr samples.*

#!/bin/bash
source ~/.bashrc
set -beEu -o pipefail

# Align INSDC sequences to reference and build two trees, one for RSV-A and one for RSV-B.

rsvScriptDir=$(dirname "${BASH_SOURCE[0]}")

today=$(date +%F)

rsvDir=/hive/data/outside/otto/rsv
rsvNcbiDir=$rsvDir/ncbi/ncbi.latest

usherDir=~angie/github/usher
usherSampled=$usherDir/build/usher-sampled
usher=$usherDir/build/usher
matUtils=$usherDir/build/matUtils
matOptimize=$usherDir/build/matOptimize

# RSV-A reference: Nextstrain uses KJ627695.1 but RefSeq is NC_038235.1 (M74568)
asmAccA=GCF_002815475.1
gbffA=$rsvDir/NC_038235.1.gbff
nextcladeNameA=rsv_a
refFaA=$rsvDir/NC_038235.1.fa
archiveRootA=/hive/users/angie/publicTreesRsvA

# RSV-B reference: Nextstrain uses ? but RefSeq is NC_001781.1 (AF013254)
asmAccB=GCF_000855545.1
gbffB=$rsvDir/NC_001781.1.gbff
nextcladeNameB=rsv_b
refFaB=$rsvDir/NC_001781.1.fa
archiveRootB=/hive/users/angie/publicTreesRsvB

if [[ ! -d $rsvDir/ncbi/ncbi.$today || ! -s $rsvDir/ncbi/ncbi.$today/genbank.fa.xz ]]; then
    mkdir -p $rsvDir/ncbi/ncbi.$today
    $rsvScriptDir/getNcbiRsv.sh >& $rsvDir/ncbi/ncbi.$today/getNcbiRsv.log
fi

buildDir=$rsvDir/build/$today
mkdir -p $buildDir
cd $buildDir

# Make sure the download wasn't truncated without reporting an error:
count=$(wc -l < $rsvNcbiDir/metadata.tsv)
minSamples=4500
if (( $count < $minSamples )); then
    echo "*** Too few samples ($count)!  Expected at least $minSamples.  Halting. ***"
    exit 1
fi

# Use metadata to make a renaming file
cat $rsvNcbiDir/metadata.tsv \
| sed -re 's@([\t /])h?rsv(-?[ab])?/@\1@ig;' \
| sed -re 's@([\t /])[ab]/@\1@ig;' \
| sed -re 's@([\t /])human/@\1@ig;' \
| sed -re 's@([\t /])homo sapiens/@\1@ig;' \
| sed -re 's@Human respiratory syncytial virus nonstructural protein 1, nonstructural protein 2, nucleocapsid protein, phosphoprotein, matrix protein, small hydrophobic protein, glycoprotein, fusion glycoprotein, 22K/M2 protein and L protein mRNA, complete cds@@;' \
    > tweakedMetadata.tsv
cut -f 1,12 tweakedMetadata.tsv \
| trimWordy.pl \
    > accToIdFromTitle.tsv
join -t$'\t' <(sort tweakedMetadata.tsv) accToIdFromTitle.tsv \
| perl -wne 'chomp; @w=split(/\t/);
    my ($acc, $iso, $loc, $date, $str, $tid) = ($w[0], $w[1], $w[3], $w[4], $w[14], $w[15]);
    if (! defined $str) { $str = ""; }
    my $name = $str ? $str : $iso ? $iso : $tid ? $tid : "";
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
    > renaming.tsv

# This builds the whole tree from scratch!  Eventually we'll want to add only the new sequences
# to yesterday's tree.

echo '()' > emptyTree.nwk

for aOrB in A B; do
    if [[ $aOrB == "A" ]]; then
        refFa=$refFaA
        gbff=$gbffA
        asmAcc=$asmAccA
        archiveRoot=$archiveRootA
        nextcladeName=$nextcladeNameA
    else
        refFa=$refFaB
        gbff=$gbffB
        asmAcc=$asmAccB
        archiveRoot=$archiveRootB
        nextcladeName=$nextcladeNameB
    fi

    # Run nextclade to get clade assignments -- but not alignments because we need alignments
    # to RefSeqs not nextclade's chosen references.
    nextclade dataset get --name $nextcladeName --output-zip $nextcladeName.zip
    time nextclade run \
        -D $nextcladeName.zip \
        -j 30 \
        --retry-reverse-complement true \
        --output-tsv nextclade.rsv$aOrB.tsv \
        $rsvDir/ncbi/ncbi.$today/genbank.fa.xz \
        >& nextclade.$aOrB.log

    # Run nextalign with RefSeq.
    nextalign run --input-ref $refFa \
        --include-reference \
        --jobs 32 \
        --output-fasta aligned.$aOrB.fa.xz \
        $rsvDir/ncbi/ncbi.$today/genbank.fa.xz \
        >& nextalign.log

    time faToVcf -verbose=2 -includeRef -includeNoAltN -excludeFile=$rsvScriptDir/exclude.ids \
        <(xzcat aligned.$aOrB.fa.xz) stdout \
        | vcfRenameAndPrune stdin renaming.tsv stdout \
        | pigz -p 8 \
        > all.$aOrB.vcf.gz

    time $usherSampled -T 64 -A -e 5 \
        -t emptyTree.nwk \
        -v all.$aOrB.vcf.gz \
        -o rsv$aOrB.$today.preFilter.pb\
        --optimization_radius 0 --batch_size_per_process 10 \
        > usher.addNew.$aOrB.log 2>usher-sampled.$aOrB.stderr

    # Filter out branches that are so long they must lead to the other subspecies (B or A)
    $matUtils extract -i rsv$aOrB.$today.preFilter.pb \
        --max-branch-length 1000 \
        -O -o rsv$aOrB.$today.preOpt.pb >& tmp.log

    # Optimize:
    time $matOptimize -T 64 -r 20 -M 2 -S move_log.$aOrB \
        -i rsv$aOrB.$today.preOpt.pb \
        -o rsv$aOrB.$today.opt.pb \
        >& matOptimize.$aOrB.log

    # Annotate Ramaekers et al., Goya et al. and consortium clades using nextclade assignments and
    # other sources (nextstrain.org, supplemental table...).
    function tl {
        perl -wne 'chomp; @w = split(/\t/, $_, -1); $i = 1; foreach $w (@w) { print "$i\t$w[$i-1]\n"; $i++; }'
    }
    rCol=$(head -1 nextclade.rsv$aOrB.tsv | tl | grep -w clade | cut -f 1)
    gCol=$(head -1 nextclade.rsv$aOrB.tsv | tl | grep -w G_clade | cut -f 1)
    subsCol=$(head -1 nextclade.rsv$aOrB.tsv | tl | grep -w totalSubstitutions | cut -f 1)
    tail -n+2 nextclade.rsv$aOrB.tsv \
    | tawk '$'$subsCol' < 1000 && $'$gCol' != "" {print $1, $'$gCol';}' \
    | sed -re 's/unassigned/Unassigned/;' \
    | sort > accToGClade
    join -t$'\t' accToGClade renaming.tsv | grep -v Unassigned | cut -f 2,3 | sort \
        > gCladeToName.$aOrB
    if [[ $aOrB == A ]]; then
        # RSV-A only: if nextclade is not calling some GAs, use assignments from nextstrain.org.
        for ga in GA1 GA2.1 GA2.2 GA2.3.6a; do
            set +o pipefail
            gaCount=$(grep -w $ga gCladeToName.A | wc -l)
            set -o pipefail
            if (( $gaCount < 10 )); then
                join -t$'\t' \
                    <(grep -w $ga $rsvDir/nextstrain_acc_to_clade.tsv | sort) \
                    <(sed -re 's/\.[0-9]+\t/\t/;' renaming.tsv) \
                | cut -f 2,3 \
                    >> gCladeToName.$aOrB
            fi
        done
    else
        # RSV-B only: if nextclade is not calling GB1, use assignments from nextstrain.org.
        set +o pipefail
        gb1Count=$(grep -w GB1 gCladeToName.B | wc -l)
        set -o pipefail
        if (( $gb1Count < 10 )); then
            join -t$'\t' \
                <(grep -w GB1 $rsvDir/nextstrain_acc_to_clade.tsv | sort) \
                <(sed -re 's/\.[0-9]+\t/\t/;' renaming.tsv) \
            | cut -f 2,3 \
                >> gCladeToName.$aOrB
        fi
    fi
    if [[ -s $rsvScriptDir/goya.$aOrB.clade-mutations.tsv ]]; then
        cladeMuts="-M $rsvScriptDir/goya.$aOrB.clade-mutations.tsv"
    else
        cladeMuts=""
    fi
    $matUtils annotate -T 64 -f 0.9 -m 0.1 -i rsv$aOrB.$today.opt.pb \
        -l -c gCladeToName.$aOrB $cladeMuts -o rsv$aOrB.$today.gClade.pb \
        >& annotate.$aOrB.gClade.log
    tail -n+2 nextclade.rsv$aOrB.tsv \
    | tawk '$'$subsCol' < 1000 && $'$rCol' != "" {print $1, $'$rCol';}' \
    | sed -re 's/unassigned/Unassigned/;' \
    | sort > accToRClade
    # As of 2023-01-24, nextclade doesn't call all of the Ramaekers clades, but Table S1 from the
    # publication (https://academic.oup.com/ve/article/6/2/veaa052/5876035) lists INSDC accessions
    # and assignments.  Use those when available, and fall back on nextclade for other sequences.
    # Exclude nextclade A17 because it messes up A18.
    join -a 1 -o 1.1,1.2,2.2 -t$'\t' \
        <(sed -re 's/\.[0-9]+\t/\t/;' accToRClade ) \
        $rsvDir/Ramaekers_TableS1_accTo$aOrB.tsv \
    | tawk '{ if ($3 != "" || $2 == "A17") { print $1, $3; } else { print $1, $2; } }' \
    | sort \
        > accToRCladeCombined
    join -t$'\t' accToRCladeCombined <(sed -re 's/\.[0-9]+\t/\t/;' renaming.tsv) \
    | grep -v Unassigned | cut -f 2,3 | sort \
        > rCladeToName.$aOrB
    if [[ -s $rsvScriptDir/ramaekers.$aOrB.clade-mutations.tsv ]]; then
        cladeMuts="-M $rsvScriptDir/ramaekers.$aOrB.clade-mutations.tsv"
    else
        cladeMuts=""
    fi
    $matUtils annotate -T 64 -f 0.9 -m 0.1 -i rsv$aOrB.$today.gClade.pb \
        -c rCladeToName.$aOrB $cladeMuts -o rsv$aOrB.$today.pb \
        >& annotate.$aOrB.rClade.log
    $matUtils summary -i rsv$aOrB.$today.pb -C sample-clades.$aOrB >& tmp.log

    # Make metadata that uses same names as tree
    echo -e "strain\tgenbank_accession\tdate\tcountry\tlocation\tlength\thost\tbioproject_accession\tbiosample_accession\tsra_accession\tauthors\tpublications\tgoya_nextclade\tramaekers_nextclade\tgoya_usher\tramaekers_usher\tramaekers_tableS1\tgcc_lineage" \
        > rsv$aOrB.$today.metadata.tsv
    join -t$'\t' -o 1.2,2.1,2.6,2.4,2.5,2.8,2.9,2.10,2.11,2.13,2.14,2.15 \
        <(sort renaming.tsv) \
        <(sort $rsvNcbiDir/metadata.tsv \
          | perl -F'/\t/' -walne '$F[3] =~ s/(: ?|$)/\t/;  print join("\t", @F);') \
    | sort \
    | join -t$'\t' - <(join -a 1 -o 1.2,2.2 -t$'\t' renaming.tsv accToGClade | sort) \
    | join -t$'\t' - <(join -a 1 -o 1.2,2.2 -t$'\t' renaming.tsv accToRClade | sort) \
    | join -t$'\t' - <(sort sample-clades.$aOrB | sed -re 's/None/Unassigned/g') \
    | join -t$'\t' - <(join -a 1 -o 1.2,2.2 -t$'\t' \
                           <(sed -re 's/\.[0-9]+\t/\t/;' renaming.tsv) \
                           $rsvDir/Ramaekers_TableS1_accTo$aOrB.tsv \
                       | sort) \
    | join -t$'\t' - <(join -a 1 -o 1.2,2.2 -t$'\t' \
                           <(sed -re 's/\.[0-9]+\t/\t/;' renaming.tsv) \
                           <(tawk '{print $2, $1;}' $rsvDir/RSV${aOrB}_GCC_2023-05-16.tsv | sort) \
                       | sort) \
        >> rsv$aOrB.$today.metadata.tsv
    pigz -f -p 8 rsv$aOrB.$today.metadata.tsv

    # Make a tree version description for hgPhyloPlace
    $matUtils extract -i rsv$aOrB.$today.pb -u samples.$aOrB.$today >& tmp.log
    sampleCountComma=$(wc -l < samples.$aOrB.$today \
              | sed -re 's/([0-9]+)([0-9]{3})$/\1,\2/; s/([0-9]+)([0-9]{3},[0-9]{3})$/\1,\2/;')
    echo "$sampleCountComma genomes from INSDC (GenBank/ENA/DDBJ) ($today)" \
        > hgPhyloPlace.description.$aOrB.txt

    # Make a taxonium view
    usher_to_taxonium --input rsv$aOrB.$today.pb \
        --metadata rsv$aOrB.$today.metadata.tsv.gz \
        --columns genbank_accession,country,location,date,authors,ramaekers_nextclade,goya_nextclade,ramaekers_usher,goya_usher,ramaekers_tableS1,gcc_lineage \
        --clade_types=pango,placeholder \
        --genbank $gbff \
        --name_internal_nodes \
        --title "RSV-"$aOrB" $today tree with $sampleCountComma genomes from INSDC" \
        --output rsv$aOrB.$today.taxonium.jsonl.gz \
        >& usher_to_taxonium.$aOrB.log

    # Update links to latest protobuf and metadata in hgwdev cgi-bin directories
    for dir in /usr/local/apache/cgi-bin{-angie,,-beta}/hgPhyloPlaceData/$asmAcc; do
        ln -sf $(pwd)/rsv$aOrB.$today.pb $dir/rsv$aOrB.latest.pb
        ln -sf $(pwd)/rsv$aOrB.$today.metadata.tsv.gz $dir/rsv$aOrB.latest.metadata.tsv.gz
        ln -sf $(pwd)/hgPhyloPlace.description.$aOrB.txt $dir/rsv$aOrB.latest.version.txt
    done

    # Extract Newick and VCF for anyone who wants to download those instead of protobuf
    $matUtils extract -i rsv$aOrB.$today.pb \
        -t rsv$aOrB.$today.nwk \
        -v rsv$aOrB.$today.vcf >& tmp.log
    pigz -p 8 -f rsv$aOrB.$today.nwk rsv$aOrB.$today.vcf

    # Link to public trees download directory hierarchy
    read y m d < <(echo $today | sed -re 's/-/ /g')
    archive=$archiveRoot/$y/$m/$d
    mkdir -p $archive
    ln -f $(pwd)/rsv$aOrB.$today.{nwk,vcf,metadata.tsv,taxonium.jsonl}.gz $archive/
    gzip -c rsv$aOrB.$today.pb > $archive/rsv$aOrB.$today.pb.gz
    ln -f $(pwd)/hgPhyloPlace.description.$aOrB.txt $archive/rsv$aOrB.$today.version.txt

    # Update 'latest' in $archiveRoot
    for f in $archive/rsv$aOrB.$today.*; do
        latestF=$(echo $(basename $f) | sed -re 's/'$today'/latest/')
        ln -f $f $archiveRoot/$latestF
    done

    # Update hgdownload-test link for archive
    asmDir=$(echo $asmAcc \
        | sed -re 's@^(GC[AF])_([0-9]{3})([0-9]{3})([0-9]{3})\.([0-9]+)@\1/\2/\3/\4/\1_\2\3\4.\5@')
    mkdir -p /usr/local/apache/htdocs-hgdownload/hubs/$asmDir/UShER_RSV-$aOrB/$y/$m
    ln -sf $archive /usr/local/apache/htdocs-hgdownload/hubs/$asmDir/UShER_RSV-$aOrB/$y/$m
    # rsync to hgdownload hubs dir
    rsync -v -a -L --delete /usr/local/apache/htdocs-hgdownload/hubs/$asmDir/UShER_RSV-$aOrB/* \
        qateam@hgdownload.soe.ucsc.edu:/mirrordata/hubs/$asmDir/UShER_RSV-$aOrB/
done

set +o pipefail
grep 'Could not' annotate.*.log | cat
grep skipping annotate.*.log | cat
set -o pipefail

cat hgPhyloPlace.description.A.txt
zcat rsvA.$today.metadata.tsv.gz | tail -n+2 | cut -f 13,15 | sort | uniq -c
echo ""
cat hgPhyloPlace.description.B.txt
zcat rsvB.$today.metadata.tsv.gz | tail -n+2 | cut -f 13,15 | sort | uniq -c
echo ""
echo "Ramaekers clades:"
zcat rsvA.$today.metadata.tsv.gz | tail -n+2 | cut -f 14,16,17 | sort | uniq -c
echo ""
zcat rsvB.$today.metadata.tsv.gz | tail -n+2 | cut -f 14,16,17 | sort | uniq -c

rm -f mutation-paths.txt *.pre*.pb final-tree.nh *.opt.pb *.gClade.pb *.pbintermediate*.pb
nice gzip -f *.log *.tsv move_log* *.stderr samples.* sample-clades.*

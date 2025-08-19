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

# Filter out branches longer than this so we don't get RSV-A and RSV-B in same final tree
maxSubs=1500
maxBranchLen=500

if [[ ! -d $rsvDir/ncbi/ncbi.$today || ! -s $rsvDir/ncbi/ncbi.$today/genbank.fa.xz ]]; then
    mkdir -p $rsvDir/ncbi/ncbi.$today
    $rsvScriptDir/getNcbiRsv.sh >& $rsvDir/ncbi/ncbi.$today/getNcbiRsv.log
fi

buildDir=$rsvDir/build/$today
mkdir -p $buildDir
cd $buildDir

# Make sure the download wasn't truncated without reporting an error:
count=$(wc -l < $rsvNcbiDir/metadata.tsv)
minSamples=5600
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
    $fullName =~ s/[ ,:()]/_/g;
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

    if [[ -s $rsvScriptDir/mask.$aOrB.vcf ]]; then
        maskCmd="vcfFilter -excludeVcf=$rsvScriptDir/mask.$aOrB.vcf stdin "
    else
        maskCmd="cat"
    fi
    time faToVcf -verbose=2 -includeNoAltN -excludeFile=$rsvScriptDir/exclude.ids \
        <(xzcat aligned.$aOrB.fa.xz) stdout \
        | vcfRenameAndPrune stdin renaming.tsv stdout \
        | $maskCmd \
        | pigz -p 8 \
        > all.$aOrB.vcf.gz

    time $usherSampled -T 16 -A -e 5 \
        -t emptyTree.nwk \
        -v all.$aOrB.vcf.gz \
        -o rsv$aOrB.$today.preFilter.pb\
        --optimization_radius 0 --batch_size_per_process 10 \
        > usher.addNew.$aOrB.log 2>usher-sampled.$aOrB.stderr

    # Filter out branches that are so long they must lead to the other subspecies (B or A)
    $matUtils extract -i rsv$aOrB.$today.preFilter.pb \
        --max-branch-length $maxBranchLen \
        -O -o rsv$aOrB.$today.preOpt.pb >& tmp.log

    # Optimize:
    time $matOptimize -T 16 -r 20 -M 2 -S move_log.$aOrB \
        -i rsv$aOrB.$today.preOpt.pb \
        -o rsv$aOrB.$today.opt.pb \
        >& matOptimize.$aOrB.log

    # Annotate consortium clades using nextclade assignments and other sources
    # (nextstrain.org, supplemental table...).
    function tl {
        perl -wne 'chomp; @w = split(/\t/, $_, -1); $i = 1; foreach $w (@w) { print "$i\t$w[$i-1]\n"; $i++; }'
    }
    cCol=$(head -1 nextclade.rsv$aOrB.tsv | tl | grep -w clade | cut -f 1)
    subsCol=$(head -1 nextclade.rsv$aOrB.tsv | tl | grep -w totalSubstitutions | cut -f 1)
    tail -n+2 nextclade.rsv$aOrB.tsv \
    | tawk '$'$subsCol' < '$maxSubs' && $'$cCol' != "" {print $2, $'$cCol';}' \
    | sed -re 's/unassigned/Unassigned/;' \
    | sort > accToCClade
    join -t$'\t' accToCClade renaming.tsv \
    | grep -v Unassigned | cut -f 2,3 | sort \
        > cCladeToName.$aOrB
    if [[ -s $rsvScriptDir/gcc.$aOrB.clade-mutations.tsv ]]; then
        cladeMuts="-M $rsvScriptDir/gcc.$aOrB.clade-mutations.tsv"
    else
        cladeMuts=""
    fi
    $matUtils annotate -T 16 -f 0.9 -m 0.1 -i rsv$aOrB.$today.opt.pb \
        -c cCladeToName.$aOrB $cladeMuts -o rsv$aOrB.$today.pb \
        >& annotate.$aOrB.cClade.log
    $matUtils summary -i rsv$aOrB.$today.pb -C sample-clades.$aOrB >& tmp.log

    # Make metadata that uses same names as tree
    echo -e "strain\tgenbank_accession\tdate\tcountry\tlocation\tlength\thost\tbioproject_accession\tbiosample_accession\tsra_accession\tauthors\tpublications\tGCC_nextclade\tGCC_usher\tGCC_assigned_2023-11" \
        > rsv$aOrB.$today.metadata.tsv
    join -t$'\t' -o 1.2,2.1,2.6,2.4,2.5,2.8,2.9,2.10,2.11,2.13,2.14,2.15 \
        <(sort renaming.tsv) \
        <(sort $rsvNcbiDir/metadata.tsv \
          | perl -F'/\t/' -walne '$F[3] =~ s/(: ?|$)/\t/;  print join("\t", @F);') \
    | sort \
    | join -t$'\t' - <(join -a 1 -o 1.2,2.2 -t$'\t' renaming.tsv accToCClade | sort) \
    | join -t$'\t' - <(sort sample-clades.$aOrB | sed -re 's/None/Unassigned/g') \
    | join -t$'\t' - <(join -a 1 -o 1.2,2.2 -t$'\t' \
                           <(sed -re 's/\.[0-9]+\t/\t/;' renaming.tsv) \
                           $rsvDir/RSV${aOrB}_GCC_2023-11-06.tsv \
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
        --columns genbank_accession,country,location,date,authors,GCC_nextclade,GCC_usher,GCC_assigned_2023-11 \
        --clade_types=placeholder,pango \
        --genbank $gbff \
        --name_internal_nodes \
        --title "RSV-"$aOrB" $today tree with $sampleCountComma genomes from INSDC" \
        --output rsv$aOrB.$today.taxonium.jsonl.gz \
        >& usher_to_taxonium.$aOrB.log

    # Update links to latest protobuf and metadata in /gbdb directories
    nc=$(basename $gbff .gbff)
    dir=/gbdb/wuhCor1/hgPhyloPlaceData/rsv/$nc
    mkdir -p $dir
    ln -sf $(pwd)/rsv$aOrB.$today.pb $dir/rsv$aOrB.latest.pb
    ln -sf $(pwd)/rsv$aOrB.$today.metadata.tsv.gz $dir/rsv$aOrB.latest.metadata.tsv.gz
    ln -sf $(pwd)/hgPhyloPlace.description.$aOrB.txt $dir/rsv$aOrB.latest.version.txt

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
    mkdir -p /data/apache/htdocs-hgdownload/hubs/$asmDir/UShER_RSV-$aOrB/$y/$m
    ln -sf $archive /data/apache/htdocs-hgdownload/hubs/$asmDir/UShER_RSV-$aOrB/$y/$m
    # rsync to hgdownload{1,2} hubs dir
    for h in hgdownload1 hgdownload3; do
        if rsync -a -L --delete /data/apache/htdocs-hgdownload/hubs/$asmDir/UShER_RSV-$aOrB/* \
                 qateam@$h:/mirrordata/hubs/$asmDir/UShER_RSV-$aOrB/; then
            true
        else
            echo ""
            echo "*** rsync to $h failed; disk full? ***"
            echo ""
        fi
    done
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
echo "RSV GCC clades:"
zcat rsvA.$today.metadata.tsv.gz | tail -n+2 | cut -f 14,16,17 | sort | uniq -c
echo ""
zcat rsvB.$today.metadata.tsv.gz | tail -n+2 | cut -f 14,16,17 | sort | uniq -c

rm -f mutation-paths.txt *.pre*.pb final-tree.nh *.opt.pb *.pbintermediate*.pb
nice gzip -f *.log *.tsv move_log* *.stderr samples.* sample-clades.*

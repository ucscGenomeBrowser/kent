#!/bin/bash
source ~/.bashrc
set -beEu -o pipefail

mpxvScriptDir=$(dirname "${BASH_SOURCE[0]}")

today=$(date +%F)

mpxvDir=/hive/data/outside/otto/mpxv

# RefSeq assembly for our track hubs for clade I and clade II
asmAcc_I=GCF_000857045.1
asmAcc_II=GCF_014621545.1

# GenBank flat file for taxonium
gbff_I=$mpxvDir/NC_003310.1.gbff
gbff_II=$mpxvDir/NC_063383.1.gbff

# Metadata file
metadata_I=$mpxvNcbiDir/metadata.cladeI.tsv
metadata_II=$mpxvNcbiDir/metadata.2017outbreak.tsv

# Mask file
#*** TODO get mask regions for clade I!
mask_I=/dev/null
mask_II=$mpxvScriptDir/mask.vcf.gz

# faToVcf -maxDiff
maxDiff_I=10000
maxDiff_II=100

# Minimum number of samples in tree to be sure the NCBI download wasn't incomplete
minSamples_I=600
minSamples_II=7000

# Download archive storage directory
archiveRoot_I=/hive/users/angie/publicTreesMpxvCladeI
archiveRoot_II=/hive/users/angie/publicTreesHMPXV

# Download archive URL directory
downloadDir_I=UShER_MPXV_cladeI
downloadDir_II=UShER_hMPXV

mpxvNcbiDir=$mpxvDir/ncbi/ncbi.latest

usherDir=~angie/github/usher
usherSampled=$usherDir/build/usher-sampled
usher=$usherDir/build/usher
matUtils=$usherDir/build/matUtils

if [[ ! -d $mpxvDir/ncbi/ncbi.$today || ! -s $mpxvDir/ncbi/ncbi.$today/genbank.fa.xz ]]; then
    mkdir -p $mpxvDir/ncbi/ncbi.$today
    $mpxvScriptDir/getNcbiMpxv.sh >& $mpxvDir/ncbi/ncbi.$today/getNcbiMpxv.log
fi

buildDir=$mpxvDir/build/$today
mkdir -p $buildDir
cd $buildDir

# This builds the whole tree from scratch!  Eventually we'll want to add only the new sequences
# to yesterday's tree.
echo '()' > emptyTree.nwk

for clade in I II; do
    if [[ $clade == "I" ]]; then
        asmAcc=$asmAcc_I
        gbff=$gbff_I
        metadata=$metadata_I
        mask=$mask_I
        maxDiff=$maxDiff_I
        minSamples=$minSamples_I
        archiveRoot=$archiveRoot_I
        downloadDir=$downloadDir_I
    else
        asmAcc=$asmAcc_II
        gbff=$gbff_II
        metadata=$metadata_II
        mask=$mask_II
        maxDiff=$maxDiff_II
        minSamples=$minSamples_II
        archiveRoot=$archiveRoot_II
        downloadDir=$downloadDir_II
    fi

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
        my $fullName = $name ? "$name|$acc|$date" : "$acc|$date";
        $fullName =~ s/[ ,:()]/_/g;
        print "$acc\t$fullName\n";' \
        $metadata > renaming.$clade.tsv

    # Use Nextstrain's exclusion list too.
    awk '{print $1;}' ~angie/github/monkeypox/phylogenetic/defaults/exclude_accessions.txt \
        | grep -Fwf - <(cut -f 1 $metadata) \
        > exclude.ids

    asmDir=$(echo $asmAcc \
        | sed -re 's@^(GC[AF])_([0-9]{3})([0-9]{3})([0-9]{3})\.([0-9]+)@\1/\2/\3/\4/\1_\2\3\4.\5@')
    ref2bit=/hive/data/genomes/asmHubs/$asmDir/$asmAcc.2bit
    time cat <(twoBitToFa $ref2bit stdout) <(xzcat $mpxvNcbiDir/nextalign.$clade.fa.xz) \
    | faToVcf -maxDiff=$maxDiff -verbose=2 -excludeFile=exclude.ids -includeNoAltN stdin stdout \
        | vcfRenameAndPrune stdin renaming.$clade.tsv stdout \
        | vcfFilter -excludeVcf=$mask stdin \
        | pigz -p 8 \
        > all.masked.vcf.gz

    time $usherSampled -T 16 -A -e 10 \
        -t emptyTree.nwk \
        -v all.masked.vcf.gz \
        -o mpxv.clade$clade.$today.masked.preOpt.pb.gz \
        --optimization_radius 0 --batch_size_per_process 10 \
        > usher.addNew.log 2>usher-sampled.stderr

    # Optimize:
    time ~angie/github/usher_branch/build/matOptimize \
        -T 16 -r 20 -M 2 -S move_log.usher_branch \
        -i mpxv.clade$clade.$today.masked.preOpt.pb.gz \
        -o mpxv.clade$clade.$today.masked.opt.pb.gz \
        >& matOptimize.usher_branch.log
    # It crashes when I add
    #    -v all.masked.vcf.gz \
    # -- bug Cheng later.

    if [[ $clade == "II" ]]; then
        # Annotate root nodes for Nextstrain lineages.
        join -t$'\t' <(sort renaming.$clade.tsv) <(zcat $mpxvNcbiDir/nextclade.$clade.tsv.gz | cut -f 2,5 | sort) \
        | tawk '{print $3, $2;}' | sort > lineageToName
        $matUtils annotate -T 16 -i mpxv.clade$clade.$today.masked.opt.pb.gz -c lineageToName \
            -o mpxv.clade$clade.$today.masked.pb.gz \
            >& annotate.$clade.log

        # Make metadata that uses same names as tree and includes nextclade lineage assignments.
        echo -e "strain\tgenbank_accession\tdate\tcountry\tlocation\tlength\thost\tbioproject_accession\tbiosample_accession\tsra_accession\tauthors\tpublications\tNextstrain_lineage" \
            > mpxv.clade$clade.$today.metadata.tsv
        join -t$'\t' <(sort renaming.$clade.tsv) <(zcat $mpxvNcbiDir/nextclade.$clade.tsv.gz | cut -f 2,5 | sort) \
        | join -t$'\t' -o 1.2,2.1,2.6,2.4,2.5,2.8,2.9,2.10,2.11,2.13,2.14,2.15,1.3 \
            - <(sort $metadata \
        | perl -F'/\t/' -walne '$F[3] =~ s/(: ?|$)/\t/;  print join("\t", @F);') \
            >> mpxv.clade$clade.$today.metadata.tsv
    else
        # No lineages to annotate; just clean up .opt with -O.
        $matUtils extract -i mpxv.clade$clade.$today.masked.opt.pb.gz \
            -O -o mpxv.clade$clade.$today.masked.pb.gz

        # Make metadata that uses same names as tree and includes nextclade clade Ia/Ib assignments.
        echo -e "strain\tgenbank_accession\tdate\tcountry\tlocation\tlength\thost\tbioproject_accession\tbiosample_accession\tsra_accession\tauthors\tpublications\tNextstrain_clade" \
            > mpxv.clade$clade.$today.metadata.tsv
        join -t$'\t' <(sort renaming.$clade.tsv) <(zcat $mpxvNcbiDir/nextclade.$clade.tsv.gz | cut -f 2,3 | sort) \
        | join -t$'\t' -o 1.2,2.1,2.6,2.4,2.5,2.8,2.9,2.10,2.11,2.13,2.14,2.15,1.3 \
            - <(sort $metadata \
        | perl -F'/\t/' -walne '$F[3] =~ s/(: ?|$)/\t/;  print join("\t", @F);') \
            >> mpxv.clade$clade.$today.metadata.tsv
    fi
    pigz -f -p 8 mpxv.clade$clade.$today.metadata.tsv

    # Make a tree version description for hgPhyloPlace
    $matUtils extract -i mpxv.clade$clade.$today.masked.pb.gz -u samples.$clade.$today
    sampleCount=$(wc -l < samples.$clade.$today)
    # Sometimes NCBI download is incomplete; don't replace yesterday's tree in that case.
    if (( $sampleCount < $minSamples )); then
        echo "*** Too few samples ($sampleCount) for clade $clade!  Expected at least $minSamples.  Halting. ***"
        exit 1
    fi
    sampleCountComma=$(echo $sampleCount \
                       | sed -re 's/([0-9]+)([0-9]{3})$/\1,\2/; s/([0-9]+)([0-9]{3},[0-9]{3})$/\1,\2/;')
    echo "$sampleCountComma genomes from INSDC (GenBank/ENA/DDBJ) ($today)" \
        > hgPhyloPlace.description.$clade.txt

    # Make a taxonium view
    if [[ $clade == "I" ]]; then
        columns=genbank_accession,location,date,authors,Nextstrain_clade
    else
        columns=genbank_accession,location,date,authors,Nextstrain_lineage
    fi
    usher_to_taxonium --input mpxv.clade$clade.$today.masked.pb.gz \
        --metadata mpxv.clade$clade.$today.metadata.tsv.gz \
        --genbank $gbff \
        --columns $columns \
        --clade_types=pango \
        --output mpxv.clade$clade.$today.masked.taxonium.jsonl.gz \
        >& usher_to_taxonium.log

    # Update links to latest protobuf and metadata in /gbdb directories
    nc=$(basename $gbff .gbff)
    dir=/gbdb/wuhCor1/hgPhyloPlaceData/mpxv/$nc
    mkdir -p $dir
    ln -sf $(pwd)/mpxv.clade$clade.$today.masked.pb.gz $dir/mpxv.clade$clade.latest.pb.gz
    ln -sf $(pwd)/mpxv.clade$clade.$today.metadata.tsv.gz $dir/mpxv.clade$clade.latest.metadata.tsv.gz
    ln -sf $(pwd)/hgPhyloPlace.description.$clade.txt $dir/mpxv.clade$clade.latest.version.txt

    # Extract Newick and VCF for anyone who wants to download those instead of protobuf
    $matUtils extract -i mpxv.clade$clade.$today.masked.pb.gz \
        -t mpxv.clade$clade.$today.nwk \
        -v mpxv.clade$clade.$today.masked.vcf
    pigz -p 8 -f mpxv.clade$clade.$today.nwk mpxv.clade$clade.$today.masked.vcf

    # Link to public trees download directory hierarchy
    read y m d < <(echo $today | sed -re 's/-/ /g')
    archive=$archiveRoot/$y/$m/$d
    mkdir -p $archive
    if [[ $clade == "I" ]]; then
        ln -f $(pwd)/mpxv.clade$clade.$today.{nwk,masked.vcf,metadata.tsv,masked.taxonium.jsonl,masked.pb}.gz $archive/
        ln -f $(pwd)/hgPhyloPlace.description.$clade.txt $archive/mpxv.clade$clade.$today.version.txt
    else
        # At first, clade II hMPXV was the only mpox, so cladeII was not part of the download file names.
        # Also, for clade II we're not making a tree with all clade II, only the 2017 outbreak, so cladeII
        # in the name wouldn't be entirely accurate either.
        for f in mpxv.clade$clade.$today.{nwk,masked.vcf,metadata.tsv,masked.taxonium.jsonl,masked.pb}.gz; do
            downloadF=$(echo $f | sed -re 's/.cladeII//;')
            ln -f $(pwd)/$f $archive/$downloadF
        done
        ln -f $(pwd)/hgPhyloPlace.description.$clade.txt $archive/mpxv.$today.version.txt
    fi

    # Update 'latest' in $archiveRoot
    for f in $archive/mpxv*.$today.*; do
        latestF=$(echo $(basename $f) | sed -re 's/'$today'/latest/')
        ln -f $f $archiveRoot/$latestF
    done

    # Update hgdownload-test link for archive
    mkdir -p /usr/local/apache/htdocs-hgdownload/hubs/$asmDir/$downloadDir/$y/$m
    ln -sf $archive /usr/local/apache/htdocs-hgdownload/hubs/$asmDir/$downloadDir/$y/$m
    # rsync to hgdownload hubs dir
    for h in hgdownload1 hgdownload3; do
        if rsync -a -L --delete /usr/local/apache/htdocs-hgdownload/hubs/$asmDir/$downloadDir/* \
                 qateam@$h:/mirrordata/hubs/$asmDir/$downloadDir/; then
            true
        else
            echo ""
            echo "*** rsync to $h failed; disk full? ***"
            echo ""
        fi
    done

    if [[ $clade == "II" ]]; then
        set +o pipefail
        grep 'Could not' annotate.$clade.log | cat
        grep skipping annotate.$clade.log | cat
        set -o pipefail
    fi

    cat hgPhyloPlace.description.$clade.txt

    zcat mpxv.clade$clade.$today.metadata.tsv.gz | tail -n+2 | cut -f 13 | sort | uniq -c

done

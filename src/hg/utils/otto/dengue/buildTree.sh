#!/bin/bash
source ~/.bashrc
set -beEu -o pipefail

# Align INSDC sequences to references and build 4 trees, one for each subtype 1-4.

dengueScriptDir=$(dirname "${BASH_SOURCE[0]}")

today=$(date +%F)

dengueDir=/hive/data/outside/otto/dengue
dengueNcbiDir=$dengueDir/ncbi/ncbi.latest

usherDir=~angie/github/usher
usherSampled=$usherDir/build/usher-sampled
usher=$usherDir/build/usher
matUtils=$usherDir/build/matUtils
matOptimize=$usherDir/build/matOptimize

# Subtype 1
asmAcc1=GCF_000862125.1
gbff1=$dengueDir/NC_001477.1.gbff
refFa1=$dengueDir/NC_001477.1.fa
archiveRoot1=/hive/users/angie/publicTreesDenv1

# Subtype 2
asmAcc2=GCF_000871845.1
gbff2=$dengueDir/NC_001474.2.gbff
refFa2=$dengueDir/NC_001474.2.fa
archiveRoot2=/hive/users/angie/publicTreesDenv2

# Subtype 3
asmAcc3=GCF_000866625.1
gbff3=$dengueDir/NC_001475.2.gbff
refFa3=$dengueDir/NC_001475.2.fa
archiveRoot3=/hive/users/angie/publicTreesDenv3

# Subtype 4
asmAcc4=GCF_000865065.1
gbff4=$dengueDir/NC_002640.1.gbff
refFa4=$dengueDir/NC_002640.1.fa
archiveRoot4=/hive/users/angie/publicTreesDenv4

if [[ ! -d $dengueDir/ncbi/ncbi.$today || ! -s $dengueDir/ncbi/ncbi.$today/genbank.fa.xz ]]; then
    mkdir -p $dengueDir/ncbi/ncbi.$today
    $dengueScriptDir/getNcbiDengue.sh >& $dengueDir/ncbi/ncbi.$today/getNcbiDengue.log
fi

buildDir=$dengueDir/build/$today
mkdir -p $buildDir
cd $buildDir

# Use metadata to make a renaming file
cat $dengueNcbiDir/metadata.tsv \
| sed -re 's@([\t /])h?rsv(-?[ab])?/@\1@ig;' \
| sed -re 's@([\t /])human/@\1@ig;' \
| sed -re 's@([\t /])homo sapiens/@\1@ig;' \
| sed -re 's@Capsid, Premembrane, Envelope, Non-structural 1, Non-structural 2a, Non-structural 2b, Non-structural 3, Non-structural 4a, Non-structural 4b, Non-structural 5 gene for polyprotein, Capsid, Premembrane, Envelope, Non-structural 1, Non-structural 2a, Non-structural 2b, Non-structural 3, Non-structural 4a, Non-structural 4b, Non-structural 5 region, @@;' \
| sed -re 's@Capsid, Premembrane, Envelope, Non-structural 1, Non-structural 2a, Non-structural 2b, Non-structural 3, Non-structural 4a, Non-structural 4b, Non-structural 5 gene for polyprotein, 1 region, @@;' \
| sed -re 's@(NS1, NS2A, NS2B, NS3, NS4A, NS4B, NS5)@@;' \
    > tweakedMetadata.tsv
cut -f 1,12 tweakedMetadata.tsv \
| trimWordy.pl \
    > accToIdFromTitle.tsv

#*** If this is really the same as RSV then it should become a real script.
#*** -- I changed $w[15] to $w[16] because I added serotype as a metadata column in getNcbiDengue.sh.
#*** Still should probably become a proper script -- I could cut -f... from metadata.
#*** Also had to add the paren-stripping seds.
join -t$'\t' <(sort tweakedMetadata.tsv) accToIdFromTitle.tsv \
| perl -wne 'chomp; @w=split(/\t/);
    my ($acc, $iso, $loc, $date, $str, $tid) = ($w[0], $w[1], $w[3], $w[4], $w[14], $w[16]);
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
| sed -re 's/[():'"'"']/_/g; s/\[/_/g; s/\]/_/g;' \
| sed -re 's/_+\|/|/;' \
    > renaming.tsv

# This builds the whole tree from scratch!  Eventually we'll want to add only the new sequences
# to yesterday's tree.

echo '()' > emptyTree.nwk

for subtype in 1 2 3 4; do
    case $subtype in
    1)
        refFa=$refFa1
        gbff=$gbff1
        asmAcc=$asmAcc1
        archiveRoot=$archiveRoot1
        ;;
    2)
        refFa=$refFa2
        gbff=$gbff2
        asmAcc=$asmAcc2
        archiveRoot=$archiveRoot2
        ;;
    3)
        refFa=$refFa3
        gbff=$gbff3
        asmAcc=$asmAcc3
        archiveRoot=$archiveRoot3
        ;;
    4)
        refFa=$refFa4
        gbff=$gbff4
        asmAcc=$asmAcc4
        archiveRoot=$archiveRoot4
        ;;
    esac

    # Run nextclade -- fortunately they used RefSeqs.
    nextclade dataset get --name community/v-gen-lab/dengue/denv$subtype \
        --output-zip denv$subtype.zip
    time nextclade run \
        -D denv$subtype.zip \
        -j 32 \
        --include-reference \
        --retry-reverse-complement true \
        --output-fasta aligned.$subtype.fa.xz \
        --output-columns-selection seqName,clade,totalSubstitutions,totalDeletions,totalInsertions,totalMissing,totalNonACGTNs,alignmentStart,alignmentEnd,substitutions,deletions,insertions,aaSubstitutions,aaDeletions,aaInsertions,missing,unknownAaRanges,nonACGTNs \
        --output-tsv nextclade.denv$subtype.tsv \
        $dengueDir/ncbi/ncbi.$today/genbank.fa.xz \
        >& nextclade.denv$subtype.log

# If it becomes necessary, add -excludeFile=$dengueScriptDir/exclude.ids
    time faToVcf -verbose=2 -includeRef -includeNoAltN \
        <(xzcat aligned.$subtype.fa.xz) stdout \
        | vcfRenameAndPrune stdin renaming.tsv stdout \
        | pigz -p 8 \
        > all.$subtype.vcf.gz

    time $usherSampled -T 16 -A -e 5 \
        -t emptyTree.nwk \
        -v all.$subtype.vcf.gz \
        -o denv$subtype.$today.preFilter.pb\
        --optimization_radius 0 --batch_size_per_process 10 \
        > usher.addNew.$subtype.log 2>usher-sampled.$subtype.stderr

    # Filter out branches that are so long they must lead to some other subtype
    $matUtils extract -i denv$subtype.$today.preFilter.pb \
        --max-branch-length 1000 \
        -O -o denv$subtype.$today.preOpt.pb >& tmp.log

    # Optimize:
    time $matOptimize -T 16 -r 20 -M 2 -S move_log.$subtype \
        -i denv$subtype.$today.preOpt.pb \
        -o denv$subtype.$today.pb \
        >& matOptimize.$subtype.log
    rm -f *.pbintermediate*.pb
    chmod 664 denv$subtype.$today.pb

    # Make metadata that uses same names as tree
    echo -e "strain\tgenbank_accession\tdate\tcountry\tlocation\tlength\thost\tbioproject_accession\tbiosample_accession\tsra_accession\tauthors\tpublications\tNextclade_lineage" \
        > denv$subtype.$today.metadata.tsv
    sort $dengueNcbiDir/metadata.tsv \
    | perl -F'/\t/' -walne '$F[3] =~ s/(: ?|$)/\t/;  print join("\t", @F);' \
    | join -t$'\t' -o 1.1,1.1,1.6,1.4,1.5,1.8,1.9,1.10,1.11,1.12,1.14,1.15,2.2 \
        - <(cut -f 1,2 nextclade.denv$subtype.tsv | sort) \
    | join -t$'\t' -o 1.2,2.2,2.3,2.4,2.5,2.6,2.7,2.8,2.9,2.10,2.11,2.12,2.13 \
        <(sort renaming.tsv) \
        - \
    | sort \
        >> denv$subtype.$today.metadata.tsv
    pigz -f -p 8 denv$subtype.$today.metadata.tsv

    # Make a tree version description for hgPhyloPlace
    $matUtils extract -i denv$subtype.$today.pb -u samples.$subtype.$today >& tmp.log
    sampleCountComma=$(wc -l < samples.$subtype.$today \
              | sed -re 's/([0-9]+)([0-9]{3})$/\1,\2/; s/([0-9]+)([0-9]{3},[0-9]{3})$/\1,\2/;')
    echo "$sampleCountComma genomes from INSDC (GenBank/ENA/DDBJ) ($today)" \
        > hgPhyloPlace.description.$subtype.txt

    # Make a taxonium view
    usher_to_taxonium --input denv$subtype.$today.pb \
        --metadata denv$subtype.$today.metadata.tsv.gz \
        --columns genbank_accession,country,location,date,authors,Nextclade_lineage \
        --genbank $gbff \
        --name_internal_nodes \
        --title "Dengue subtype "$subtype" $today tree with $sampleCountComma genomes from INSDC" \
        --output denv$subtype.$today.taxonium.jsonl.gz \
        >& usher_to_taxonium.$subtype.log

    # Update links in /gbdb
    nc=$(basename $gbff .gbff)
    dir=/gbdb/wuhCor1/hgPhyloPlaceData/dengue/$nc
    mkdir -p $dir
    ln -sf $(pwd)/denv$subtype.$today.pb $dir/denv$subtype.latest.pb
    ln -sf $(pwd)/denv$subtype.$today.metadata.tsv.gz $dir/denv$subtype.latest.metadata.tsv.gz
    ln -sf $(pwd)/hgPhyloPlace.description.$subtype.txt $dir/denv$subtype.latest.version.txt


    # Extract Newick and VCF for anyone who wants to download those instead of protobuf
    $matUtils extract -i denv$subtype.$today.pb \
        -t denv$subtype.$today.nwk \
        -v denv$subtype.$today.vcf >& tmp.log
    pigz -p 8 -f denv$subtype.$today.nwk denv$subtype.$today.vcf

    # Link to public trees download directory hierarchy
    read y m d < <(echo $today | sed -re 's/-/ /g')
    archive=$archiveRoot/$y/$m/$d
    mkdir -p $archive
    ln -f $(pwd)/denv$subtype.$today.{nwk,vcf,metadata.tsv,taxonium.jsonl}.gz $archive/
    gzip -c denv$subtype.$today.pb > $archive/denv$subtype.$today.pb.gz
    ln -f $(pwd)/hgPhyloPlace.description.$subtype.txt $archive/denv$subtype.$today.version.txt

    # Update 'latest' in $archiveRoot
    for f in $archive/denv$subtype.$today.*; do
        latestF=$(echo $(basename $f) | sed -re 's/'$today'/latest/')
        ln -f $f $archiveRoot/$latestF
    done

    # Update hgdownload-test link for archive
    asmDir=$(echo $asmAcc \
        | sed -re 's@^(GC[AF])_([0-9]{3})([0-9]{3})([0-9]{3})\.([0-9]+)@\1/\2/\3/\4/\1_\2\3\4.\5@')
    mkdir -p /data/apache/htdocs-hgdownload/hubs/$asmDir/UShER_DENV-$subtype/$y/$m
    ln -sf $archive /data/apache/htdocs-hgdownload/hubs/$asmDir/UShER_DENV-$subtype/$y/$m
    # rsync to hgdownload hubs dir
    for h in hgdownload1 hgdownload3; do
        if rsync -a -L --delete /data/apache/htdocs-hgdownload/hubs/$asmDir/UShER_DENV-$subtype/* \
                 qateam@$h:/mirrordata/hubs/$asmDir/UShER_DENV-$subtype/; then
            true
        else
            echo ""
            echo "*** rsync to $h failed; disk full? ***"
            echo ""
        fi
    done
done

rm -f mutation-paths.txt *.pre*.pb final-tree.nh
nice gzip -f *.log *.tsv move_log* *.stderr samples.*

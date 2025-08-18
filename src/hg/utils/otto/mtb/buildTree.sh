#!/bin/bash
source ~/.bashrc
set -beEu -o pipefail

# Align INSDC sequences to reference and build tree.

mtbScriptDir=$(dirname "${BASH_SOURCE[0]}")

today=$(date +%F)

mtbDir=/hive/data/outside/otto/mtb
mtbNcbiDir=$mtbDir/ncbi/ncbi.latest

usherDir=~angie/github/usher
usherSampled=$usherDir/build/usher-sampled
usher=$usherDir/build/usher
matUtils=$usherDir/build/matUtils
matOptimize=$usherDir/build/matOptimize

asmAcc=GCF_000195955.2
gbff=$mtbDir/NC_000962.3.gbff
refFa=$mtbDir/NC_000962.3.fa
archiveRoot=/hive/users/angie/publicTreesMtb

if [[ ! -d $mtbDir/ncbi/ncbi.$today || ! -s $mtbDir/ncbi/ncbi.$today/genbank.fa.xz ]]; then
    mkdir -p $mtbDir/ncbi/ncbi.$today
    $mtbScriptDir/getNcbiMtb.sh >& $mtbDir/ncbi/ncbi.$today/getNcbiMtb.log
fi

buildDir=$mtbDir/build/$today
mkdir -p $buildDir
cd $buildDir

# Make renaming file from metadata.tsv.
perl -wne 'chomp;
    s/\tmissing\t/\t\t/g;
    s/\bnot applicable\b//g;
    @w=split(/\t/);
    my ($acc, $biosample, $date, $loc, $host, $asmName, $len, $bStrain, $org, $oStrain, $iso) = @w;
    my $name = $bStrain ? $bStrain : $iso ? $iso : $oStrain ? $oStrain : $asmName ? $asmName : "";
    my $country = $loc;  $country =~ s/:.*//;
    my $COU = $country;  $COU =~ s/^(\w{3}).*/$1/;  $COU = uc($COU);
    if ($country eq "United Kingdom") { $COU = "UK"; }
    if ($name !~ /$country/ && $name !~ /\b$COU\b/ && $name ne "") { $name = "$country/$name"; }
    $name =~ s/[,;]//g;
    $name =~ s/ /_/g;
    my $year = $date;  $year =~ s/-.*//;
    my $year2 = $year;   $year2 =~ s/^\d{2}(\d{2})$/$1/;
    if ($name ne "" && $name !~ /$year/ && $name !~ /\/$year2$/) { $name = "$name/$year"; }
    if ($date eq "") { $date = "?"; }
    my $fullName = $name ? "$name|$acc|$date" : "$acc|$date";
    $fullName =~ s/[ ,:()]/_/g;
    print "$acc\t$fullName\n";' \
        $mtbNcbiDir/metadata.tsv \
| sed -re 's/[():'"'"']/_/g; s/\[/_/g; s/\]/_/g;' \
| sed -re 's/_+\|/|/;' \
    > renaming.tsv

# This adds all sequences to Lily's tree even if they were added yesterday.
# Eventually we'll want to add only the new sequences to yesterday's tree.

# These sequences are too diverged for nextclade -- use minimap2 and make maple diff file.
xzcat $mtbDir/ncbi/ncbi.$today/genbank.fa.xz \
| minimap2 -x asm20 --score-N 0 --secondary no --cs -t 32 $refFa - \
| pafToMaple stdin aligned.maple -minNonNBases=4000000 -rename=renaming.tsv \
    -excludeBed=$mtbDir/R00000039_repregions.bed

time $usherSampled -T 64 -A -e 5 \
    -i $mtbDir/lily_pruned_20240221.pb \
    --diff aligned.maple \
    --ref $refFa \
    -o mtb.$today.preFilter.pb \
    --optimization_radius 0 --batch_size_per_process 10 \
    > usher.addNew.log 2>usher-sampled.stderr

# Filter out a few sequences on very long branches.
$matUtils extract -i mtb.$today.preFilter.pb \
    --max-parsimony 500 \
    --max-branch-length 2000 \
    -O -o mtb.$today.preOpt.pb >& tmp.log

# Optimize:
time $matOptimize -T 64 -r 20 -M 2 -S move_log \
    -i mtb.$today.preOpt.pb \
    -o mtb.$today.pb \
    >& matOptimize.log
rm -f *.pbintermediate*.pb
chmod 664 mtb.$today.pb

# Make metadata that uses same names as tree
echo -e "strain\tgenbank_accession\tbiosample_accession\tdate\tcountry\tlocation\thost\tlength\tbioproject_accession\tsra_sample_accession" \
        > gb.metadata.tsv
sort $mtbNcbiDir/metadata.tsv \
| sed -re 's/not applicable//g;' \
| perl -F'/\t/' -walne '$F[3] =~ s/(: ?|$)/\t/;  print join("\t", @F);' \
| join -t$'\t' -o 1.2,2.1,2.2,2.3,2.4,2.5,2.6,2.8,2.9,2.10 \
    <(sort renaming.tsv) \
    - \
| sort \
    >> gb.metadata.tsv
pigz -f -p 8 gb.metadata.tsv

$mtbScriptDir/combineMetadata.py gb.metadata.tsv.gz $mtbDir/tree_metadata_etc_fran_v3.tsv.gz \
| pigz -p 8 \
    > mtb.$today.metadata.tsv.gz

# Make a tree version description for hgPhyloPlace
$matUtils extract -i mtb.$today.pb -u samples.$today >& tmp.log
sampleCountComma=$(wc -l < samples.$today \
                   | sed -re 's/([0-9]+)([0-9]{3})$/\1,\2/; s/([0-9]+)([0-9]{3},[0-9]{3})$/\1,\2/;')
echo "$sampleCountComma genomes from SRA and INSDC (GenBank/ENA/DDBJ) ($today)" \
    > hgPhyloPlace.description.txt

# Make a taxonium view
#***    --columns genbank_accession,country,location,date,biosample_accession \
usher_to_taxonium --input mtb.$today.pb \
    --metadata mtb.$today.metadata.tsv.gz \
    --genbank $gbff \
    --only_variable_sites \
    --name_internal_nodes \
    --title "M. tb $today tree with $sampleCountComma genomes from INSDC and 66498 genomes from SRA" \
    --output mtb.$today.taxonium.jsonl.gz \
    >& usher_to_taxonium.log

    # Update links to latest protobuf and metadata in hgwdev cgi-bin directories

#*** TODO: use the /gbdb/ dir not cgi-bin

#***for dir in /usr/local/apache/cgi-bin{-angie,,-beta}/hgPhyloPlaceData/$asmAcc; do
for dir in /usr/local/apache/cgi-bin{-angie,}/hgPhyloPlaceData/$asmAcc; do
    ln -sf $(pwd)/mtb.$today.pb $dir/mtb.latest.pb
    ln -sf $(pwd)/mtb.$today.metadata.tsv.gz $dir/mtb.latest.metadata.tsv.gz
    ln -sf $(pwd)/hgPhyloPlace.description.txt $dir/mtb.latest.version.txt
done

    # Extract Newick and VCF for anyone who wants to download those instead of protobuf
    $matUtils extract -i mtb.$today.pb \
        -t mtb.$today.nwk \
        -v mtb.$today.vcf >& tmp.log
    pigz -p 8 -f mtb.$today.nwk mtb.$today.vcf

    # Link to public trees download directory hierarchy
    read y m d < <(echo $today | sed -re 's/-/ /g')
    archive=$archiveRoot/$y/$m/$d
    mkdir -p $archive
    ln -f $(pwd)/mtb.$today.{nwk,vcf,metadata.tsv,taxonium.jsonl}.gz $archive/
    gzip -c mtb.$today.pb > $archive/mtb.$today.pb.gz
    ln -f $(pwd)/hgPhyloPlace.description.txt $archive/mtb.$today.version.txt

    # Update 'latest' in $archiveRoot
    for f in $archive/mtb.$today.*; do
        latestF=$(echo $(basename $f) | sed -re 's/'$today'/latest/')
        ln -f $f $archiveRoot/$latestF
    done

    # Update hgdownload-test link for archive
    asmDir=$(echo $asmAcc \
        | sed -re 's@^(GC[AF])_([0-9]{3})([0-9]{3})([0-9]{3})\.([0-9]+)@\1/\2/\3/\4/\1_\2\3\4.\5@')
    mkdir -p /usr/local/apache/htdocs-hgdownload/hubs/$asmDir/UShER_Mtb/$y/$m
    ln -sf $archive /usr/local/apache/htdocs-hgdownload/hubs/$asmDir/UShER_Mtb/$y/$m
    # rsync to hgdownload hubs dir
    for h in hgdownload1 hgdownload2 hgdownload3; do
        if rsync -v -a -L --delete /usr/local/apache/htdocs-hgdownload/hubs/$asmDir/UShER_Mtb/* \
                 qateam@$h:/mirrordata/hubs/$asmDir/UShER_Mtb/ ; then
            true
        else
            echo ""
            echo "*** rsync to $h failed; disk full? ***"
            echo ""
        fi
    done

rm -f mutation-paths.txt *.pre*.pb final-tree.nh
nice gzip -f *.log *.tsv move_log* *.stderr samples.*

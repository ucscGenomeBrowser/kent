#!/bin/tcsh -e
# assumes running in build directory
set db=$1

if "$db" == "hg19" then
    set grc=GRCh37
    set liftUp="liftUp -type=.bed stdout ../../hg19.lift warn stdin"
else if "$db" == "hg38" then
    set grc=GRCh38
    set liftUp="liftUp -type=.bed stdout ../../hg38.lift warn stdin"
else if "$db" == "hg18" then
    # left unchanged, as we no longer build this for hg18.  If that changes, we'd need to build a liftUp file
    # to translate the chromosome names.
    set grc=NCBI36
    set liftUp=cat
else
    echo "Unfamiliar assembly: $db - please edit the script to provide the correct GRC name and liftUp command"
    exit 1;
endif

cd $1

# set today = `date +%Y_%m_%d`
# mkdir /hive/data/genomes/hg19/bed/isca/$today
# cd /hive/data/genomes/hg19/bed/isca/$today

# Get variants submitted on this assembly, and variants remapped from other assemblies.
setenv LANG C

wget -N -q "ftp://ftp.ncbi.nlm.nih.gov/pub/dbVar/data/Homo_sapiens/by_study/gvf/nstd37.${grc}.variant*.gvf.gz"
wget -N -q "ftp://ftp.ncbi.nlm.nih.gov/pub/dbVar/data/Homo_sapiens/by_study/gvf/nstd101.${grc}.variant*.gvf.gz"
wget -N -q "ftp://ftp.ncbi.nlm.nih.gov/pub/dbVar/data/Homo_sapiens/by_study/gvf/nstd45.${grc}.variant*.gvf.gz"

zcat nstd101*.gvf.gz nstd37*.gvf.gz | ../../gvfToBed8Attrs.pl | $liftUp | sort > foo

# Not sure why this step exists.  It appears to just be a chromosome filter, but that's effectively already done ...
join ../../chromInfo.$db foo -t '	' > isca.bed

zcat nstd45*.gvf.gz | ../../gvfToBed8Attrs.pl | $liftUp | sort > foo

# Not sure why this step exists (as above).  It appears to just be a chromosome filter, but that's effectively already done ...
join ../../chromInfo.$db foo -t '	' > iscaCurated.bed
rm -f foo

foreach subtrack (Benign Pathogenic)
  grep -w  $subtrack isca.bed > isca$subtrack.bed
  echo -n "Loading isca${subtrack}New and iscaCurated${subtrack}New ... "
  hgLoadBed -tab -renameSqlTable -sqlTable=$HOME/kent/src/hg/lib/bed8Attrs.sql \
    -allowStartEqualEnd $db isca"$subtrack"New isca$subtrack.bed >&/dev/null
  grep -w  $subtrack iscaCurated.bed > iscaCurated$subtrack.bed
  hgLoadBed -tab -renameSqlTable -sqlTable=$HOME/kent/src/hg/lib/bed8Attrs.sql \
    -allowStartEqualEnd $db iscaCurated"$subtrack"New iscaCurated$subtrack.bed >&/dev/null
  echo "Success"
end

# The subcategories of Uncertain need a bit more sophisticated treatment:
set subtrack = Uncertain
grep -w $subtrack isca.bed \
| grep -vi 'Uncertain Significance: likely' \
  > isca$subtrack.bed
echo -n "Loading isca${subtrack}New ... "
hgLoadBed -tab -renameSqlTable -sqlTable=$HOME/kent/src/hg/lib/bed8Attrs.sql \
  -allowStartEqualEnd $db isca"$subtrack"New isca$subtrack.bed >&/dev/null
echo "Success"

foreach unc (benign pathogenic)
  set subtrack = Likely`perl -we 'print ucfirst("'$unc'");'`
  grep -wi "Likely $unc" isca.bed \
    > isca$subtrack.bed
  echo -n "Loading isca${subtrack}New ... "
  hgLoadBed -tab -renameSqlTable -sqlTable=$HOME/kent/src/hg/lib/bed8Attrs.sql \
    -allowStartEqualEnd $db isca"$subtrack"New isca$subtrack.bed >&/dev/null
  echo "Success"
end

# make bedGraphs
hgsql -N -e "SELECT chrom, chromStart, chromEnd FROM iscaPathogenicNew \
    WHERE attrVals LIKE '%number_gain%'" $db | sort \
| bedItemOverlapCount $db stdin > iscaPathGain.bedGraph

hgsql -N -e "SELECT chrom, chromStart, chromEnd FROM iscaPathogenicNew \
    WHERE attrVals LIKE '%number_loss%'" $db | sort \
| bedItemOverlapCount $db stdin > iscaPathLoss.bedGraph

hgsql -N -e "SELECT chrom, chromStart, chromEnd FROM iscaBenignNew \
    WHERE attrVals LIKE '%number_gain%'" $db | sort \
| bedItemOverlapCount $db stdin > iscaBenignGain.bedGraph

hgsql -N -e "SELECT chrom, chromStart, chromEnd FROM iscaBenignNew \
    WHERE attrVals LIKE '%number_loss%'" $db | sort \
| bedItemOverlapCount $db stdin > iscaBenignLoss.bedGraph

# load tables
echo -n "Loading bedGraphs ... "
hgLoadBed -bedGraph=4 $db iscaPathGainCumNew iscaPathGain.bedGraph >&/dev/null
hgLoadBed -bedGraph=4 $db iscaPathLossCumNew iscaPathLoss.bedGraph >&/dev/null
hgLoadBed -bedGraph=4 $db iscaBenignGainCumNew iscaBenignGain.bedGraph >&/dev/null
hgLoadBed -bedGraph=4 $db iscaBenignLossCumNew iscaBenignLoss.bedGraph >&/dev/null
echo "Success"

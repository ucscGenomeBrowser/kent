#!/bin/tcsh -e
# assumes running in build directory
set db=$1

if "$db" == "hg19" then
    set grc=GRCh37
    set liftUp=cat
else if "$db" == "hg38" then
    set grc=GRCh38
    set liftUp="liftUp -type=.bed stdout /cluster/data/hg38/jkStuff/dbVar.lift warn stdin"
else if "$db" == "hg18" then
    set grc=NCBI36
    set liftUp=cat
endif

cd $1

# set today = `date +%Y_%m_%d`
# mkdir /hive/data/genomes/hg19/bed/isca/$today
# cd /hive/data/genomes/hg19/bed/isca/$today

# Get variants submitted on this assembly, and variants remapped from other assemblies.
setenv LANG C

# ftp://ftp.ncbi.nlm.nih.gov/pub/dbVar/data/Homo_sapiens/by_study/nstd37_ClinGen_Laboratory-Submitted/gvf/
# ftp://ftp.ncbi.nlm.nih.gov/pub/dbVar/data/Homo_sapiens/by_study/nstd101_ClinGen_Kaminsky_et_al_2011/gvf/
# ftp://ftp.ncbi.nlm.nih.gov/pub/dbVar/data/Homo_sapiens/by_study/nstd45_ClinGen_Curated_Dosage_Sensitivity_Map/gvf/

wget -N -q "ftp://ftp.ncbi.nlm.nih.gov/pub/dbVar/data/Homo_sapiens/by_study/nstd37_ClinGen_Laboratory-Submitted/gvf/nstd37_ClinGen_Laboratory-Submitted.${grc}.*.all.germline.ucsc.gvf.gz"
wget -N -q "ftp://ftp.ncbi.nlm.nih.gov/pub/dbVar/data/Homo_sapiens/by_study/nstd45_ClinGen_Curated_Dosage_Sensitivity_Map/gvf/nstd45_ClinGen_Curated_Dosage_Sensitivity_Map.${grc}.*.all.germline.ucsc.gvf.gz"
wget -N -q "ftp://ftp.ncbi.nlm.nih.gov/pub/dbVar/data/Homo_sapiens/by_study/nstd101_ClinGen_Kaminsky_et_al_2011/gvf/nstd101_ClinGen_Kaminsky_et_al_2011.${grc}.*.all.germline.ucsc.gvf.gz"

# Remove patch contig mappings; we don't display them anyway.
rm -f *.p*.gvf.gz


zcat nstd101*.gvf.gz nstd37_ClinGen_Laboratory-Submitted*.gvf.gz | ../../gvfToBed8Attrs.pl | $liftUp | sort > foo
	
join ../../chromInfo.$db foo -t '	' > isca.bed

zcat nstd45_*.gvf.gz | ../../gvfToBed8Attrs.pl | $liftUp | sort > foo

join ../../chromInfo.$db foo -t '	' > iscaCurated.bed

foreach subtrack (Benign Pathogenic)
  grep -w  $subtrack isca.bed > isca$subtrack.bed
  hgLoadBed -tab -renameSqlTable -sqlTable=$HOME/kent/src/hg/lib/bed8Attrs.sql \
    -allowStartEqualEnd $db isca"$subtrack"New isca$subtrack.bed
  grep -w  $subtrack iscaCurated.bed > iscaCurated$subtrack.bed
  hgLoadBed -tab -renameSqlTable -sqlTable=$HOME/kent/src/hg/lib/bed8Attrs.sql \
    -allowStartEqualEnd $db iscaCurated"$subtrack"New iscaCurated$subtrack.bed
end

# The subcategories of Uncertain need a bit more sophisticated treatment:
set subtrack = Uncertain
grep -w $subtrack isca.bed \
| grep -vi 'Uncertain Significance: likely' \
  > isca$subtrack.bed
hgLoadBed -tab -renameSqlTable -sqlTable=$HOME/kent/src/hg/lib/bed8Attrs.sql \
  -allowStartEqualEnd $db isca"$subtrack"New isca$subtrack.bed

foreach unc (benign pathogenic)
  set subtrack = Likely`perl -we 'print ucfirst("'$unc'");'`
  grep -wi "Likely $unc" isca.bed \
    > isca$subtrack.bed
  hgLoadBed -tab -renameSqlTable -sqlTable=$HOME/kent/src/hg/lib/bed8Attrs.sql \
    -allowStartEqualEnd $db isca"$subtrack"New isca$subtrack.bed
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
hgLoadBed -bedGraph=4 $db iscaPathGainCumNew iscaPathGain.bedGraph
hgLoadBed -bedGraph=4 $db iscaPathLossCumNew iscaPathLoss.bedGraph
hgLoadBed -bedGraph=4 $db iscaBenignGainCumNew iscaBenignGain.bedGraph
hgLoadBed -bedGraph=4 $db iscaBenignLossCumNew iscaBenignLoss.bedGraph

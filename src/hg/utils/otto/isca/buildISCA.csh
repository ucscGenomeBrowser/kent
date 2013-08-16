#!/bin/tcsh -ex
# assumes running in build directory

# set today = `date +%Y_%m_%d`
# mkdir /hive/data/genomes/hg19/bed/isca/$today
# cd /hive/data/genomes/hg19/bed/isca/$today

# Get variants submitted on this assembly, and variants remapped from other assemblies.
wget -N ftp://ftp.ncbi.nlm.nih.gov/pub/dbVar/data/Homo_sapiens/by_study/nstd37_ISCA/gvf/nstd37_ISCA.GRCh37.submitted.all.germline.ucsc.gvf.gz
wget -N ftp://ftp.ncbi.nlm.nih.gov/pub/dbVar/data/Homo_sapiens/by_study/nstd37_ISCA/gvf/nstd37_ISCA.GRCh37.remap.all.germline.ucsc.gvf.gz
wget -N ftp://ftp.ncbi.nlm.nih.gov/pub/dbVar/data/Homo_sapiens/by_study/nstd45_ISCA_curated_dataset/gvf/nstd45_ISCA_curated_dataset.GRCh37.remap.all.germline.ucsc.gvf.gz
zcat nstd37_ISCA*.gvf.gz \
    | ~/kent/src/hg/utils/automation/gvfToBed8Attrs.pl \
      > isca.bed
zcat nstd45_ISCA*.gvf.gz \
    | ~/kent/src/hg/utils/automation/gvfToBed8Attrs.pl \
      > iscaCurated.bed

foreach subtrack (Benign Pathogenic)
  grep -w  $subtrack isca.bed > isca$subtrack.bed
  hgLoadBed -tab -renameSqlTable -sqlTable=$HOME/kent/src/hg/lib/bed8Attrs.sql \
    -allowStartEqualEnd hg19 isca"$subtrack"New isca$subtrack.bed
  grep -w  $subtrack iscaCurated.bed > iscaCurated$subtrack.bed
  hgLoadBed -tab -renameSqlTable -sqlTable=$HOME/kent/src/hg/lib/bed8Attrs.sql \
    -allowStartEqualEnd hg19 iscaCurated"$subtrack"New iscaCurated$subtrack.bed
end

# The subcategories of Uncertain need a bit more sophisticated treatment:
set subtrack = Uncertain
grep -w $subtrack isca.bed \
| grep -vi 'Uncertain Significance: likely' \
  > isca$subtrack.bed
hgLoadBed -tab -renameSqlTable -sqlTable=$HOME/kent/src/hg/lib/bed8Attrs.sql \
  -allowStartEqualEnd hg19 isca"$subtrack"New isca$subtrack.bed

foreach unc (benign pathogenic)
  set subtrack = Likely`perl -we 'print ucfirst("'$unc'");'`
  grep -wi "Uncertain Significance: likely $unc" isca.bed \
    > isca$subtrack.bed
  hgLoadBed -tab -renameSqlTable -sqlTable=$HOME/kent/src/hg/lib/bed8Attrs.sql \
    -allowStartEqualEnd hg19 isca"$subtrack"New isca$subtrack.bed
end

# make bedGraphs
hgsql -N -e "SELECT chrom, chromStart, chromEnd FROM iscaPathogenicNew \
    WHERE attrVals LIKE '%number_gain%'" hg19 | sort \
| bedItemOverlapCount hg19 stdin > iscaPathGain.bedGraph

hgsql -N -e "SELECT chrom, chromStart, chromEnd FROM iscaPathogenicNew \
    WHERE attrVals LIKE '%number_loss%'" hg19 | sort \
| bedItemOverlapCount hg19 stdin > iscaPathLoss.bedGraph

# load tables
hgLoadBed -bedGraph=4 hg19 iscaPathGainCumNew iscaPathGain.bedGraph
hgLoadBed -bedGraph=4 hg19 iscaPathLossCumNew iscaPathLoss.bedGraph

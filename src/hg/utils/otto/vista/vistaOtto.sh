#! /bin/bash
# Check if any of the latest files differ from the current files

cd /hive/data/outside/otto/vista


wget -q  https://gitlab.com/egsb-mfgl/vista-data/-/raw/main/locus_ucsc_hg38.bed  -O vista.hg38.latest.bed
wget -q  https://gitlab.com/egsb-mfgl/vista-data/-/raw/main/locus_ucsc_mm10.bed  -O vista.mm10.latest.bed

bedToBigBed -tab -sort -type=bed9+1 -as=vista.as vista.hg38.latest.bed \
  https://hgdownload.soe.ucsc.edu/goldenPath/hg38/bigZips/hg38.chrom.sizes vista.hg38.latest.bb \
  > /dev/null 2>&1

bedToBigBed -tab -sort -type=bed9+1 -as=vista.as vista.mm10.latest.bed \
  https://hgdownload.soe.ucsc.edu/goldenPath/mm10/bigZips/mm10.chrom.sizes vista.mm10.latest.bb \
  > /dev/null 2>&1

# Check if the files are the same
if cmp -s vista.hg38.latest.bb vista.hg38.bb; then
  # Files are the same, exit silently
  rm vista.hg38.latest.bb vista.hg38.latest.bed
  rm vista.mm10.latest.bb vista.mm10.latest.bed
  exit 0
else
  # Files are different, continue with the script or add actions
  echo "Updating vista track..."
fi

oldCountHg38=$(bigBedInfo vista.hg38.bb | grep -i "itemCount" | awk '{gsub(",", "", $NF); print $NF}')
oldCountMm10=$(bigBedInfo vista.mm10.bb | grep -i "itemCount" | awk '{gsub(",", "", $NF); print $NF}')

newCountHg38=$(bigBedInfo vista.hg38.latest.bb | grep -i "itemCount" | awk '{gsub(",", "", $NF); print $NF}')
newCountMm10=$(bigBedInfo vista.mm10.latest.bb | grep -i "itemCount" | awk '{gsub(",", "", $NF); print $NF}')

# Calculate the percentage difference
diffHg38=$(echo "scale=2; (($newCountHg38 - $oldCountHg38) / $oldCountHg38) * 100" | bc)
diffMm10=$(echo "scale=2; (($newCountMm10 - $oldCountMm10) / $oldCountMm10) * 100" | bc)

# Get the absolute values of the differences
absDiffHg38=$(echo "$diffHg38" | sed 's/-//')
absDiffMm10=$(echo "$diffMm10" | sed 's/-//')

# Check if any difference exceeds 10%
if (( $(echo "$absDiffHg38 > 10" | bc -l) || $(echo "$absDiffMm10 > 10" | bc -l) )); then
  echo
  echo "Error: Difference in item count exceeds 10%."
  echo "Difference in hg38: $diffHg38%"
  echo "Difference in Mm10: $diffMm10%"
  exit 1
fi

# Replace old files with new ones
mv vista.hg38.latest.bb vista.hg38.bb
mv vista.mm10.latest.bb vista.mm10.bb

liftOver -tab  -bedPlus=9 vista.hg38.latest.bed \
	/hive/data/gbdb/hg38/liftOver/hg38ToHg19.over.chain.gz vista.hg19.bed unMapped  > /dev/null 2>&1
bedToBigBed -tab -sort -type=bed9+1 -as=vista.as vista.hg19.bed \
       	https://hgdownload.soe.ucsc.edu/goldenPath/hg19/bigZips/hg19.chrom.sizes vista.hg19_updated.bb  > /dev/null 2>&1

liftOver -tab  -bedPlus=9 vista.mm10.latest.bed \
	/hive/data/gbdb/mm10/liftOver/mm10ToMm39.over.chain.gz vista.mm39.bed unMapped  > /dev/null 2>&1
bedToBigBed -tab -sort -type=bed9+1 -as=vista.as vista.mm39.bed \
       	https://hgdownload.soe.ucsc.edu/goldenPath/mm39/bigZips/mm39.chrom.sizes vista.mm39_updated.bb  > /dev/null 2>&1

echo
echo "Item counts for hg38 old vs. new bigBed. Old: $oldCountHg38 New: $newCountHg38"
echo "Item counts for mm10 old vs. new bigBed. Old: $oldCountMm10 New: $newCountMm10"
echo
echo "VISTA tracks updated successfully."

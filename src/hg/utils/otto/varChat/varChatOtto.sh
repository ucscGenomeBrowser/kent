#!/bin/bash

set -e -o pipefile

cd /hive/data/outside/otto/varChat

wget -q https://ucsc-engenome-varchat.s3.eu-west-1.amazonaws.com/latest/VarChat_GRCh38_latest.bb -O varChat.hg38.latest.bb
wget -q https://ucsc-engenome-varchat.s3.eu-west-1.amazonaws.com/latest/VarChat_GRCh37_latest.bb -O varChat.hg19.latest.bb

# Check if the files are the same
if cmp -s varChat.hg38.latest.bb varChat.hg38.bb; then
  # Files are the same, exit silently
  rm varChat.hg38.latest.bb
  rm varChat.hg19.latest.bb
  exit 0
else
  # Files are different, continue with the script or add actions
  echo "Updating VarChat track..."
fi

oldCountHg38=$(bigBedInfo varChat.hg38.bb | grep -i "itemCount" | awk '{print $NF}')
oldCountHg19=$(bigBedInfo varChat.hg19.bb | grep -i "itemCount" | awk '{print $NF}')

newCountHg38=$(bigBedInfo varChat.hg38.latest.bb | grep -i "itemCount" | awk '{print $NF}')
newCountHg19=$(bigBedInfo varChat.hg19.latest.bb | grep -i "itemCount" | awk '{print $NF}')

# Calculate the percentage difference
diffHg38=$(echo "scale=2; (($newCountHg38 - $oldCountHg38) / $oldCountHg38) * 100" | bc)
diffHg19=$(echo "scale=2; (($newCountHg19 - $oldCountHg19) / $oldCountHg19) * 100" | bc)

# Get the absolute values of the differences
absDiffHg38=$(echo "$diffHg38" | sed 's/-//')
absDiffHg19=$(echo "$diffHg19" | sed 's/-//')

# Check if the absolute difference is greater than 20%
if (( $(echo "$absDiffHg38 > 20" | bc -l) || $(echo "$absDiffHg19 > 20" | bc -l) )); then
    echo
    echo "Error: Difference in item count exceeds 20%."
    echo "Difference in hg38: $diffHg38%"
    echo "Difference in hg19: $diffHg19%"
    exit 1
fi

# If the difference is within the 20%, proceed
mv varChat.hg38.latest.bb varChat.hg38.bb
mv varChat.hg19.latest.bb varChat.hg19.bb

wget -q https://ucsc-engenome-varchat.s3.eu-west-1.amazonaws.com/latest/version.txt -O version.txt

echo
echo "Item counts for hg38 old vs. new bigBed. Old: $oldCountHg38 New: $newCountHg38"
echo "Item counts for hg19 old vs. new bigBed. Old: $oldCountHg19 New: $newCountHg19"
echo
echo "VarChat track built successfully."

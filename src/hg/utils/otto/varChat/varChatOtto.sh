#!/bin/bash

set -e -o pipefail

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

# /gbdb/hg38/bbi/varChatVersion.txt symlinks straight to this file, so the browser reads
# it directly, and wget truncates its -O target before it has anything to put there. Fetch
# to a temp file and only replace the live one if something actually arrived: the bigBeds
# above are already live by this point, so a failed fetch would otherwise leave the track
# showing a blank version until the next upstream release, which can be many months away.
# refs #38300
if wget -q https://ucsc-engenome-varchat.s3.eu-west-1.amazonaws.com/latest/version.txt -O version.new.txt && [ -s version.new.txt ]; then
    mv version.new.txt version.txt
else
    rm -f version.new.txt
    echo "Warning: could not fetch the VarChat version file, keeping $(cat version.txt 2>/dev/null)"
fi

echo
echo "Item counts for hg38 old vs. new bigBed. Old: $oldCountHg38 New: $newCountHg38"
echo "Item counts for hg19 old vs. new bigBed. Old: $oldCountHg19 New: $newCountHg19"
echo
echo "VarChat track built successfully."

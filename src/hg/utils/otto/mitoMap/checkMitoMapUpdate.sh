#! /bin/bash

cd /hive/data/outside/otto/mitoMap

# mitomap.org sits behind a Cloudflare bot check that answers our requests with 403,
# so MitoMap told us to use their mirror until their IT sorts it out. See #38097.
mitoMapUrl=https://fr.mitomap.org

# Download a file, stopping the run if it fails or comes back empty. wget writes an
# output file even on a 403, and without this the build would carry on from empty
# input and only get caught later by the item count check.
downloadFile() {
  if ! wget -q "$mitoMapUrl/$1" -O "$2" || [ ! -s "$2" ]; then
    echo "Error: could not download $mitoMapUrl/$1"
    exit 1
  fi
}

downloadFile downloads/VariantsControl.tsv variantsControl.latest.tsv
downloadFile downloads/VariantsCoding.tsv variantsCoding.latest.tsv
downloadFile downloads/MutationsRNA.tsv mutationsRNA.latest.tsv
downloadFile downloads/MutationsCodingControl.tsv mutationsCodingControl.latest.tsv

# Flag to track if any files are different
run_script=false

# Function to check if two files are the same
check_files() {
  if ! cmp -s "$1" "$2"; then
    echo "Update needed for $2. Updating MitoMap track..."
    run_script=true
  fi
}

# Compare each pair of files
check_files "mutationsCodingControl.latest.tsv" "mutationsCodingControl.tsv"
check_files "mutationsRNA.latest.tsv" "mutationsRNA.tsv"
check_files "variantsCoding.latest.tsv" "variantsCoding.tsv"
check_files "variantsControl.latest.tsv" "variantsControl.tsv"

# If any files were different, continue script execution
if $run_script; then
  echo "Proceeding with MitoMap update..."
  # Add your script logic here
else
  rm mutationsCodingControl.latest.tsv mutationsRNA.latest.tsv variantsCoding.latest.tsv variantsControl.latest.tsv
  exit 0
fi

python ./buildMitoMap.py

oldCountDiseaseMuts=$(bigBedInfo mitoMapDiseaseMuts.bb | grep -i "itemCount" | awk '{print $NF}' | sed 's/,//g')
oldCountVars=$(bigBedInfo mitoMapVars.bb | grep -i "itemCount" | awk '{print $NF}' | sed 's/,//g')

newCountDiseaseMuts=$(bigBedInfo mitoMapDiseaseMuts.new.bb | grep -i "itemCount" | awk '{print $NF}' | sed 's/,//g')
newCountVars=$(bigBedInfo mitoMapVars.new.bb | grep -i "itemCount" | awk '{print $NF}' | sed 's/,//g')

# Calculate the percentage difference
diffDiseaseMuts=$(echo "scale=2; (($newCountDiseaseMuts - $oldCountDiseaseMuts) / $oldCountDiseaseMuts) * 100" | bc)
diffVars=$(echo "scale=2; (($newCountVars - $oldCountVars) / $oldCountVars) * 100" | bc)

# Get the absolute values of the differences
absDiffDiseaseMuts=$(echo "$diffDiseaseMuts" | sed 's/-//')
absDiffVars=$(echo "$diffVars" | sed 's/-//')

# Check if the absolute difference is greater than 20%
if (( $(echo "$absDiffDiseaseMuts > 20" | bc -l) || $(echo "$absDiffVars > 20" | bc -l) )); then
    echo
    echo "Error: Difference in item count exceeds 20%."
    echo "Difference in disease mutations: $absDiffDiseaseMuts%"
    echo "Difference in variants: $absDiffVars%"
    exit 1
fi

# If the difference is within the 20%, proceed
mv mitoMapDiseaseMuts.new.bb mitoMapDiseaseMuts.bb
mv mitoMapVars.new.bb mitoMapVars.bb
mv mitoMapDiseaseMuts.hg19.new.bb mitoMapDiseaseMuts.hg19.bb
mv mitoMapVars.hg19.new.bb mitoMapVars.hg19.bb

mv mutationsCodingControl.latest.tsv mutationsCodingControl.tsv
mv mutationsRNA.latest.tsv mutationsRNA.tsv
mv variantsCoding.latest.tsv variantsCoding.tsv
mv variantsControl.latest.tsv variantsControl.tsv

# Fetch MitoMap's own release date. Not fatal: the new tracks are already in place,
# so a failure here just leaves the previous date showing in hgTrackUi.
if wget -q $mitoMapUrl/update-date.txt -O version.new.txt && [ -s version.new.txt ]; then
  mv version.new.txt version.txt
else
  echo "Warning: could not fetch $mitoMapUrl/update-date.txt, keeping $(cat version.txt)"
  rm -f version.new.txt
fi

echo
echo "Item counts for disease mutation old vs. new bigBed. Old: $oldCountDiseaseMuts New: $newCountDiseaseMuts"
echo "Item counts for variants old vs. new bigBed. Old: $oldCountVars New: $newCountVars"
echo
echo "MitoMap tracks built successfully."

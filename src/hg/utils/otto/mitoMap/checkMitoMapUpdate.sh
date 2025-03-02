#! /bin/bash

cd /hive/data/outside/otto/mitoMap

wget -q https://mitomap.org/downloads/VariantsControl.tsv -O variantsControl.latest.tsv
wget -q https://mitomap.org/downloads/VariantsCoding.tsv -O variantsCoding.latest.tsv
wget -q https://mitomap.org/downloads/MutationsRNA.tsv -O mutationsRNA.latest.tsv
wget -q https://mitomap.org/downloads/MutationsCodingControl.tsv -O mutationsCodingControl.latest.tsv

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

wget -q https://mitomap.org/update-date.txt -O version.txt

echo
echo "Item counts for disease mutation old vs. new bigBed. Old: $oldCountDiseaseMuts New: $newCountDiseaseMuts"
echo "Item counts for variants old vs. new bigBed. Old: $oldCountVars New: $newCountVars"
echo
echo "MitoMap tracks built successfully."

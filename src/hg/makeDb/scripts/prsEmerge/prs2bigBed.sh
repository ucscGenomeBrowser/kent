# written by Zia Truong and slightly adapted by Max and committed
echo "Converting PRS files to BED files..."
mkdir output
wait

python3 ~/kent/src/hg/makeDb/scripts/prsEmerge/prs2bed.py # iterates over files in data/, writes to output/

echo "Converting BED files to bigBED files..."
#looping through files for loop code based on https://stackoverflow.com/a/10523501
for bed in ./output/*.bed
do
  sort -k1,1 -k2,2n "${bed}" -o "${bed}"
  bedToBigBed -as=prs.as -type=bed9+2 "${bed}" /hive/data/genomes/hg19/chrom.sizes "${bed}.bb"
  rm ${bed}
done

echo "Done!"

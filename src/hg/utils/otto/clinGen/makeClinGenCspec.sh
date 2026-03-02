#! /bin/bash

export LANG=en_US.UTF-8
export LC_ALL=en_US.UTF-8

cd /hive/data/outside/otto/clinGen/clinGenCspec

# Check to see if svis.json is available for download and quit if it fails
# after 4 attempts
for i in 1 2 3; do
    wget -q -O svis.json https://cspec.genome.network/cspec/api/svis && break
    if [ $i -lt 3 ]; then
        echo "Warning: Failed to download SVIs data.\n"
        echo "Retrying in 5 minutes (attempt $i of 3)..."
        sleep 300
    fi
done
if [ $i -eq 3 ]; then
    echo "Error: Failed to download the SVIs data. The ClinGen CSpec API appears to be down.\n"
    echo "Please try again later or check https://cspec.genome.network for service status."
    exit 1
fi

# Check if the files are the same
if cmp -s svis.json svis.json.old; then
  # Files are the same, exit silently
  rm svis.json
  exit 0
else
  # Files are different, continue with the script or add actions
  echo "Updating ClinGen VCEP track..."
fi

# Check to see if mondo.json is available, and quit if it fails
# after 3 attempts
for i in 1 2 3; do
    wget -q http://purl.obolibrary.org/obo/mondo.json && break
    if [ $i -lt 3 ]; then
        echo "Warning: Failed to download MONDO disease ontology.\n"
        echo "Retrying in 5 minutes (attempt $i of 3)..."
        sleep 300
    fi
done
if [ $i -eq 3 ]; then
    echo "Error: Failed to download the MONDO disease ontology.\n"
    echo "The OBO Foundry API appears to be down.\n"
    echo "Please try again later or check https://obofoundry.org for service status."
    exit 1
fi

# Try to get the geneToDisease.csv file, and quit if it fails
# after 3 attempts
for i in 1 2 3; do
    wget -q -O geneToDisease.csv https://search.clinicalgenome.org/kb/gene-validity/download && break
    if [ $i -lt 3 ]; then
        echo "Warning: Failed to download gene-disease data.\n"
        echo "Retrying in 5 minutes (attempt $i of 3)..."
        sleep 300
    fi
done
if [ $i -eq 3 ]; then
    echo "Error: Failed to download gene-disease data. The ClinGen API appears to be down.\n"
    echo "Please try again later or check https://clinicalgenome.org for service status."
    exit 1
fi

bigBedToBed /gbdb/hg38/hgnc/hgnc.bb hgnc.bed

python3 - << END | sort -k1,1 -k2,2n > cspec.bed
import json
import re
import sys

# create a dict that matches MONDO ID with disease
mondoDict = dict()
jsonData = json.load(open("mondo.json"))
mondo = jsonData["graphs"]
nodes = mondo[0]['nodes']
for item in nodes:
    if item['id'].startswith('http'):
        mondoID = item['id'].split('/')[-1]
        if mondoID.startswith('MONDO_'):
            lbl = 'not specified'
            if 'lbl' in item:
                lbl = item['lbl']
            mondoDict[mondoID] = lbl
        
jsonData = json.load(open("svis.json"))
data = jsonData["data"]

# some genes occur more than once. In those cases only the @id and ruleset seems to differ
# e.g. for ACTA1:
# https://cspec.genome.network/cspec/api/SequenceVariantInterpretation/id/GN147
# https://cspec.genome.network/cspec/api/SequenceVariantInterpretation/id/GN169
# since the relevant stuff is the same we can just keep that.

mane = dict()
with open('hgnc.bed', 'r') as bed:
    for line in bed:
        fields = line.split('\t')
        fields[3] = fields[9]   # replace name
        mane[fields[9]] = ('\t').join(fields[:8]) # remove color

colors_dict = {
    "Classification Rules In Prep": "128,0,128", # Dark Purple
    "Classification Rules Submitted": "0,0,139", # Dark blue
    "Pilot Rules In Prep": "0,100,0",            # Dark Green
    "Pilot Rules Submitted": "139,0,0",          # Dark Red
    "Released": "0,0,0"                          # Black
}

# disease can be looked up by MONDO id
# the csv is poorly formatted so don't use csv module
mondo = dict()
with open('geneToDisease.csv', 'r') as inf:
    for line in inf:
        fields = line.split(',')
        mondoID = fields[3].strip('"')
        disease = fields[2].strip('"')
        mondo[mondoID] = disease

seen = [] 

for panel in data:
    for rset in panel["ruleSets"]:
        if 'genes' in rset:
            for gene in rset['genes']:
                name = gene['label']
                if name in seen:
                    continue
                seen.append(name)
                if not name in mane:
                    print('WARNING, cannot find', name, file=sys.stderr)
                    continue
                disease = 'no MONDO ID specified'
                if "diseases" in gene:
                    try:
                        mondoID = gene["diseases"][0]["label"].replace(':', '_')
                        disease = f'{mondoID}, {mondoDict[mondoID]}'
                    except:
                        mondoID = "No MONDO ID"
                        disease = f'{mondoID}'
                url = panel['url']
                diseaseURL = f'<a target="_blank" href="{url}">{disease}</a>'
                affiliationURL = panel["affiliation"]["url"]
                affURL = f'<a target="_blank" href="{affiliationURL}">{panel["affiliation"]["label"]}</a>'
                status = panel['status']
                color = '0'
                print(f'{mane[name]}\t{color}\t{diseaseURL}\t{affURL}\t{status}')
END

cat << '_EOF_' > clinGenCspec.as
table clinGenCspec
"Cspecs for Clingen genes"
    (
    string chrom;      "Reference sequence chromosome or scaffold"
    uint   chromStart; "Start position in chromosome"
    uint   chromEnd;   "End position in chromosome"
    string name;       "Short Name of item"
    uint   score;      "Score from 0-1000"
    char[1] strand;    "+ or -"
    uint thickStart;   "Start of where display should be thick (start codon)"
    uint thickEnd;     "End of where display should be thick (stop codon)"
    uint reserved;     "Used as itemRgb as of 2004-11-22"
    lstring disease;   "Disease"
    lstring panel;     "CSPEC panel"
    lstring status;    "Status"
    )
_EOF_

bedToBigBed -type=bed9+2 -tab -as=clinGenCspec.as cspec.bed /hive/data/genomes/hg38/chrom.sizes clinGenCspecHg38.new.bb
rm cspec.bed

bigBedToBed /gbdb/hg19/hgnc/hgnc.bb hgnc.bed

python3 - << END | sort -k1,1 -k2,2n > cspec.bed
import json
import re
import sys

# create a dict that matches MONDO ID with disease
mondoDict = dict()
jsonData = json.load(open("mondo.json"))
mondo = jsonData["graphs"]
nodes = mondo[0]['nodes']
for item in nodes:
    if item['id'].startswith('http'):
        mondoID = item['id'].split('/')[-1]
        if mondoID.startswith('MONDO_'):
            lbl = 'not specified'
            if 'lbl' in item:
                lbl = item['lbl']
            mondoDict[mondoID] = lbl
        
jsonData = json.load(open("svis.json"))
data = jsonData["data"]

# some genes occur more than once. In those cases only the @id and ruleset seems to differ
# e.g. for ACTA1:
# https://cspec.genome.network/cspec/api/SequenceVariantInterpretation/id/GN147
# https://cspec.genome.network/cspec/api/SequenceVariantInterpretation/id/GN169
# since the relevant stuff is the same we can just keep that.

mane = dict()
with open('hgnc.bed', 'r') as bed:
    for line in bed:
        fields = line.split('\t')
        fields[3] = fields[9]   # replace name
        mane[fields[9]] = ('\t').join(fields[:8]) # remove color

colors_dict = {
    "Classification Rules In Prep": "128,0,128", # Dark Purple
    "Classification Rules Submitted": "0,0,139", # Dark blue
    "Pilot Rules In Prep": "0,100,0",            # Dark Green
    "Pilot Rules Submitted": "139,0,0",          # Dark Red
    "Released": "0,0,0"                          # Black
}

# disease can be looked up by MONDO id
# the csv is poorly formatted so don't use csv module
mondo = dict()
with open('geneToDisease.csv', 'r') as inf:
    for line in inf:
        fields = line.split(',')
        mondoID = fields[3].strip('"')
        disease = fields[2].strip('"')
        mondo[mondoID] = disease

seen = [] 

for panel in data:
    for rset in panel["ruleSets"]:
        if 'genes' in rset:
            for gene in rset['genes']:
                name = gene['label']
                if name in seen:
                    continue
                seen.append(name)
                if not name in mane:
                    print('WARNING, cannot find', name, file=sys.stderr)
                    continue
                disease = 'no MONDO ID specified'
                if "diseases" in gene:
                    try:
                        mondoID = gene["diseases"][0]["label"].replace(':', '_')
                        disease = f'{mondoID}, {mondoDict[mondoID]}'
                    except:
                        mondoID = "No MONDO ID"
                        disease = f'{mondoID}'
                url = panel['url']
                diseaseURL = f'<a target="_blank" href="{url}">{disease}</a>'
                affiliationURL = panel["affiliation"]["url"]
                affURL = f'<a target="_blank" href="{affiliationURL}">{panel["affiliation"]["label"]}</a>'
                status = panel['status']
                color = '0'
                print(f'{mane[name]}\t{color}\t{diseaseURL}\t{affURL}\t{status}')
END

bedToBigBed -type=bed9+2 -tab -as=clinGenCspec.as cspec.bed /hive/data/genomes/hg19/chrom.sizes clinGenCspecHg19.new.bb

rm hgnc.bed cspec.bed mondo.json geneToDisease.csv

oldCountHg38=$(bigBedInfo clinGenCspecHg38.bb | grep -i "itemCount" | awk '{print $NF}')
oldCountHg19=$(bigBedInfo clinGenCspecHg19.bb | grep -i "itemCount" | awk '{print $NF}')

newCountHg38=$(bigBedInfo clinGenCspecHg38.new.bb | grep -i "itemCount" | awk '{print $NF}')
newCountHg19=$(bigBedInfo clinGenCspecHg19.new.bb | grep -i "itemCount" | awk '{print $NF}')

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
mv clinGenCspecHg38.new.bb clinGenCspecHg38.bb
mv clinGenCspecHg19.new.bb clinGenCspecHg19.bb
mv svis.json svis.json.old

echo
echo "Item counts for hg38 old vs. new bigBed. Old: $oldCountHg38 New: $newCountHg38"
echo "Item counts for hg19 old vs. new bigBed. Old: $oldCountHg19 New: $newCountHg19"
echo
echo "ClinGen VCEP specifications track built successfully."

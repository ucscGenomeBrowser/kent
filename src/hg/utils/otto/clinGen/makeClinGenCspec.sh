#! /bin/bash

cd /hive/data/outside/otto/clinGen/clinGenCspec

wget -q -O svis.json https://cspec.genome.network/cspec/api/svis
wget -q http://purl.obolibrary.org/obo/mondo.json
wget -q -O geneToDisease.csv https://search.clinicalgenome.org/kb/gene-validity/download
bigBedToBed /gbdb/hg38/hgnc/hgnc.bb hgnc.bed

oldCountHg38=$(bigBedInfo clinGenCspecHg38.bb | grep -i "itemCount")
oldCountHg19=$(bigBedInfo clinGenCspecHg19.bb | grep -i "itemCount")

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

bedToBigBed -type=bed9+2 -tab -as=clinGenCspec.as cspec.bed /hive/data/genomes/hg38/chrom.sizes clinGenCspecHg38.bb
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

bedToBigBed -type=bed9+2 -tab -as=clinGenCspec.as cspec.bed /hive/data/genomes/hg19/chrom.sizes clinGenCspecHg19.bb

rm hgnc.bed cspec.bed mondo.json geneToDisease.csv svis.json 

newCountHg38=$(bigBedInfo clinGenCspecHg38.bb | grep -i "itemCount")
newCountHg19=$(bigBedInfo clinGenCspecHg19.bb | grep -i "itemCount")

echo
echo Item counts for hg38 old vs. new bigBed. Old: $oldCountHg38 New: $newCountHg38
echo Item counts for hg19 old vs. new bigBed. Old: $oldCountHg19 New: $newCountHg19
echo
echo ClinGen VCEP specifications track built successfully.

from os.path import *
import time, string, os
from wikitools import wiki, category, page
from StringIO import StringIO
import csv

# create the snpediaAll.bed files

def reformatBed(bedFname, allInfo, outFname):

    ofh = open(outFname, "w")

    for line in open(bedFname):
        row = line.rstrip("\n").split("\t")
        row = row[:4]

        name = row[3]
        if name not in allInfo:
            print "missing: %s" % name
            continue
        gffInfo = allInfo[name]

        row.extend(gffInfo)
        ofh.write("\t".join(row))
        ofh.write("\n")

allInfo = {}

i = 0
for line in open("SNPedia.gff"):
    i+=1
    # chr5    snpedia snp     148826785       148826785       .       .       .       Name=rs:Rs1042711;
    #chr17	snpedia	snp	39723335	39723335	.	.	.	Name=rs:Rs1136201;ID=rs:Rs1136201;Url="http://www.snpedia.com/index.php?title=Rs:Rs1136201";Note="rs1136201 (Ile655Val) is a SNP within ERBB2/HER2 (Human epidermal growth factor receptor 2). (among ..."
    #chr19	snpedia	illumina	7668969	7668969	.	.	.	Name=rs:Rs3219175;ID=rs:Rs3219175;Url="http://www.snpedia.com/index.php?title=Rs:Rs3219175";Note=""
    if line.startswith("#") or line=="\n":
        continue

    row = line.rstrip("\n").split("\t")
    arrayType = row[2]
    annotParts = string.split(row[8], ";", maxsplit=3)

    annots = {}
    for ap in annotParts:
        k, v = string.split(ap, "=", maxsplit=1)
        v = v.strip('"')
        annots[k] = v

    snpId = annots["ID"].replace("rs:","").lower()
    url = annots["Url"]
    urlId = url.split(":")[-1].lower()

    assert(urlId==snpId.lower())
    data = (arrayType, annots["Note"])
    if snpId in allInfo:
        print "dupl", snpId, data, allInfo[snpId]
        continue
    allInfo[snpId]= data

ofh = open("build/allIds.txt", "w")
ofh.write("\n".join(allInfo))
ofh.close()

#cmd = 'zcat /hive/data/outside/dbSNP/150/human_hg38/snp150.bed.gz | grep -Fwf build/allIds.txt > build/hg38/allIdRows.txt'
#print "running %s" % cmd
#assert(os.system(cmd)==0)

#cmd = 'zcat /hive/data/outside/dbSNP/150/human_hg19/snp150.bed.gz | grep -Fwf build/allIds.txt > build/hg19/allIdRows.txt'
#print "running %s" % cmd
#assert(os.system(cmd)==0)

reformatBed("build/hg38/allIdRows.txt", allInfo, "build/hg38/snpAll.bed")
reformatBed("build/hg19/allIdRows.txt", allInfo, "build/hg19/snpAll.bed")

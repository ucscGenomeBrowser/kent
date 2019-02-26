# convert mastermind .csv file to .bed with extra fields
# arguments: $1 = mastermind csv file
# output goes to mastermind.bed
import re, sys, csv

csv.field_size_limit(sys.maxsize)

ncbiToUcsc = {}

for line in open("chromAlias.tab"):
    row = line.rstrip("\n").split("\t")
    ncbiToUcsc[row[0]] = row[1]

ofh = open("mastermind.bed", "w")

prefixRe = re.compile(r'NC_[0-9]*.[0-9]*:g.[0-9_]*')

fname = sys.argv[1]
reader = csv.reader(open(fname))

for row in reader:
    #CHROM,POS,ID,REF,ALT,QUAL,FILTER,GENE,HGVSG,MMCNT1,MMCNT2,MMCNT3,MMID3,MMURI3
    if row[0].startswith("CHROM"):
        continue
    ncbiChrom, pos, _, ref, alt, _, _, gene, hgvsg, mmcnt1, mmcnt2, mmcnt3, mmid3, mmuri3 = row
    mmid3s = mmid3.split(",")
    mmuri3s = mmuri3.split(",")

    chrom = ncbiToUcsc[ncbiChrom]
    start = int(pos)-1
    end = start+len(ref)
    name = prefixRe.sub("", hgvsg)
    if len(name)>17:
        name = name[:14]+"..."
    if len(mmid3s)==1:
        mouseOver = mmid3+" - %s/%s/%s" % (mmcnt1, mmcnt2, mmcnt3)
    else:
        mouseOver = mmid3+" on %d transcripts - %s/%s/%s" % (len(mmid3s), mmcnt1, mmcnt2, mmcnt3)

    score = "0"
    strand = "."
    # values copied from knownGene.html
    itemRgb = "130,130,210" # light blue by default: mmcnt3==1
    if int(mmcnt3)>1: # medium-blue if mmcnt3 >=2
        itemRgb = "50,80,160"
    if int(mmcnt1)!=0: # dark-blue if exact cDNA match
        itemRgb = "12,12,120"

    urlSuffix = mmuri3s[0].replace("https://mastermind.genomenon.com/detail?disease=all%20diseases&", "")
    assert(urlSuffix!=mmuri3)

    url = urlSuffix+"|"+mmid3
    outRow = [chrom, str(start), str(end), name, score, strand, str(start), str(end), itemRgb, url, gene, mmcnt1, mmcnt2, mmcnt3, mouseOver]
    ofh.write("\t".join(outRow))
    ofh.write("\n")

print("Wrote %s" % ofh.name)





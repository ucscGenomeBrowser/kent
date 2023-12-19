# The original source code of these files is in ~/kent/src/utils/otto/clinvar
# Don't modify the files in /hive/outside/otto/mastermind! modify the originals in the code repo.
# then run "make" in kent/src/utils/otto/mastermind

# convert mastermind .csv file to .bed with extra fields
# arguments: $1 = mastermind csv file
# output goes to mastermind.bed
import re, sys, csv

csv.field_size_limit(sys.maxsize)

aliasFname, inFname, outFname = sys.argv[1:4]

ncbiToUcsc = {}
for line in open(aliasFname):
    row = line.rstrip("\n").split("\t")
    ncbiToUcsc[row[0]] = row[1]

ofh = open(outFname, "w")

prefixRe = re.compile(r'NC_[0-9]*.[0-9]*:g.[0-9_]*')

reader = csv.reader(open(inFname))

for row in reader:
    #CHROM,POS,ID,REF,ALT,QUAL,FILTER,GENE,HGVSG,MMCNT1,MMCNT2,MMCNT3,MMID3,MMURI3
    row = row[:14]
    if row[0].startswith("CHROM"):
        continue
    ncbiChrom, pos, _, ref, alt, _, _, gene, hgvsg, mmcnt1, mmcnt2, mmcnt3, mmid3, mmuri3 = row

    #received this from schwartz@genomenon.com, June 22 2019
    #That'd be great! I actually do have this particular thing implemented in one of our internal scripts. If it helps, this is one of our Python scripts we threw together to allow us to quickly check our CVR VCF file for various things from the command line:
    #https://gist.github.com/JangoSteve/34b383f82e5145c05eedd96a189f07a7
    #If I want to output just MNVs from the file, I can run `python read_cvr.py mnps path/to/cvr.vcf`, which corresponds to these lines:
    #https://gist.github.com/JangoSteve/34b383f82e5145c05eedd96a189f07a7#file-read_cvr-py-L54-L57
    #If I want just the MNVs with nucleotide-specific citation counts, I can run `python read_cvr.py mnps-with-citations path/to/cvr.vcf`, which is these lines:
    #https://gist.github.com/JangoSteve/34b383f82e5145c05eedd96a189f07a7#file-read_cvr-py-L64-L68
    #So, from the above, if your import script were in Python, you could basically have a line that skips importing a given line based on this criteria (or negate it for inclusion):
    #if len(ref) > 1 and len(ref) == len(alt) and mmcnt1 == 0:
    #  # skip
    #However, there is one big caveat here. We've discovered several variants where you can only get that particular protein substitution by a MNV (i.e. there is no SNV which would result in the change of the ref amino acid to the alt). In those cases, the MNV should probably be included even if MMCNT1=0, since just a protein description is pretty conclusive about the MNV. I'll check with the team to see if we have code that does that determination already. The only two ways I can think of to determine this would be to either check to see if that protein variant exists anywhere else in the CVR, or to essentially incorporate the codon chart into the import code and do a lookup.

    if len(ref) > 1 and len(ref) == len(alt) and mmcnt1 == '0':
        continue

    mmid3s = mmid3.split(",")
    mmuri3s = mmuri3.split(",")
    # in hg38, hgvsg has two parts, the first are the hg19 coords, the second are the hg38 coords
    hgvsg = hgvsg.split(",")[0]

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
    urlSuffix = urlSuffix.replace("ref=cvr", "ref=ucsc")
    assert(urlSuffix!=mmuri3)

    mmid3 = mmid3.replace(",", "&#44;")
    url = urlSuffix+"|"+mmid3
    outRow = [chrom, str(start), str(end), name, score, strand, str(start), str(end), itemRgb, url, gene, mmcnt1, mmcnt2, mmcnt3, mouseOver]
    ofh.write("\t".join(outRow))
    ofh.write("\n")

print("Wrote %s" % ofh.name)





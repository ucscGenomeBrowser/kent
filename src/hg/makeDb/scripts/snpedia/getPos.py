# the snpedia snp positions are wrong, some weird mix of hg19 and hg38
# let's use our tables instead
import os
doSnps = open("build/goodPages.txt").read().splitlines()
doSnps = set([x.split("/")[-1].split(".")[0] for x in doSnps])

ofh19 = open("build/hg38/snpPos.bed", "w")
ofh38 = open("build/hg19/snpPos.bed", "w")

for line in open("SNPedia.gff"):
   #print snp
   cmd = """hgsql hg19 -NB -e 'select chrom, chromStart, chromEnd, name from snp150 where name="%s"'""" % snpId
   line = os.popen(cmd).read()
   #print line
   if(len(line)<10):
       print "%s not found in hg19" % snpId
   ofh19.write(line)

   cmd = """hgsql hg38 -NB -e 'select chrom, chromStart, chromEnd, name from snp150 where name="%s"'""" % snp
   line = os.popen(cmd).read()
   #print line
   #assert(len(line)>10)
   if(len(line)<10):
       print "%s not found in hg38" % snp
   ofh38.write(line)


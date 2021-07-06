import sys
import subprocess
## CADD GRCh38-v1.6 (c) University of Washington, Hudson-Alpha Institute for Biotechnology and Berlin Institute of Health 2013-2020. All rights reserved.
#Chrom  Pos     Ref     Alt     RawScore        PHRED
#1       10061   T       TAACCCTAACCCTAACCCTAACCCTAACCCTAACCCTAACCCTAACCC        0.199964        3.123
#1       10067   T       TAACCCTAACCCTAACCCTAACCCTAACCCTAACCCTAACCC      0.225348        3.416
#1       10108   C       CA      0.390514        5.321

insFh = open("ins.bed", "w")
delFh = open("del.bed", "w")

inFname = sys.argv[1]
for line in subprocess.Popen(['zcat', inFname], stdout=subprocess.PIPE, encoding="ascii").stdout:
    if line.startswith("#"):
        continue
    row = line.rstrip("\n").split("\t")
    chrom, pos, ref, alt, raw, phred = row
    chrom = "chr"+chrom
    pos = int(row[1])-1
    phred = float(phred)

    if len(ref) < len(alt):
        # insertion
        insLen = len(alt)-1
        name = str(insLen)
        desc = "ins."+alt[1:]
        ofh = insFh
        start = str(pos+1)
        end = str(pos+1)
    else:
        delLen = len(ref)-1
        name = str(delLen)
        desc = "del."+ref[1:]
        ofh = delFh
        start = str(pos+1)
        end = str(pos+1+delLen)

    bed = [chrom, start, end, name, str(round(phred)), ".", start, end, "0", desc, str(phred)]
    ofh.write("\t".join(bed))
    ofh.write("\n")

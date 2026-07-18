#!/usr/bin/env python3
# For each dbSNP common deletion, nearest HPRC canonical deletion of the SAME length
# (min |startDiff|), plus a length-agnostic nearest for comparison. Report cumulative
# recovery within windows. Redmine #35415
import sys, bisect
D="/hive/data/genomes/hg38/bed/hprc2X"
hprcFile = sys.argv[1] if len(sys.argv)>1 else D+"/hprc2X.canon.tsv"

# HPRC canonical: key chrom:start:len  count
byCL = {}          # (chrom,len) -> sorted start list  (same-length index)
byC  = {}          # chrom -> sorted start list        (any-length index)
with open(hprcFile) as f:
    for line in f:
        k=line.split("\t",1)[0]; c,s,l=k.split(":"); s=int(s); l=int(l)
        byCL.setdefault((c,l),[]).append(s)
        byC.setdefault(c,[]).append(s)
for d in (byCL,byC):
    for kk in d: d[kk].sort()

def nearest(lst, x):
    if not lst: return None
    i=bisect.bisect_left(lst,x); best=1<<60
    if i<len(lst): best=min(best,abs(lst[i]-x))
    if i>0: best=min(best,abs(lst[i-1]-x))
    return best

wins=[0,2,20,50,200]
sameCum={w:0 for w in wins}; anyCum={w:0 for w in wins}
tot=0
with open(D+"/dbSnp.canon.keys") as f:
    for line in f:
        c,s,l=line.strip().split(":"); s=int(s); l=int(l); tot+=1
        dS=nearest(byCL.get((c,l)),s)
        dA=nearest(byC.get(c),s)
        for w in wins:
            if dS is not None and dS<=w: sameCum[w]+=1
            if dA is not None and dA<=w: anyCum[w]+=1

print("dbSNP common deletions: %d\n"%tot)
print("window(bp)  same-length recovery      any-length recovery")
for w in wins:
    print("  <=%-6d  %8d (%5.1f%%)        %8d (%5.1f%%)"%(
        w, sameCum[w],100*sameCum[w]/tot, anyCum[w],100*anyCum[w]/tot))

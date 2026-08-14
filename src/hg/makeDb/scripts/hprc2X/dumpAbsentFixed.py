#!/usr/bin/env python3
# Dump rel1 fixed (recurrence==88) deletions split by whether they are present in rel2 full
# (any-length within +/-20bp). Writes two BEDs: absent (the ~90) and present (control).
import bisect
D="/hive/data/genomes/hg38/bed/hprc2X"
byC={}
with open(D+"/hprc2X.canon.tsv") as f:
    for line in f:
        k=line.split("\t",1)[0]; c,s,l=k.split(":"); byC.setdefault(c,[]).append(int(s))
for c in byC: byC[c].sort()
def near(lst,x,w=20):
    if not lst: return False
    i=bisect.bisect_left(lst,x)
    return (i<len(lst) and abs(lst[i]-x)<=w) or (i>0 and abs(lst[i-1]-x)<=w)
ab=open(D+"/fixedAbsent.bed","w"); pr=open(D+"/fixedPresent.bed","w")
na=npr=0
for line in open(D+"/hprc1.canon.tsv"):
    k,r=line.rstrip("\n").split("\t")
    if int(r)!=88: continue
    c,s,l=k.split(":"); s=int(s); e=s+int(l)
    if near(byC.get(c),s):
        pr.write("%s\t%d\t%d\t%s\n"%(c,s,e,k)); npr+=1
    else:
        ab.write("%s\t%d\t%d\t%s\n"%(c,s,e,k)); na+=1
ab.close(); pr.close()
print("fixed rel1 events: absent-from-rel2=%d  present=%d"%(na,npr))

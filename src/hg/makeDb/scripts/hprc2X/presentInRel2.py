#!/usr/bin/env python3
# For rel1 canonical deletions, is the event present in rel2 full, under three match rules:
#   exact key | same-length within +/-20bp | any-length within +/-20bp
# tabulated by rel1 recurrence cutoff. Redmine #35415
import bisect
D="/hive/data/genomes/hg38/bed/hprc2X"
byCL={}; byC={}; exact=set()
with open(D+"/hprc2X.canon.tsv") as f:
    for line in f:
        k=line.split("\t",1)[0]; exact.add(k); c,s,l=k.split(":"); s=int(s); l=int(l)
        byCL.setdefault((c,l),[]).append(s); byC.setdefault(c,[]).append(s)
for d in (byCL,byC):
    for kk in d: d[kk].sort()
def near(lst,x,w):
    if not lst: return False
    i=bisect.bisect_left(lst,x)
    if i<len(lst) and abs(lst[i]-x)<=w: return True
    if i>0 and abs(lst[i-1]-x)<=w: return True
    return False
# rel1 events: (recurrence, exactHit, sameLenHit, anyLenHit)
rows=[]
for line in open(D+"/hprc1.canon.tsv"):
    k,r=line.rstrip("\n").split("\t"); r=int(r); c,s,l=k.split(":"); s=int(s); l=int(l)
    eh = k in exact
    sh = eh or near(byCL.get((c,l)),s,20)
    ah = sh or near(byC.get(c),s,20)
    rows.append((r,eh,sh,ah))
maxr=88
print("rel1>=k  freq   events    exact   sameLen+/-20  anyLen+/-20   trueMissing(anyLen)")
cumT=cumE=cumS=cumA=0
buckets={88,44,20,10,6,3,2,1}
for k in range(maxr,0,-1):
    for (r,eh,sh,ah) in [x for x in rows if x[0]==k]:
        cumT+=1; cumE+=eh; cumS+=sh; cumA+=ah
    if k in buckets:
        print("  >=%-3d  %5.1f%%  %-8d  %.4f   %.4f       %.4f       %d"%(
            k,100*k/88,cumT,cumE/cumT,cumS/cumT,cumA/cumT,cumT-cumA))

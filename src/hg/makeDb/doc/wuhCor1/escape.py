import collections

byPos = collections.defaultdict(list)
for line in open("table2.txt"):
    if line=="\n":
        continue
    row = line.rstrip("\n").split()
    # A1451C E484A 2B04
    nucl, prot, ab = row
    aaPos = prot[1:-1]
    nuclPos = nucl[1:-1]
    fromAa = prot[0]
    toAa = prot[-1]
    start = 21598+(3*int(aaPos))
    end = start+3
    chrom = "NC_045512v2"
    byPos[(chrom, start, end, fromAa, aaPos)].append( [toAa, nucl, prot, ab] )

for (chrom, start, end, fromAa, aaPos), rows in byPos.items():
    toAas = set()
    nucls = []
    prots = []
    mabs = []
    for row in rows:
        toAa, nucl, prot, ab = row
        toAas.add(toAa)
        nucls.append(nucl)
        prots.append(prot)
        mabs.append(ab)

    mouseOver =  "nucleotides: "+", ".join(nucls)+" - amino acids: "+", ".join(prots) + " - antibodies: "+", ".join(mabs)
    name = fromAa+aaPos+"/".join(list(sorted(toAas)))
    bed = [chrom, str(start), str(end), name, "0", ".", str(start), str(end), "0", ", ".join(nucls), ", ".join(prots), ", ".join(mabs), mouseOver]
    print("\t".join(bed))






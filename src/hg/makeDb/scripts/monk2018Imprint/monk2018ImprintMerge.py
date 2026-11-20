#!/usr/bin/env python3
"""Merge the Monk et al 2018 imprinted DMRs (hg38 lift) with the Mackay et al 2024 classification.

Usage: monk2018ImprintMerge.py monk2018.hg38.bed mackay2024.bed out.bed

Inputs are the outputs of monk2018ImprintToBed.py + liftOver (bed9+6) and mackay2024ImprintToBed.py
(bed9+9). The regions are joined on their hg38 coordinates, which are identical in both papers for
the 48 Mackay regions. The two Monk regions that Mackay does not list (SNRPN:Int1-DMR1 and DMR2) are
kept and marked "Not in Mackay 2024". Names are taken from Monk; a different Mackay name is added to
the aliases. The CpG count is Mackay's; where Monk gives another number, both are shown.
Monk spells the secondary DMRs "No-secondary DMR", Mackay "Secondary DMR"; Mackay's term is used.
Colors: strong red/blue for clinically associated regions, light red/blue for all others.
"""
import sys, collections

STRONG = {"Maternal": "220,20,20", "Paternal": "0,60,200"}
LIGHT = {"Maternal": "240,150,150", "Paternal": "150,175,240"}
ORIGIN = {"No-secondary DMR": "Secondary DMR"}

def splitList(s):
    return [x.strip() for x in s.split(",") if x.strip()]

def main():
    monkFn, mackayFn, outFn = sys.argv[1:4]
    mackay = {}
    for l in open(mackayFn):
        f = l.rstrip("\n").split("\t")
        assert len(f) == 18, l
        mackay[tuple(f[:3])] = f
    used = set()
    counts = collections.Counter()
    notes = []
    rows = []
    for l in open(monkFn):
        m = l.rstrip("\n").split("\t")
        assert len(m) == 15, l
        key = tuple(m[:3])
        name, parent, cpgMonk, originMonk, lrg, aliasMonk, hg19Pos = m[3], m[9], m[10], m[11], m[12], m[13], m[14]
        aliases = splitList(aliasMonk)
        k = mackay.get(key)
        if k:
            used.add(key)
            cls, parentK, cpgK, origin, t1type, keyDmr, dis, disNames, other = k[9:18]
            if parentK != parent:
                raise Exception("methylated parent differs for %s: Monk %s, Mackay %s" % (name, parent, parentK))
            if k[3] != name:
                aliases.insert(0, k[3])
                notes.append("name %s in Monk, %s in Mackay" % (name, k[3]))
            for a in splitList(other):
                if a not in aliases and a != name:
                    aliases.append(a)
            cpg = cpgK if cpgK == cpgMonk else "%s (Monk 2018: %s)" % (cpgK, cpgMonk)
            if cpgK != cpgMonk:
                notes.append("CpGs of %s: Mackay %s, Monk %s" % (name, cpgK, cpgMonk))
            sources = "Monk 2018, Mackay 2024"
        else:
            cls, origin, t1type, keyDmr, dis, disNames = "Not in Mackay 2024", ORIGIN.get(originMonk, originMonk), "", "", "", ""
            cpg = cpgMonk
            sources = "Monk 2018"
        counts[cls] += 1
        rgb = (STRONG if cls == "Clinically associated" else LIGHT)[parent]
        rows.append(m[:8] + [rgb, parent, cls, keyDmr, origin, cpg, dis, disNames, ", ".join(aliases),
                             lrg, t1type, sources, hg19Pos])
    missing = set(mackay) - used
    if missing:
        raise Exception("Mackay regions without a Monk region: %s" % [mackay[k][3] for k in missing])
    with open(outFn, "w") as out:
        for r in rows:
            out.write("\t".join(r) + "\n")
    sys.stderr.write("%d regions written, %s\n" % (len(rows), dict(counts)))
    for n in notes:
        sys.stderr.write(n + "\n")

main()

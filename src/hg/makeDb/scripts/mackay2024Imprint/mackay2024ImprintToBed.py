#!/usr/bin/env python3
"""Convert Tables 1 to 3 of Mackay et al 2024 (PMID 39090763, tab-separated copies) to bed9+9, hg38.

The locations in Table 1 are hg38 and their starts are already BED starts: they are identical to the
Monk et al 2018 regions lifted from hg19, which were built with start-1. Table 1 has no column for the
parent whose allele is methylated. Oocyte gDMR means maternal and sperm gDMR paternal; a DMR with only
a secondary origin takes the parent from Table 1 of Monk et al 2018 (same DMR names). The two sources
are cross-checked for the gamete DMRs.

Usage: mackay2024ImprintToBed.py table1.tsv table3.tsv monkTable1.tsv out.bed
"""
import re, sys, collections

STRONG = {"Maternal": "220,20,20", "Paternal": "0,60,200"}
LIGHT = {"Maternal": "240,150,150", "Paternal": "150,175,240"}

# Table 2 of the paper names these disorders; the acronyms of Table 1 are expanded with them.
DISORDER = {
    "BWS": "Beckwith-Wiedemann syndrome (OMIM 130650)",
    "SRS": "Silver-Russell syndrome (OMIM 180860)",
    "TNDM": "Transient neonatal diabetes mellitus type 1 (OMIM 601410)",
    "PHP": "Pseudohypoparathyroidism type 1b (OMIM 603233)",
    "TS14": "Temple syndrome (OMIM 616222)",
    "KOS14": "Kagami-Ogata syndrome (OMIM 608140)",
    "AS": "Angelman syndrome (OMIM 105830)",
    "PWS": "Prader-Willi syndrome (OMIM 176270)",
    "MLID (ZFP57)": "Multi-locus imprinting disturbance, ZFP57 variants",
    "upd(7)mat": "Maternal uniparental disomy of chromosome 7 (Silver-Russell syndrome)",
    "upd(16)mat": "Maternal uniparental disomy of chromosome 16",
    # Table 2 lists Mulchandani-Bhoj-Conlin syndrome at chr20; it is caused by maternal UPD(20)
    "upd(20)mat": "Maternal uniparental disomy of chromosome 20 (Mulchandani-Bhoj-Conlin syndrome, OMIM 617352)",
}
TYPE = {"CA": "Clinically associated", "NC": "Non-clinical",
        "X": "Variable methylation, not interpretable clinically"}
ORIGIN = {"Oocyte_gDMR": "Oocyte gDMR", "Sperm_gDMR": "Sperm gDMR", "secondary_DMR": "Secondary DMR",
          "Oocyte_gDMR-secondary_DMR": "Oocyte gDMR-secondary DMR",
          "Sperm_gDMR-secondary_DMR": "Sperm gDMR-secondary DMR"}
# names in Table 3 that differ from Table 1 after removing punctuation
T3_ALIAS = {"h19igf2igdmr": "h19igf2igdmrtssdmr", "meg8int2dmr": "meg8int2dmr"}

def norm(s):
    return re.sub(r"[^a-z0-9]", "", s.lower())

def readTsv(fn):
    rows = [l.rstrip("\n").split("\t") for l in open(fn)]
    return rows[1:]

def table3Classes(fn):
    cls = {}
    for label, names in [(r[0], r[1]) for r in [l.rstrip("\n").split("\t") for l in open(fn)]]:
        c = "Clinically associated" if label.startswith("Clinically") else "Non-clinical"
        for n in names.split(";"):
            n = n.strip().strip("[]")
            if n:
                cls[norm(n)] = c
    return cls

def monkParents(fn):
    d = {}
    for r in readTsv(fn):
        d[norm(r[0].replace("–", "-"))] = "Maternal" if r[5] == "M" else "Paternal"
    return d

def lookup(d, key):
    if key in d:
        return d[key]
    for k in d:
        if key.startswith(k) or k.startswith(key):
            return d[k]
    return None

def splitDisorders(s):
    toks = []
    for part in re.split(r",\s*(?![^()]*\))|;\s*|/", s):
        part = part.strip().replace("[", "(").replace("]", ")")
        if part:
            toks.append(part)
    return toks

def main():
    t1, t3, monk, outFn = sys.argv[1:5]
    cls3 = table3Classes(t3)
    monkP = monkParents(monk)
    n = 0
    mism = []
    notIn3 = []
    counts = collections.Counter()
    with open(outFn, "w") as out:
        for r in readTsv(t1):
            name, other, dis, loc, cpgs, typ, origin = r
            m = re.match(r"(chr\w+):([\d,]+)–([\d,]+)$", loc)
            chrom, start, end = m.group(1), int(m.group(2).replace(",", "")), int(m.group(3).replace(",", ""))
            key = norm(name)
            tcode = typ.rstrip("*")
            keyDmr = "yes" if typ.endswith("*") else "no"
            fromOrigin = "Maternal" if origin.startswith("Oocyte") else "Paternal" if origin.startswith("Sperm") else None
            fromMonk = lookup(monkP, key)
            if fromOrigin and fromMonk and fromOrigin != fromMonk:
                mism.append((name, fromOrigin, fromMonk))
            parent = fromOrigin or fromMonk
            if parent is None:
                raise Exception("no parent for " + name)
            cls = cls3.get(key)
            if cls is None:
                cls = lookup(cls3, T3_ALIAS.get(key, key))
            if cls is None:
                notIn3.append(name)
                cls = {"CA": "Clinically associated", "NC": "Non-clinical", "X": "Non-clinical"}[tcode]
            counts[cls] += 1
            names = []
            for tok in splitDisorders(dis):
                names.append(DISORDER.get(tok, tok))
            rgb = (STRONG if cls == "Clinically associated" else LIGHT)[parent]
            out.write("\t".join([chrom, str(start), str(end), name.replace("-DMR & TSS-DMR", "-DMR"), "0", ".",
                                 str(start), str(end), rgb, cls, parent, cpgs, ORIGIN[origin], TYPE[tcode],
                                 keyDmr, dis, "; ".join(names), other]) + "\n")
            n += 1
    sys.stderr.write("%d DMRs written, %s\n" % (n, dict(counts)))
    sys.stderr.write("parent from origin differs from Monk: %s\n" % mism)
    sys.stderr.write("not in Table 3, classified from Table 1 type: %s\n" % notIn3)

main()

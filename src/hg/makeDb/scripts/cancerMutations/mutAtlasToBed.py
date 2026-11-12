#!/usr/bin/env python3
"""Convert the Cancer Mutation Atlas atlas.tsv plus the bcftools csq output to
a bed9+ file for the mutAtlas bigBed and mutAtlasLolly bigLolly tracks.

Input:
- atlas.tsv: one row per chrom/pos/ref/alt, 1-based VCF positions, lists of
  OncoTree codes with carrier and tested-patient counts, and the same per study
- csq.tsv: <atlas line number> <BCSQ INFO value>, from bcftools csq + query,
  sorted by line number
- canonicalTx.txt: Ensembl_canonical transcript IDs from the Ensembl GFF3
- oncotree.json: OncoTree tumorTypes API dump, for code -> name, tissue

Output: bed on stdout, filter value lines for trackDb to filterFn
"""
import sys, json, math, re
from collections import Counter

# minimum number of tested patients of a cancer type for the max-prevalence field.
# Without it, a rare type with one carrier out of three tested patients wins.
MINTESTED = 50

# bcftools csq consequence terms, most severe first
SEVERITY = ["stop_gained", "frameshift", "splice_acceptor", "splice_donor", "start_lost",
    "stop_lost", "inframe_insertion", "inframe_deletion", "inframe_altering", "missense",
    "coding_sequence", "feature_elongation", "feature_truncation", "splice_region",
    "stop_retained", "start_retained", "synonymous", "5_prime_utr", "3_prime_utr", "intron",
    "non_coding", "NMD_transcript"]
SEVRANK = {t: i for i, t in enumerate(SEVERITY)}
EFFECTLABELS = {"5_prime_utr": "5 prime UTR", "3_prime_utr": "3 prime UTR",
    "non_coding": "non-coding transcript", "NMD_transcript": "NMD transcript"}

# colors by number of carriers, light gray for singletons
COLORBINS = [(100, "110,0,0"), (20, "200,0,0"), (5, "224,75,0"), (2, "240,150,0"),
             (1, "150,150,150")]

def color(carriers):
    for minVal, col in COLORBINS:
        if carriers >= minVal:
            return col

def termRank(term):
    if term not in SEVRANK:
        sys.stderr.write("unknown consequence term %s\n" % term)
        SEVRANK[term] = len(SEVRANK)
    return SEVRANK[term]

def mainTerm(csqStr):
    " most severe single term of a&b&c "
    return min(csqStr.split("&"), key=termRank)

def shortSeq(s, maxLen=12):
    return s if len(s) <= maxLen else s[:maxLen] + "..."

def protChange(effect, aa):
    """ bcftools aa string '600V>600E', '75F', '127VIL..*>127VIY..*' -> short HGVS-like
    protein change. Returns '' if there is none. """
    if aa == "" or aa == "-":
        return ""
    if ">" not in aa:
        m = re.match(r"(\d+)([A-Z*]+)$", aa)
        if m:  # synonymous
            return "p.%s%s=" % (m.group(2)[0], m.group(1))
        return ""
    refPart, altPart = aa.split(">", 1)
    m1 = re.match(r"(\d+)([A-Z*?]*)$", refPart)
    m2 = re.match(r"(\d+)([A-Z*?]*)$", altPart)
    if not m1 or not m2:
        return "p." + shortSeq(aa, 30)
    pos, refAa, altAa = m1.group(1), m1.group(2), m2.group(2)
    if effect == "frameshift":
        return "p.%s%sfs" % (refAa[:1], pos)
    if len(refAa) == 1 and len(altAa) == 1:
        return "p.%s%s%s" % (refAa, pos, altAa)
    return "p.%s%s>%s" % (shortSeq(refAa, 10), pos, shortSeq(altAa, 10))

def parseCsq(bcsq, canonTx):
    """ pick one consequence: the most severe on an Ensembl canonical transcript,
    otherwise the most severe on any transcript.
    Returns gene, transcript, effect, protein change, other genes (str) """
    if bcsq == ".":
        return "", "", "intergenic", "", ""
    entries = []
    for e in bcsq.split(","):
        f = e.split("|")
        csq = f[0]
        gene = f[1] if len(f) > 1 else ""
        tx = f[2] if len(f) > 2 else ""
        aa = f[5] if len(f) > 5 else ""
        entries.append((termRank(mainTerm(csq)), tx not in canonTx, csq, gene, tx, aa))
    canonEntries = [e for e in entries if not e[1]]
    best = min(canonEntries or entries, key=lambda e: (e[0], e[1]))
    _, _, csq, gene, tx, aa = best
    effect = mainTerm(csq)
    # most severe effect for each other gene
    otherGenes = {}
    for rank, _, c, g, _, _ in entries:
        if g == gene or g == "":
            continue
        if g not in otherGenes or rank < otherGenes[g][0]:
            otherGenes[g] = (rank, mainTerm(c))
    other = ", ".join("%s (%s)" % (g, t.replace("_", " ")) for g, (r, t) in sorted(otherGenes.items()))
    return gene, tx, effect, protChange(effect, aa), other

def trimAlleles(pos, ref, alt):
    """ VCF 1-based pos -> 0-based bed start/end of the minimal representation """
    start = pos - 1
    while len(ref) > 0 and len(alt) > 0 and ref[-1] == alt[-1]:
        ref, alt = ref[:-1], alt[:-1]
    while len(ref) > 0 and len(alt) > 0 and ref[0] == alt[0]:
        ref, alt = ref[1:], alt[1:]
        start += 1
    return start, start + len(ref), ref, alt

def varType(ref, alt):
    if len(ref) == 1 and len(alt) == 1:
        return "SNV"
    if len(ref) == 0:
        return "insertion"
    if len(alt) == 0:
        return "deletion"
    if len(ref) == len(alt):
        return "MNV"
    return "delins"

def nucLabel(ref, alt):
    vt = varType(ref, alt)
    if vt in ("SNV", "MNV"):
        return "%s>%s" % (shortSeq(ref, 6), shortSeq(alt, 6))
    if vt == "insertion":
        return "ins" + (alt if len(alt) <= 6 else "%dbp" % len(alt))
    if vt == "deletion":
        return "del" + (ref if len(ref) <= 6 else "%dbp" % len(ref))
    return "delins%dbp" % len(alt)

def jsonTable(header, rows):
    d = {"r000": header}
    for i, row in enumerate(rows):
        d["r%03d" % (i + 1)] = row
    return json.dumps(d, ensure_ascii=True, separators=(",", ":"))

def pct(a, b):
    return 100.0 * a / b if b else 0.0

def fmtPct(p):
    return "%.2g%%" % p if p < 10 else "%.0f%%" % p

def main():
    if len(sys.argv) != 6:
        sys.exit("usage: mutAtlasToBed.py atlas.tsv csq.tsv canonicalTx.txt oncotree.json filterOut.ra > out.bed")
    atlasFn, csqFn, canonFn, otFn, filterFn = sys.argv[1:]
    canonTx = set(l.strip() for l in open(canonFn))
    ot = {x["code"]: x for x in json.load(open(otFn))}
    chromSizes = dict(l.split()[:2] for l in open("/hive/data/genomes/hg38/chrom.sizes"))

    csqFh = open(csqFn)
    effectCounts, tissueCounts, studyCounts, vtCounts = Counter(), Counter(), Counter(), Counter()
    unknownCodes = Counter()
    out = sys.stdout
    count = 0
    with open(atlasFn) as fh:
        fh.readline()
        for lineNo, line in enumerate(fh, start=1):
            (chrom, pos, ref, alt, codes, typeCarr, typeTested, studies,
                studyCarr, studyTested) = line.rstrip("\n").split("\t")
            csqId, bcsq = csqFh.readline().rstrip("\n").split("\t")
            assert int(csqId) == lineNo, (csqId, lineNo)  # csq output is in atlas order

            start, end, tRef, tAlt = trimAlleles(int(pos), ref, alt)
            if end > int(chromSizes[chrom]):
                sys.exit("line %d: variant beyond chrom end" % lineNo)

            codes = codes.split(",")
            typeCarr = [int(x) for x in typeCarr.split(",")]
            typeTested = [int(x) for x in typeTested.split(",")]
            studies = studies.split(",")
            studyCarr = [int(x) for x in studyCarr.split(",")]
            studyTested = [int(x) for x in studyTested.split(",")]
            assert len(codes) == len(typeCarr) == len(typeTested)
            assert len(studies) == len(studyCarr) == len(studyTested)

            carriers = sum(studyCarr)
            tested = sum(studyTested)
            assert carriers == sum(typeCarr)

            typeRows = []
            tissues = set()
            for code, c, t in zip(codes, typeCarr, typeTested):
                if code in ot:
                    name, tissue = ot[code]["name"], ot[code]["tissue"]
                else:
                    unknownCodes[code] += 1
                    name, tissue = code, "Unknown"
                tissues.add(tissue)
                typeRows.append((c, pct(c, t), code, name, tissue, t))
            typeRows.sort(key=lambda r: (-r[0], -r[1]))

            maxPrev, maxPrevType = "", ""
            okRows = [r for r in typeRows if r[5] >= MINTESTED]
            if okRows:
                best = max(okRows, key=lambda r: (r[1], r[0]))
                maxPrev = "%.3f" % best[1]
                maxPrevType = "%s (%s), %d of %d" % (best[3], best[2], best[0], best[5])

            topTypes = "<br>".join("%s (%s): %d of %d (%s)" % (r[3], r[2], r[0], r[5], fmtPct(r[1]))
                                   for r in typeRows[:3])
            if len(typeRows) > 3:
                topTypes += "<br>and %d more cancer types" % (len(typeRows) - 3)

            typeJson = jsonTable(["Cancer type", "OncoTree code", "Tissue", "Carriers",
                                  "Tested patients", "Prevalence"],
                [[r[3], r[2], r[4], str(r[0]), str(r[5]), fmtPct(r[1])] for r in typeRows])
            studyRows = sorted(zip(studies, studyCarr, studyTested), key=lambda r: -r[1])
            studyJson = jsonTable(["Study", "Carriers", "Tested patients", "Prevalence"],
                [[s, str(c), str(t), fmtPct(pct(c, t))] for s, c, t in studyRows])

            gene, tx, effect, prot, otherGenes = parseCsq(bcsq, canonTx)
            vt = varType(tRef, tAlt)
            if prot:
                label = "%s %s" % (gene, prot[2:])
            else:
                label = nucLabel(tRef, tAlt)
            effectLabel = EFFECTLABELS.get(effect, effect.replace("_", " "))

            effectCounts[effectLabel] += 1
            vtCounts[vt] += 1
            for t in tissues:
                tissueCounts[t] += 1
            for s in studies:
                studyCounts[s] += 1

            row = [chrom, str(start), str(end), label, "0", ".", str(start), str(end), color(carriers),
                ref, alt, vt, gene, tx, effectLabel, prot, otherGenes,
                str(carriers), str(tested), "%.4f" % pct(carriers, tested),
                maxPrev, maxPrevType, str(len(codes)), topTypes,
                ",".join(sorted(tissues)), ",".join(studies),
                "%.3f" % math.log10(carriers),
                ",".join(codes), ",".join(map(str, typeCarr)), ",".join(map(str, typeTested)),
                ",".join(map(str, studyCarr)), ",".join(map(str, studyTested)),
                typeJson, studyJson]
            out.write("\t".join(row) + "\n")
            count += 1

    with open(filterFn, "w") as ofh:
        for field, cnt in (("varType", vtCounts), ("effect", effectCounts),
                           ("tissues", tissueCounts), ("studies", studyCounts)):
            ofh.write("filterValues.%s %s\n" % (field, ",".join(k for k, v in cnt.most_common())))
    sys.stderr.write("%d variants written\n" % count)
    for name, cnt in (("variant types", vtCounts), ("effects", effectCounts)):
        sys.stderr.write("%s: %s\n" % (name, dict(cnt.most_common())))
    if unknownCodes:
        sys.stderr.write("OncoTree codes not found: %s\n" % dict(unknownCodes))

main()

#!/usr/bin/env python3
"""Convert the MirGeneDB annotation of one species to the mirGeneDb track files.

Two steps, with RNAfold in between (see mirGeneDbBuild.sh):

  mirGeneDbToBed.py priFa hsa-all.bed hg38.2bit pri.fa
      Write the pri-miRNA sequences, taken from the genome at the MirGeneDB
      coordinates, as FASTA for RNAfold.

  mirGeneDbToBed.py bed hsa-all.bed hsa.gff hsa.browse.html pri.fold hg38.2bit \
          mirGeneDb.bed mirGeneDbArms.bed
      Write the pri-miRNA track (bed6+, see mirGeneDb.as) and the arm decorators
      (bed12+4, kent/src/hg/lib/decoration.as).
      Pri-miRNAs whose precursor part is not one hairpin in the MFE structure are
      refolded with the flanks unpaired (see hairpinRefold), with RNAfold and RNAsubopt.

MirGeneDB 3.0 has -v1, -v2 and -v3 loci for some genes, alternative processing
variants of the same hairpin. The browse table lists such a gene once, under its
name without the variant and marked "V"; all variants get miRBase ID, family and
accessions from that row. The GFF only lists the -v1 locus, so the other variants
get the MI accession of their -v1. The seed is read from the genome at each locus'
own seed feature, and compared with the browse table for -v1 and single loci.
"""
import re
import subprocess
import sys
from collections import defaultdict
from html.parser import HTMLParser

# arm type -> (decorator color, fill color); Okabe-Ito
ARM_COLORS = {
    "dominant": ((213, 94, 0), "213,94,0,200"),      # vermillion
    "star":     ((0, 114, 178), "0,114,178,200"),    # blue
    "co":       ((0, 158, 115), "0,158,115,200"),    # bluish green
}

# nucleotides on each side of the precursor in a pri region
FLANK = 30

FEAT_RE = re.compile(r"^(?P<gene>[A-Z][a-z]{2}-[^_]+)_(?P<feat>pri|pre|[35]p\*?|[35]p_co)(?P<seed>_seed)?$")

def armClass(armType):
    if armType.endswith("_co"):
        return "co"
    if armType.endswith("*"):
        return "star"
    return "dominant"

def v1Name(gene):
    """Hsa-Mir-10-P1b-v2 -> Hsa-Mir-10-P1b-v1; names without a variant are unchanged."""
    return re.sub(r"-v[0-9]+$", "-v1", gene)

def browseName(gene):
    """Hsa-Mir-10-P1b-v2 -> Hsa-Mir-10-P1b, the name of all variants in the browse table."""
    return re.sub(r"-v[0-9]+$", "", gene)

def readFeatures(bedFile):
    """Return pri dict gene -> (chrom, start, end, strand), arms gene -> [(start, end, type)],
    seeds (gene, armType) -> (chrom, start, end, strand)."""
    pris, arms, seeds = {}, defaultdict(list), {}
    ignored = defaultdict(int)
    with open(bedFile) as f:
        for line in f:
            chrom, start, end, name, score, strand = line.rstrip("\n").split("\t")[:6]
            start, end = int(start), int(end)
            m = FEAT_RE.match(name)
            if m is None:
                ignored[name.split("_", 1)[1] if "_" in name else name] += 1
                continue
            gene, feat = m.group("gene"), m.group("feat")
            if m.group("seed"):
                seeds[(gene, feat)] = (chrom, start, end, strand)
            elif feat == "pri":
                assert gene not in pris, "duplicate pri " + gene
                pris[gene] = (chrom, start, end, strand)
            elif feat != "pre":
                arms[gene].append((start, end, feat))
    for feat, n in sorted(ignored.items()):
        print("not used: %d %s features" % (n, feat), file=sys.stderr)
    return pris, arms, seeds

def genomeSeqs(items, twoBit):
    """Sequences of (chrom, start, end, strand) items as RNA, in item order.
    twoBitToFa -bed reverse-complements minus-strand items itself."""
    bedIn = "".join("%s\t%d\t%d\tx%d\t0\t%s\n" % (c, s, e, i, st)
                    for i, (c, s, e, st) in enumerate(items))
    out = subprocess.run(["twoBitToFa", "-bed=stdin", twoBit, "stdout"],
                         input=bedIn, capture_output=True, text=True, check=True).stdout
    seqs = defaultdict(list)
    name = None
    for line in out.splitlines():
        if line.startswith(">"):
            name = line[1:].split()[0]
        else:
            seqs[name].append(line)
    return ["".join(seqs["x%d" % i]).upper().replace("T", "U") for i in range(len(items))]

class BrowseParser(HTMLParser):
    """Collect the cell texts of the rows in the browse table body."""
    def __init__(self):
        super().__init__()
        self.inBody = self.inCell = False
        self.rows, self.row, self.cell = [], None, []
    def handle_starttag(self, tag, attrs):
        if tag == "tbody":
            self.inBody = True
        elif self.inBody and tag == "tr":
            self.row = []
        elif self.inBody and tag in ("td", "th") and self.row is not None:
            self.inCell, self.cell = True, []
    def handle_endtag(self, tag):
        if tag == "tbody":
            self.inBody = False
        elif self.inBody and tag in ("td", "th") and self.inCell:
            self.inCell = False
            self.row.append("".join(self.cell).strip())
        elif self.inBody and tag == "tr" and self.row:
            self.rows.append(self.row)
            self.row = None
    def handle_data(self, data):
        if self.inCell:
            self.cell.append(data)

def readBrowse(htmlFile):
    """gene -> dict(mirbaseId, family, seed, acc5p, acc3p).
    Columns: MirGeneDB ID, miRBase ID, family, seed, 5p accession, 3p accession, ..."""
    p = BrowseParser()
    with open(htmlFile) as f:
        p.feed(f.read())
    meta = {}
    for row in p.rows:
        if len(row) < 6 or not re.match(r"[A-Z][a-z]{2}-", row[0]):
            continue
        # genes with processing variants are marked with a "V" after the name;
        # genes not in miRBase have "None" as miRBase ID and accessions
        cells = ["" if c == "None" else c for c in row[1:6]]
        meta[re.sub(r"\s+V$", "", row[0])] = dict(zip(
            ["mirbaseId", "family", "seed", "acc5p", "acc3p"], cells))
    return meta

def readGffPreAcc(gffFile):
    """gene -> miRBase precursor accession (MI), from the pre_miRNA Alias."""
    acc = {}
    with open(gffFile) as f:
        for line in f:
            cols = line.rstrip("\n").split("\t")
            if len(cols) < 9 or cols[2] != "pre_miRNA":
                continue
            attrs = dict(a.split("=", 1) for a in cols[8].split(";") if "=" in a)
            gene = attrs["ID"].rsplit("_pre", 1)[0]
            if attrs.get("Alias", "").startswith("MI"):
                acc[gene] = attrs["Alias"]
    return acc


# one unbranched stem-loop, with interior loops and bulges allowed
HAIRPIN_RE = re.compile(r"^[(.]+\(\.+\)[).]+$")
# RNAsubopt of ViennaRNA 2.7.2, the version of /cluster/bin/x86_64/RNAfold; there is
# no RNAsubopt in /cluster/bin yet
RNASUBOPT = "/cluster/home/atanzer/.local/miniforge3/bin/RNAsubopt"
# energy ranges above the MFE, in kcal/mol, searched for a single-hairpin structure
SUBOPT_RANGES = (2, 4, 6, 8)

def isHairpin(dotBracket, flank):
    """True if the structure without flank nucleotides on each side is one hairpin."""
    return HAIRPIN_RE.match(dotBracket[flank:len(dotBracket) - flank]) is not None

def armInLoop(dotBracket, arms):
    """Largest number of nucleotides of one arm inside a hairpin loop; arms are 1-based
    (start, end) ranges in the sequence."""
    most = 0
    for m in re.finditer(r"\((\.+)\)", dotBracket):
        loopStart, loopEnd = m.start(1) + 1, m.end(1)
        for start, end in arms:
            most = max(most, min(end, loopEnd) - max(start, loopStart) + 1)
    return most

def isGood(dotBracket, arms, flank):
    """One hairpin (see isHairpin) with all arms in the stem, none in the hairpin loop."""
    return isHairpin(dotBracket, flank) and armInLoop(dotBracket, arms) == 0

def rnafold(records):
    """Fold (name, sequence, constraint) records with RNAfold -d2 -C;
    name -> (dotBracket, energy)."""
    out = subprocess.run(["RNAfold", "-d2", "--noPS", "-C"],
                         input="".join(">%s\n%s\n%s\n" % r for r in records),
                         capture_output=True, text=True, check=True).stdout
    lines = [l for l in out.splitlines() if l.strip()]
    res = {}
    for i in range(0, len(lines), 3):
        m = re.match(r"^([.()]+)\s+\(\s*(-?[0-9.]+)\)$", lines[i+2])
        assert lines[i].startswith(">") and m, "unexpected RNAfold output at " + lines[i]
        res[lines[i][1:].split()[0]] = (m.group(1), m.group(2))
    return res

def bestSubopt(seq, constraint, accept):
    """Lowest-energy RNAsubopt -d2 structure for which accept(dotBracket) is true,
    searched up to the largest of SUBOPT_RANGES above the MFE; (dotBracket, energy) or None."""
    for eRange in SUBOPT_RANGES:
        out = subprocess.run([RNASUBOPT, "-d2", "-s", "-e", str(eRange), "-C"],
                             input="%s\n%s\n" % (seq, constraint),
                             capture_output=True, text=True, check=True).stdout
        for line in out.splitlines()[1:]:
            dotBracket, energy = line.split()[:2]
            if accept(dotBracket):
                return dotBracket, "%.2f" % float(energy)
    return None

def hairpinRefold(items, flank):
    """Refold the items whose MFE structure is not one hairpin in the part without the
    flank nucleotides on each side, or has an arm nucleotide in the hairpin loop.
    items: name -> (sequence, dotBracket, arms). The flanks are kept unpaired (x in the
    constraint); the MFE under that constraint is taken if it is one hairpin with the arms
    in the stem, and otherwise the lowest-energy suboptimal structure that is. Items with
    no such structure in the searched range get the best structure by the earlier rule:
    the MFE if it is one hairpin, else the constrained MFE or the lowest-energy suboptimal
    structure that is one hairpin, else the MFE. Returns name -> (dotBracket, foldType
    text) for the refolded items."""
    bad = sorted(n for n, (seq, db, arms) in items.items() if not isGood(db, arms, flank))
    nNoHairpin = sum(1 for n in bad if not isHairpin(items[n][1], flank))
    constraint = {n: "x" * flank + "." * (len(items[n][0]) - 2 * flank) + "x" * flank
                  for n in bad}
    folds = rnafold([(n, items[n][0], constraint[n]) for n in bad]) if flank else {}
    refolded, armLoop, noHairpin = {}, [], []
    nUnpaired = nSubopt = 0
    for n in bad:
        seq, db, arms = items[n]
        prefix = "flanks unpaired, " if flank else ""
        if flank and isGood(folds[n][0], arms, flank):
            refolded[n] = (folds[n][0], "flanks unpaired, %s kcal/mol" % folds[n][1])
            nUnpaired += 1
            continue
        best = bestSubopt(seq, constraint[n], lambda d: isGood(d, arms, flank))
        if best:
            refolded[n] = (best[0], prefix + "suboptimal hairpin, %s kcal/mol" % best[1])
            nSubopt += 1
            continue
        # no structure with the arms in the stem: earlier rule, one hairpin only
        if isHairpin(db, flank):
            armLoop.append(n)
            continue
        if flank and isHairpin(folds[n][0], flank):
            refolded[n] = (folds[n][0], "flanks unpaired, %s kcal/mol" % folds[n][1])
            armLoop.append(n)
            continue
        best = bestSubopt(seq, constraint[n], lambda d: isHairpin(d, flank))
        if best:
            refolded[n] = (best[0], prefix + "suboptimal hairpin, %s kcal/mol" % best[1])
            armLoop.append(n)
        else:
            noHairpin.append(n)
    print("%d items, %d not one hairpin and %d with an arm in the hairpin loop in the MFE: "
          "%d refolded with the flanks unpaired, %d as suboptimal hairpins, all with the "
          "arms in the stem; %d with an arm in the loop and %d with no hairpin within "
          "%d kcal/mol" % (len(items), nNoHairpin, len(bad) - nNoHairpin, nUnpaired, nSubopt,
          len(armLoop), len(noHairpin), SUBOPT_RANGES[-1]), file=sys.stderr)
    for n in armLoop:
        print("arm in the hairpin loop: " + n, file=sys.stderr)
    for n in noHairpin:
        print("no hairpin, MFE kept: " + n, file=sys.stderr)
    return refolded

def foldType(refolded, name):
    """Text for the foldType field."""
    return refolded[name][1] if name in refolded else "MFE"

def readFold(foldFile):
    """RNAfold output -> name -> (sequence, dotBracket, mfe)."""
    folds = {}
    with open(foldFile) as f:
        lines = [l.rstrip("\n") for l in f if l.strip()]
    for i in range(0, len(lines), 3):
        head, seq, struct = lines[i:i+3]
        m = re.match(r"^([.()]+)\s+\(\s*(-?[0-9.]+)\)$", struct)
        assert head.startswith(">") and m, "unexpected RNAfold output at " + head
        assert len(m.group(1)) == len(seq), "structure length differs: " + head
        folds[head[1:].split()[0]] = (seq, m.group(1), m.group(2))
    return folds

def armRange(pStart, pEnd, strand, aStart, aEnd):
    """1-based range of an arm in the pri-miRNA sequence (5' to 3')."""
    if strand == "+":
        return aStart - pStart + 1, aEnd - pStart
    return pEnd - aEnd + 1, pEnd - aStart

def priFa(bedFile, twoBit, faOut):
    pris, arms, seeds = readFeatures(bedFile)
    genes = sorted(pris)
    seqs = genomeSeqs([pris[g] for g in genes], twoBit)
    with open(faOut, "w") as out:
        for g, s in zip(genes, seqs):
            out.write(">%s\n%s\n" % (g, s))
    print("%d pri-miRNA sequences" % len(genes), file=sys.stderr)

def bed(bedFile, gffFile, browseFile, foldFile, twoBit, bedOut, armsOut):
    pris, arms, seeds = readFeatures(bedFile)
    meta = readBrowse(browseFile)
    preAcc = readGffPreAcc(gffFile)
    folds = readFold(foldFile)
    print("%d pri-miRNAs, %d genes in browse table, %d MI accessions in GFF, %d folds" %
          (len(pris), len(meta), len(preAcc), len(folds)), file=sys.stderr)

    seedKeys = sorted(seeds)
    seedSeq = dict(zip(seedKeys, genomeSeqs([seeds[k] for k in seedKeys], twoBit)))

    # the pri regions are the precursors plus FLANK nt on each side
    items = {}
    for g, (chrom, start, end, strand) in pris.items():
        items[g] = folds[g][:2] + ([armRange(start, end, strand, s, e)
                                     for s, e, t in arms.get(g, [])],)
    refolded = hairpinRefold(items, FLANK)

    noMeta = seedDiff = 0
    with open(bedOut, "w") as out, open(armsOut, "w") as aOut:
        for gene in sorted(pris, key=lambda g: (pris[g][0], pris[g][1], g)):
            chrom, start, end, strand = pris[gene]
            m = meta.get(browseName(gene))
            if m is None:
                noMeta += 1
                print("no browse table row for " + gene, file=sys.stderr)
                m = dict(mirbaseId="", family="", seed="", acc5p="", acc3p="")
            mi = preAcc.get(gene) or preAcc.get(v1Name(gene), "")

            geneArms = sorted(arms.get(gene, []))
            # seed of the dominant arm(s), from the genome at this locus
            domSeeds = [seedSeq[(gene, t)] for s, e, t in geneArms
                        if armClass(t) != "star" and (gene, t) in seedSeq]
            seed = ",".join(domSeeds)
            if v1Name(gene) == gene and m["seed"] and m["seed"] not in domSeeds:
                seedDiff += 1
                print("seed differs from browse table: %s %s %s" % (gene, seed, m["seed"]),
                      file=sys.stderr)

            seq, dotBracket, mfe = folds[gene]
            if gene in refolded:
                dotBracket = refolded[gene][0]
            armPos, armText = [], []
            for aStart, aEnd, armType in geneArms:
                if aStart < start or aEnd > end:
                    sys.exit("arm %s %s outside its pri-miRNA" % (gene, armType))
                s, e = armRange(start, end, strand, aStart, aEnd)
                armPos.append((s, e, armType))
                armText.append("%s %s:%d-%d" % (armType, chrom, aStart + 1, aEnd))
                color, fill = ARM_COLORS[armClass(armType)]
                aOut.write("\t".join(str(x) for x in [
                    chrom, aStart, aEnd, "%s_%s" % (gene, armType), 0, strand,
                    aStart, aEnd, "%d,%d,%d" % color, 1, "%d," % (aEnd - aStart), "0,",
                    "%s:%d-%d:%s" % (chrom, start, end, gene), "block", fill, ""]) + "\n")
            armPos.sort()

            out.write("\t".join([chrom, str(start), str(end), gene, "0", strand,
                m["mirbaseId"], m["family"], seed, mi, m["acc5p"], m["acc3p"],
                gene.split("-", 1)[1], ", ".join(armText), mfe, foldType(refolded, gene),
                seq, dotBracket,
                ",".join("%d-%d:%s" % a for a in armPos)]) + "\n")
    print("%d without browse table row, %d seeds differ from browse table" %
          (noMeta, seedDiff), file=sys.stderr)

if __name__ == "__main__":
    if len(sys.argv) == 5 and sys.argv[1] == "priFa":
        priFa(*sys.argv[2:])
    elif len(sys.argv) == 9 and sys.argv[1] == "bed":
        bed(*sys.argv[2:])
    else:
        sys.exit(__doc__)

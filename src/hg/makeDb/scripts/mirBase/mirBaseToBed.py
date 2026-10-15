#!/usr/bin/env python3
"""Convert the miRBase human GFF3 to the mirBase track files.

Two steps, with RNAfold in between (see mirBaseBuild.sh):

  mirBaseToBed.py preFa hsa.gff3 hg38.2bit pre.fa
      Write the precursor (hairpin) sequences, taken from the genome at the miRBase
      coordinates, as FASTA for RNAfold. The FASTA names are the GFF IDs, which are
      unique per locus (MI0033425, MI0033425_2, ...), unlike the precursor names.

  mirBaseToBed.py bed hsa.gff3 pre.fold mirBase.bed mirBaseArms.bed
      Write the precursor track (bed6+, see mirBase.as) and the mature miRNA
      decorators (bed12+4, kent/src/hg/lib/decoration.as).
      Precursors whose MFE structure is not one hairpin get the lowest-energy
      suboptimal structure that is one (see hairpinRefold), from RNAsubopt.
      A few structures are chosen by hand (MANUAL_STRUCTURES).

A mature miRNA's Derives_from names the precursor accession (MI0033425), not the
locus, so for a precursor at several loci the mature is assigned to the locus with
that accession that contains it. The arm is taken from the -5p/-3p suffix of the
mature name, or, for names without one, from the side of the hairpin it is on.
"""
import re
import subprocess
import sys
from collections import defaultdict

# arm -> (decorator color, fill color); Okabe-Ito, different from the
# dominant/star colors of the MirGeneDB track
ARM_COLORS = {
    "5p": ((230, 159, 0), "230,159,0,200"),      # orange
    "3p": ((86, 180, 233), "86,180,233,200"),    # sky blue
}

def readGff(gffFile):
    pres, matures = [], []
    with open(gffFile) as f:
        for line in f:
            if line.startswith("#"):
                continue
            cols = line.rstrip("\n").split("\t")
            attrs = dict(a.split("=", 1) for a in cols[8].split(";") if "=" in a)
            rec = dict(chrom=cols[0], start=int(cols[3]) - 1, end=int(cols[4]),
                       strand=cols[6], id=attrs["ID"], acc=attrs["Alias"],
                       name=attrs["Name"])
            if cols[2] == "miRNA_primary_transcript":
                pres.append(rec)
            elif cols[2] == "miRNA":
                rec["parent"] = attrs["Derives_from"]
                matures.append(rec)
            else:
                sys.exit("unexpected feature type " + cols[2])
    ids = [p["id"] for p in pres]
    assert len(set(ids)) == len(ids), "duplicate precursor IDs"
    return pres, matures

def armRange(pre, start, end):
    """1-based range in the precursor sequence (5' to 3')."""
    if pre["strand"] == "+":
        return start - pre["start"] + 1, end - pre["start"]
    return pre["end"] - end + 1, pre["end"] - start

def assignMatures(pres, matures):
    """precursor ID -> list of (mature, arm)."""
    byAcc = defaultdict(list)
    for p in pres:
        byAcc[p["acc"]].append(p)
    res = defaultdict(list)
    for m in matures:
        cand = [p for p in byAcc[m["parent"]] if p["chrom"] == m["chrom"] and
                p["strand"] == m["strand"] and p["start"] <= m["start"] and m["end"] <= p["end"]]
        if len(cand) != 1:
            sys.exit("%d precursor loci contain mature %s %s:%d-%d" %
                     (len(cand), m["name"], m["chrom"], m["start"], m["end"]))
        p = cand[0]
        if m["name"].endswith("-5p"):
            arm = "5p"
        elif m["name"].endswith("-3p"):
            arm = "3p"
        else:
            s, e = armRange(p, m["start"], m["end"])
            arm = "5p" if (s + e) / 2 < (p["end"] - p["start"] + 1) / 2 else "3p"
        res[p["id"]].append((m, arm))
    return res

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


# one unbranched stem-loop, with interior loops and bulges allowed
HAIRPIN_RE = re.compile(r"^[(.]+\(\.+\)[).]+$")
# RNAsubopt of ViennaRNA 2.7.2, the version of /cluster/bin/x86_64/RNAfold; there is
# no RNAsubopt in /cluster/bin yet
RNASUBOPT = "/cluster/home/atanzer/.local/miniforge3/bin/RNAsubopt"
# energy ranges above the MFE, in kcal/mol, searched for a single-hairpin structure
SUBOPT_RANGES = (2, 4, 6, 8)
RNAEVAL = RNASUBOPT.replace("RNAsubopt", "RNAeval")

# structures chosen by hand from the RNAsubopt -d2 output, for precursors with no
# single hairpin within the SUBOPT_RANGES. hsa-mir-6134: the suboptimal structure
# 0.8 kcal/mol above the MFE, in which the mature is more helical (a 1 nt bulge, U79,
# and a 3 nt bulge, GAU 82-84, instead of the 2x6 interior loop of the MFE).
MANUAL_STRUCTURES = {
    "MI0021279": ".((((((((.......))))))))(((((((...(((((((((((((((((((.(((............))))))))).))...))))))..)))))...)))))))..",
}

def isHairpin(dotBracket, flank):
    """True if the structure without flank nucleotides on each side is one hairpin."""
    return HAIRPIN_RE.match(dotBracket[flank:len(dotBracket) - flank]) is not None

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

def bestHairpin(seq, constraint, flank):
    """Lowest-energy RNAsubopt -d2 structure that is one hairpin (see isHairpin), searched
    up to the largest of SUBOPT_RANGES above the MFE; (dotBracket, energy) or None."""
    for eRange in SUBOPT_RANGES:
        out = subprocess.run([RNASUBOPT, "-d2", "-s", "-e", str(eRange), "-C"],
                             input="%s\n%s\n" % (seq, constraint),
                             capture_output=True, text=True, check=True).stdout
        for line in out.splitlines()[1:]:
            dotBracket, energy = line.split()[:2]
            if isHairpin(dotBracket, flank):
                return dotBracket, "%.2f" % float(energy)
    return None

def hairpinRefold(items, flank):
    """Refold the items whose MFE structure is not one hairpin, in the part without the
    flank nucleotides on each side. items: name -> (sequence, dotBracket).
    The flanks are kept unpaired (x in the constraint), and if the MFE under that
    constraint is not one hairpin either, the lowest-energy suboptimal structure that is
    one is taken. Returns name -> (dotBracket, foldType text) for the refolded items;
    items without a hairpin in the searched range keep their MFE structure."""
    bad = sorted(n for n, (seq, db) in items.items() if not isHairpin(db, flank))
    constraint = {n: "x" * flank + "." * (len(items[n][0]) - 2 * flank) + "x" * flank
                  for n in bad}
    refolded = {}
    if flank:
        folds = rnafold([(n, items[n][0], constraint[n]) for n in bad])
        for n in bad:
            if isHairpin(folds[n][0], flank):
                refolded[n] = (folds[n][0], "flanks unpaired, %s kcal/mol" % folds[n][1])
    nUnpaired = len(refolded)
    noHairpin = []
    for n in bad:
        if n in refolded:
            continue
        best = bestHairpin(items[n][0], constraint[n], flank)
        if best is None:
            noHairpin.append(n)
        else:
            refolded[n] = (best[0], ("flanks unpaired, " if flank else "") +
                           "suboptimal hairpin, %s kcal/mol" % best[1])
    print("%d items, %d without one hairpin in the MFE: %d refolded with the flanks "
          "unpaired, %d as suboptimal hairpins, %d with no hairpin within %d kcal/mol" %
          (len(items), len(bad), nUnpaired, len(refolded) - nUnpaired, len(noHairpin),
           SUBOPT_RANGES[-1]), file=sys.stderr)
    for n in noHairpin:
        print("no hairpin, MFE kept: " + n, file=sys.stderr)
    return refolded

def manualStructures(items):
    """name -> (dotBracket, foldType text) for MANUAL_STRUCTURES, with the energy from
    RNAeval -d2, which also checks that the structure fits the sequence."""
    res = {}
    for n, dotBracket in MANUAL_STRUCTURES.items():
        seq = items[n][0]
        assert len(dotBracket) == len(seq), "manual structure length differs: " + n
        out = subprocess.run([RNAEVAL, "-d2"], input="%s\n%s\n" % (seq, dotBracket),
                             capture_output=True, text=True, check=True).stdout.splitlines()
        m = re.match(r"^([.()]+)\s+\(\s*(-?[0-9.]+)\)$", out[-1])
        assert m and m.group(1) == dotBracket, "unexpected RNAeval output for " + n
        res[n] = (dotBracket, "suboptimal chosen by hand, %s kcal/mol" % m.group(2))
        print("structure chosen by hand: %s, %s kcal/mol" % (n, m.group(2)), file=sys.stderr)
    return res

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

def preFa(gffFile, twoBit, faOut):
    pres, matures = readGff(gffFile)
    seqs = genomeSeqs([(p["chrom"], p["start"], p["end"], p["strand"]) for p in pres], twoBit)
    with open(faOut, "w") as out:
        for p, s in zip(pres, seqs):
            out.write(">%s\n%s\n" % (p["id"], s))
    print("%d precursor sequences" % len(pres), file=sys.stderr)

def bed(gffFile, foldFile, bedOut, armsOut):
    pres, matures = readGff(gffFile)
    folds = readFold(foldFile)
    assigned = assignMatures(pres, matures)
    print("%d precursors, %d matures, %d folds" % (len(pres), len(matures), len(folds)),
          file=sys.stderr)
    items = {p["id"]: folds[p["id"]][:2] for p in pres}
    refolded = hairpinRefold(items, 0)
    refolded.update(manualStructures(items))

    armCount = defaultdict(int)
    with open(bedOut, "w") as out, open(armsOut, "w") as aOut:
        for p in pres:
            ms = sorted(assigned.get(p["id"], []), key=lambda x: x[0]["start"])
            byArm = defaultdict(list)
            armPos, armText = [], []
            for m, arm in ms:
                armCount[arm] += 1
                byArm[arm].append(m)
                s, e = armRange(p, m["start"], m["end"])
                armPos.append((s, e, arm))
                armText.append("%s %s:%d-%d" % (m["name"], m["chrom"], m["start"] + 1, m["end"]))
                color, fill = ARM_COLORS[arm]
                aOut.write("\t".join(str(x) for x in [
                    m["chrom"], m["start"], m["end"], m["name"], 0, m["strand"],
                    m["start"], m["end"], "%d,%d,%d" % color, 1, "%d," % (m["end"] - m["start"]),
                    "0,", "%s:%d-%d:%s" % (p["chrom"], p["start"], p["end"], p["name"]),
                    "block", fill, ""]) + "\n")
            for arm in byArm:
                if len(byArm[arm]) > 1:
                    print("%s has %d matures on the %s arm" % (p["id"], len(byArm[arm]), arm),
                          file=sys.stderr)
            armPos.sort()
            seq, dotBracket, mfe = folds[p["id"]]
            if p["id"] in refolded:
                dotBracket = refolded[p["id"]][0]
            out.write("\t".join([p["chrom"], str(p["start"]), str(p["end"]), p["name"], "0",
                p["strand"], p["acc"],
                ",".join(m["name"] for m in byArm["5p"]), ",".join(m["acc"] for m in byArm["5p"]),
                ",".join(m["name"] for m in byArm["3p"]), ",".join(m["acc"] for m in byArm["3p"]),
                ", ".join(armText), mfe, foldType(refolded, p["id"]), seq, dotBracket,
                ",".join("%d-%d:%s" % a for a in armPos)]) + "\n")
    print("matures per arm: " + ", ".join("%s %d" % kv for kv in sorted(armCount.items())),
          file=sys.stderr)

if __name__ == "__main__":
    if len(sys.argv) == 5 and sys.argv[1] == "preFa":
        preFa(*sys.argv[2:])
    elif len(sys.argv) == 6 and sys.argv[1] == "bed":
        bed(*sys.argv[2:])
    else:
        sys.exit(__doc__)

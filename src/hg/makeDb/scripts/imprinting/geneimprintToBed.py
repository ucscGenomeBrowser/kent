#!/usr/bin/env python3
"""
Turn the Geneimprint human catalog of imprinted genes into a bed9+ file.

Geneimprint (https://geneimprint.com) lists candidate imprinted genes with a
cytogenetic band but no coordinates, so the gene symbols have to be resolved
against a gene annotation. We resolve against the HGNC track, which carries the
approved symbol, the previous approved symbols and the alias symbols for every
locus together with its cytogenetic band. Two fallbacks catch what HGNC cannot
match on a symbol alone: small RNA cluster names such as SNORD115@, which are
spanned from the first to the last numbered family member, and a handful of
symbols that only survive in RefSeq.

A few genes appear twice in the catalog, under a current and a retired symbol;
those rows collapse into one feature that carries both names.

The band from Geneimprint is used to break ties when a symbol matches more than
one locus, and a match that lands on a different chromosome than Geneimprint
states is refused rather than published.

Usage:
    geneimprintToBed.py <geneimprint.html> <hgnc.bed> <refGene.tsv> <out.bed> <out.report.txt>

<geneimprint.html> is https://geneimprint.com/site/genes-by-species.Homo+sapiens
<hgnc.bed>         is bigBedToBed of /gbdb/hg38/hgnc/hgnc.bb
<refGene.tsv>      is chrom, txStart, txEnd, strand, name2 from the hg38 refGene table
"""
import sys, re, html
from collections import defaultdict

# HGNC bigBed column indexes, 0-based; see the autoSql in hgnc.bb
H_CHROM, H_START, H_END, H_ID, H_STRAND = 0, 1, 2, 3, 5
H_SYMBOL, H_GENENAME, H_LOCUSTYPE, H_BAND = 9, 10, 12, 14
H_ALIAS, H_PREV = 16, 18

MAIN_CHROM = re.compile(r"^chr([0-9]+|X|Y|M)$")

# One color scheme is shared by the whole Imprinting collection: vermillion
# means the maternal copy, blue means the paternal copy, and neutral gray means
# the annotation carries no parent of origin. Categories that exist only in this
# catalog get their own hues, which no other subtrack reuses. Okabe-Ito, so the
# colors stay distinguishable under all three kinds of colorblindness.
ALLELE_COLOR = {
    "Paternal":          "0,114,178",    # blue,           shared: paternal copy
    "Maternal":          "213,94,0",     # vermillion,     shared: maternal copy
    "Unknown":           "85,85,85",     # gray,           shared: no parent of origin
    "Biallelic":         "0,158,115",    # bluish green,   this catalog only
    "Isoform Dependent": "230,159,0",    # orange,         this catalog only
    "Random":            "204,121,167",  # reddish purple, this catalog only
}


def parseGeneimprint(fname):
    " pull the gene table out of the Geneimprint species page "
    text = open(fname, encoding="utf8", errors="replace").read()
    rowRe = re.compile(
        r'<tr>\s*<td class="gene">(.*?)</td>\s*'
        r'<td class="aliases">(.*?)</td>\s*'
        r'<td class="location">(.*?)</td>\s*'
        r'<td class="status">(.*?)</td>\s*'
        r'<td class="allele">(.*?)</td>', re.S)

    def clean(s):
        s = re.sub(r"<[^>]+>", "", s)                 # cells hold <a> and <em>
        s = html.unescape(s).replace("\xa0", " ")
        return " ".join(s.split()).strip(",").strip()

    rows = []
    for gene, aliases, location, status, allele in rowRe.findall(text):
        # <em>AS</em> in the location cell marks a gene on the antisense
        # (minus) strand, it is not part of the band
        antisense = "<em>" in location
        band = clean(location)
        if band.endswith(" AS"):
            band = band[:-3].strip()
        rows.append(dict(gene=clean(gene), aliases=clean(aliases), band=band,
                         status=clean(status), allele=clean(allele) or "Unknown",
                         antisense=antisense))
    if not rows:
        sys.exit("error: no rows parsed from %s, the page layout may have changed" % fname)
    return rows


def loadHgnc(fname):
    """ three symbol indexes (approved, previous, alias) plus an index of
    numbered family members for cluster names. Main chromosomes only, so an
    alt or fix haplotype can never win a match. """
    bySymbol, byPrev, byAlias = defaultdict(list), defaultdict(list), defaultdict(list)
    families = defaultdict(list)
    for line in open(fname):
        f = line.rstrip("\n").split("\t")
        if not MAIN_CHROM.match(f[H_CHROM]):
            continue
        locus = dict(chrom=f[H_CHROM], start=int(f[H_START]), end=int(f[H_END]),
                     strand=f[H_STRAND], hgncId=f[H_ID], symbol=f[H_SYMBOL],
                     geneName=f[H_GENENAME], locusType=f[H_LOCUSTYPE], band=f[H_BAND])
        bySymbol[f[H_SYMBOL].upper()].append(locus)
        for prev in f[H_PREV].split("|"):
            if prev:
                byPrev[prev.upper()].append(locus)
        for alias in f[H_ALIAS].split("|"):
            if alias:
                byAlias[alias.upper()].append(locus)
        fam = re.match(r"^(.*?)-[0-9]+$", f[H_SYMBOL])
        if fam:
            families[fam.group(1).upper()].append(locus)
    return bySymbol, byPrev, byAlias, families


def loadRefGene(fname):
    " symbol -> loci, from the refGene table, for symbols HGNC has retired "
    byName = defaultdict(list)
    for line in open(fname):
        f = line.rstrip("\n").split("\t")
        if len(f) < 5 or not MAIN_CHROM.match(f[0]):
            continue
        byName[f[4].upper()].append(dict(
            chrom=f[0], start=int(f[1]), end=int(f[2]), strand=f[3],
            hgncId="", symbol=f[4], geneName="", locusType="", band=""))
    return byName


def bandChrom(band):
    " '15q11.2' -> 'chr15'; '' when the band cannot be read "
    m = re.match(r"^([0-9]{1,2}|X|Y)[pq]", band)
    return "chr" + m.group(1) if m else ""


def bandArm(band):
    " '15q11.2' -> '15q'; '' when the band cannot be read "
    m = re.match(r"^([0-9]{1,2}|X|Y)([pq])", band)
    return m.group(1) + m.group(2) if m else ""


def pickLocus(candidates, giBand):
    """ choose among loci matching one symbol, preferring agreement with the
    Geneimprint band, first on the arm and then on the chromosome """
    if len(candidates) == 1:
        return candidates[0]
    for key, want in ((bandArm, bandArm(giBand)), (lambda c: c["chrom"], bandChrom(giBand))):
        if not want:
            continue
        if key is bandArm:
            same = [c for c in candidates if bandArm(c["band"]) == want]
        else:
            same = [c for c in candidates if c["chrom"] == want]
        if len(same) == 1:
            return same[0]
        if same:
            candidates = same
    # still ambiguous: longest locus, ties broken by ID so the result is stable
    return sorted(candidates, key=lambda c: (c["start"] - c["end"], c["hgncId"], c["chrom"]))[0]


def spanFamily(members, giBand):
    """ collapse a numbered family (SNORD116-1 .. SNORD116-30) into the span it
    covers on the chromosome the catalog points at """
    chrom = bandChrom(giBand)
    onChrom = [m for m in members if m["chrom"] == chrom] if chrom else members
    if not onChrom:
        return None
    strands = set(m["strand"] for m in onChrom)
    return dict(chrom=onChrom[0]["chrom"],
                start=min(m["start"] for m in onChrom),
                end=max(m["end"] for m in onChrom),
                strand=strands.pop() if len(strands) == 1 else ".",
                hgncId="", symbol=onChrom[0]["symbol"].rsplit("-", 1)[0],
                geneName="cluster of %d family members" % len(onChrom),
                locusType=onChrom[0]["locusType"],
                band=onChrom[0]["band"])


def resolve(row, bySymbol, byPrev, byAlias, families, byRefGene):
    """ (locus, howMatched) or (None, reason). The catalog symbol is tried
    against all indexes before any of its aliases is tried. """
    catalogSym = row["gene"].rstrip("@")
    tables = (("symbol", bySymbol), ("prevSymbol", byPrev), ("alias", byAlias))

    for level, table in tables:
        hits = table.get(catalogSym.upper())
        if hits:
            return pickLocus(hits, row["band"]), level

    members = families.get(catalogSym.upper())
    if members:
        locus = spanFamily(members, row["band"])
        if locus:
            return locus, "cluster"

    aliases = [a.strip() for a in row["aliases"].split(",") if a.strip()]
    for alias in aliases:
        for level, table in tables:
            hits = table.get(alias.upper())
            if hits:
                return pickLocus(hits, row["band"]), "catalogAlias"
        members = families.get(alias.upper())
        if members:
            locus = spanFamily(members, row["band"])
            if locus:
                return locus, "cluster"

    for name in [catalogSym] + aliases:
        hits = byRefGene.get(name.upper())
        if hits:
            return pickLocus(hits, row["band"]), "refGene"

    return None, "no locus in HGNC or refGene for %s or its aliases" % row["gene"]


RESOLVE_RANK = {"symbol": 0, "prevSymbol": 1, "alias": 2, "catalogAlias": 3,
                "cluster": 4, "refGene": 5}


def mergeDuplicates(beds):
    """ The catalog lists a few genes twice, once under the current symbol and
    once under a retired one, so both rows resolve to the same locus. Keep one
    feature and fold the other names into its alias list. """
    byKey = defaultdict(list)
    for bed in beds:
        byKey[(bed[0], bed[1], bed[2], bed[9], bed[10])].append(bed)
    out, merged = [], []
    for key in byKey:
        group = byKey[key]
        if len(group) == 1:
            out.append(group[0])
            continue
        group.sort(key=lambda b: (RESOLVE_RANK.get(b[17], 9), b[3]))
        keep, rest = group[0], group[1:]
        extra = [b[3] for b in rest]
        aliases = [a.strip() for a in keep[11].split(",") if a.strip()]
        for name in extra:
            if name not in aliases and name != keep[3]:
                aliases.append(name)
        keep[11] = ", ".join(aliases)
        out.append(keep)
        merged.append((keep[3], extra))
    return out, merged


def countBy(beds, idx):
    counts = defaultdict(int)
    for b in beds:
        counts[b[idx]] += 1
    return counts


def main():
    if len(sys.argv) != 6:
        sys.exit(__doc__)
    giFname, hgncFname, refGeneFname, outFname, reportFname = sys.argv[1:6]

    rows = parseGeneimprint(giFname)
    bySymbol, byPrev, byAlias, families = loadHgnc(hgncFname)
    byRefGene = loadRefGene(refGeneFname)

    beds, dropped, armMismatch, strandMismatch = [], [], [], []
    for row in rows:
        locus, how = resolve(row, bySymbol, byPrev, byAlias, families, byRefGene)
        if locus is None:
            dropped.append((row, how))
            continue

        giChrom = bandChrom(row["band"])
        if giChrom and giChrom != locus["chrom"]:
            # an unresolved symbol collision; refuse to publish a locus that
            # contradicts the catalog rather than guess
            dropped.append((row, "%s resolves to %s but Geneimprint says %s"
                            % (row["gene"], locus["chrom"], row["band"])))
            continue
        if bandArm(row["band"]) and bandArm(locus["band"]) \
                and bandArm(locus["band"]) != bandArm(row["band"]):
            armMismatch.append((row, locus))
        if locus["strand"] != ("-" if row["antisense"] else "+"):
            strandMismatch.append((row, locus))

        beds.append([
            locus["chrom"], locus["start"], locus["end"], row["gene"], 0,
            locus["strand"], locus["start"], locus["end"],
            ALLELE_COLOR.get(row["allele"], ALLELE_COLOR["Unknown"]),
            row["status"], row["allele"], row["aliases"], row["band"],
            locus["symbol"], locus["hgncId"], locus["geneName"],
            locus["locusType"], how,
        ])

    beds, merged = mergeDuplicates(beds)

    beds.sort(key=lambda b: (b[0], b[1], b[2], b[3]))
    with open(outFname, "w") as fh:
        for b in beds:
            fh.write("\t".join(str(x) for x in b) + "\n")

    with open(reportFname, "w") as fh:
        def out(s=""):
            fh.write(s + "\n")
            print(s)

        out("Geneimprint human catalog to hg38 bed")
        out("rows in the Geneimprint table:  %d" % len(rows))
        out("features written:               %d" % len(beds))
        out("rows dropped:                   %d" % len(dropped))
        out()
        for label, idx in (("imprint status", 9), ("expressed allele", 10),
                           ("how the symbol was resolved", 17)):
            out("features by %s:" % label)
            for k, v in sorted(countBy(beds, idx).items(), key=lambda kv: -kv[1]):
                out("    %-20s %d" % (k, v))
        out()
        out("dropped rows (%d):" % len(dropped))
        for row, why in dropped:
            out("    %-14s %-12s %s" % (row["gene"], row["band"], why))
        out()
        out("catalog rows merged because they resolve to the same locus with the "
            "same status (%d):" % len(merged))
        for name, extra in sorted(merged):
            out("    %-14s also listed as %s" % (name, ", ".join(extra)))
        out()
        out("chromosome arm disagrees with the annotation (%d, kept):" % len(armMismatch))
        for row, locus in armMismatch:
            out("    %-14s Geneimprint %-12s annotation %s"
                % (row["gene"], row["band"], locus["band"]))
        out()
        out("antisense marker disagrees with the annotated strand "
            "(%d, kept, annotated strand used):" % len(strandMismatch))
        for row, locus in strandMismatch:
            out("    %-14s catalog %-6s annotation %s"
                % (row["gene"], "AS" if row["antisense"] else "sense", locus["strand"]))


main()

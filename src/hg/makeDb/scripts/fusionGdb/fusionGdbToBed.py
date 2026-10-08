#!/usr/bin/env python3
"""Convert the FusionGDB fusion breakpoint table to a bigBed-ready BED file.

Every distinct pair of breakpoints (5' gene, chrom, position, strand, 3' gene,
chrom, position, strand) becomes one fusion event. Each event is written as two
1 bp features, one at the 5' partner breakpoint and one at the 3' partner
breakpoint, so the fusion shows up on both genes. Rows from different sources
or samples with the same breakpoints are merged into one event.

FusionGDB breakpoints are 1-based hg19 positions: in the TCGA call sets, about
95% of them fall on the last base of the 5' partner's exon and the first base of
the 3' partner's exon in GENCODE V19. For hg38, the breakpoints are lifted with
liftOver.

usage: fusionGdbToBed.py infoFile orfFile db outBed outIxTxt
  db is hg19 or hg38.
"""
import sys, os, subprocess, tempfile, json, urllib.parse
from collections import defaultdict, Counter

srcNames = {"ChiTaRS3.1": "ChiTaRS", "TCGARV": "TumorFusions", "TCGALD": "PanCanAtlas"}

# Okabe-Ito blue and vermillion, plus grey
colors = {"In-frame": "0,114,178", "Frame-shift": "213,94,0"}
otherColor = "136,136,136"

warnCounts = Counter()

def warn(key):
    warnCounts[key] += 1

def parseInfo(fname):
    """ return dict (5'gene,chr,pos,strand,3'gene,chr,pos,strand) -> dict with sets of sources etc """
    events = defaultdict(lambda: {"sources": set(), "cancers": set(), "samples": set(), "accs": set()})
    rowCount = 0
    for line in open(fname):
        row = line.rstrip("\r\n").split("\t")
        assert len(row) == 12, row
        src, subSrc, cancer, sample, g5, c5, p5, s5, g3, c3, p3, s3 = row
        rowCount += 1
        key = (g5, c5, int(p5), s5, g3, c3, int(p3), s3)
        ev = events[key]
        ev["sources"].add(srcNames[src + subSrc])
        if cancer:
            ev["cancers"].add(cancer)
        if sample.startswith("TCGA-"):
            ev["samples"].add(sample)
        elif sample:
            ev["accs"].add(sample)
    print("%s: %d rows, %d distinct breakpoint pairs" % (fname, rowCount, len(events)), file=sys.stderr)
    return events

def parseOrf(fname):
    """ return dict breakpoint key -> set of ORF classes (In-frame, Frame-shift, 5CDS-intron, ...) and
    a dict key -> set of in-frame ENST pairs """
    classes = defaultdict(set)
    inFrameTx = defaultdict(set)
    for line in open(fname):
        row = line.rstrip("\r\n").split("\t")
        orfClass, tx5, tx3 = row[0], row[1], row[2]
        g5, c5, p5, s5, g3, c3, p3, s3 = row[8:16]
        key = (g5, c5, int(p5), s5, g3, c3, int(p3), s3)
        classes[key].add(orfClass)
        if orfClass == "In-frame":
            inFrameTx[key].add(tx5 + "::" + tx3)
    return classes, inFrameTx

def summarizeFrame(orfClasses):
    if "In-frame" in orfClasses:
        return "In-frame"
    if "Frame-shift" in orfClasses:
        return "Frame-shift"
    if orfClasses:
        return "Outside CDS"
    return "Not annotated"

def liftBreakpoints(bps, chromSizes):
    """ bps is a set of (chrom, pos) on hg19, 1-based. Return dict (chrom,pos) -> (hg38chrom, hg38pos) or None """
    mapping = {}
    tmpDir = tempfile.mkdtemp(dir=".")
    inBed = os.path.join(tmpDir, "in.bed")
    outBed = os.path.join(tmpDir, "out.bed")
    unBed = os.path.join(tmpDir, "unmapped.bed")
    with open(inBed, "w") as ofh:
        for chrom, pos in sorted(bps):
            if chrom == "chrMT":
                # rCRS on both assemblies, hg38 calls it chrM
                mapping[(chrom, pos)] = ("chrM", pos)
                continue
            ofh.write("%s\t%d\t%d\t%s:%d\n" % (chrom, pos-1, pos, chrom, pos))
    subprocess.run(["liftOver", "-minMatch=1", inBed, "/gbdb/hg19/liftOver/hg19ToHg38.over.chain.gz",
        outBed, unBed], check=True, stderr=subprocess.DEVNULL)
    for line in open(outBed):
        chrom, start, end, name = line.rstrip("\n").split("\t")
        oldChrom, oldPos = name.split(":")
        if chrom not in chromSizes:
            warn("breakpoint lifted to a sequence not in hg38 chromInfo")
            continue
        mapping[(oldChrom, int(oldPos))] = (chrom, int(end))
    for chrom, pos in bps:
        if (chrom, pos) not in mapping:
            warn("breakpoint could not be lifted from hg19 to hg38")
            mapping[(chrom, pos)] = None
    for fn in (inBed, outBed, unBed):
        os.remove(fn)
    os.rmdir(tmpDir)
    return mapping

def readChromSizes(db):
    sizes = {}
    for line in open("/hive/data/genomes/%s/chrom.sizes" % db):
        chrom, size = line.split()
        sizes[chrom] = int(size)
    return sizes

def posStr(pos):
    if pos is None:
        return ""
    return "%s:%d" % pos

def makeEventIds(events):
    """ return dict event key -> unique ID "<5' gene>::<3' gene>.<n>". The same fusion gene can
    have many breakpoint pairs, n numbers them in the order of their hg19 coordinates. The
    features are named <ID>.5p and <ID>.3p, so every feature has a unique name, which hgc and
    the hgFind.matches highlight of the partner links need. """
    byFusion = defaultdict(list)
    for key in events:
        byFusion[key[0] + "::" + key[4]].append(key)
    ids = {}
    for fusion, keys in byFusion.items():
        for i, key in enumerate(sorted(keys)):
            ids[key] = "%s.%d" % (fusion, i + 1)
    return ids

def featureName(eventId, side):
    return eventId + (".5p" if side == "5'" else ".3p")

def otherSide(side):
    return "3'" if side == "5'" else "5'"

def partnerWindow(pos, chromSizes, pad=5000):
    " return a position string for a window around a breakpoint, for links to the partner "
    if pos is None:
        return ""
    chrom, bp = pos
    return "%s:%d-%d" % (chrom, max(1, bp - pad), min(chromSizes[chrom], bp + pad))

def partnerQuery(region, name):
    """ return the hgTracks query string that shows region and highlights the items called name,
    like the result of a search. hgFind.matches is a comma-separated list of item names. """
    return "%s&hgFind.matches=%s," % (region, urllib.parse.quote(name, safe=""))

def partnerField(query, region):
    """ value of the partnerPos field: the hgc "urls" code puts the part before | into $$ without
    encoding and shows the part after | as the link text """
    if not query:
        return ""
    return query + "|" + region

def mouseOver(name, side, gene5, gene3, pos5, pos3, frame, cancers, sampleCount, accCount, sources,
        partnerQuery):
    """ partnerQuery is the hgTracks query string that the link to the partner opens """
    partnerGene, partnerPos = (gene3, pos3) if side == "5'" else (gene5, pos5)
    if partnerPos and partnerQuery:
        partner = '<a href="hgTracks?position=%s">%s breakpoint at %s</a>' % \
            (partnerQuery.replace("&", "&amp;"), partnerGene, partnerPos)
    elif partnerPos:
        partner = "%s breakpoint at %s" % (partnerGene, partnerPos)
    else:
        partner = "%s breakpoint not on this assembly" % partnerGene
    lines = ["<b>%s</b>, %s partner" % (name, side), partner, "Frame: " + frame]
    if sampleCount:
        lines.append("TCGA samples: %d (%s)" % (sampleCount, ", ".join(sorted(cancers))))
    if accCount:
        lines.append("GenBank transcripts: %d" % accCount)
    lines.append("Sources: " + ", ".join(sorted(sources)))
    return "<br>".join(lines)

def main():
    infoFile, orfFile, db, outBed, outIx = sys.argv[1:]
    assert db in ("hg19", "hg38")
    events = parseInfo(infoFile)
    orfClasses, inFrameTx = parseOrf(orfFile)
    chromSizes = readChromSizes(db)

    bps = set()
    for key in events:
        g5, c5, p5, s5, g3, c3, p3, s3 = key
        for c, p in ((c5, p5), (c3, p3)):
            if c != "chr?":
                bps.add((c, p))

    if db == "hg38":
        mapping = liftBreakpoints(bps, chromSizes)
    else:
        # FusionGDB's chrMT is the rCRS mitochondrion, which hg19 also has as chrMT
        mapping = {bp: bp for bp in bps}

    eventIds = makeEventIds(events)
    frameCounts = Counter()
    rows = []
    ixNames = defaultdict(set)
    for key, ev in events.items():
        g5, c5, p5, s5, g3, c3, p3, s3 = key
        name = g5 + "::" + g3
        pos5 = mapping.get((c5, p5)) if c5 != "chr?" else None
        pos3 = mapping.get((c3, p3)) if c3 != "chr?" else None
        frame = summarizeFrame(orfClasses.get(key, set()))
        frameCounts[frame] += 1
        color = colors.get(frame, otherColor)
        cancers = ev["cancers"]
        samples = sorted(ev["samples"])
        accs = sorted(ev["accs"])
        for side, pos, strand, chromOrig in (("5'", pos5, s5, c5), ("3'", pos3, s3, c3)):
            if pos is None:
                if chromOrig == "chr?":
                    warn("breakpoint has no chromosome in FusionGDB (chr?), side not shown")
                continue
            chrom, bp = pos
            if bp > chromSizes[chrom]:
                print("breakpoint beyond chrom end: %s %s:%d" % (name, chrom, bp), file=sys.stderr)
                sys.exit(1)
            partnerBp = pos3 if side == "5'" else pos5
            partnerRegion = partnerWindow(partnerBp, chromSizes)
            query = ""
            if partnerBp:
                query = partnerQuery(partnerRegion, featureName(eventIds[key], otherSide(side)))
            mo = mouseOver(name, side, g5, g3, posStr(pos5), posStr(pos3), frame, cancers,
                len(samples), len(accs), ev["sources"], query)
            featName = featureName(eventIds[key], side)
            rows.append((chrom, bp-1, bp, featName, "0", strand, bp-1, bp, color,
                name, side, partnerField(query, partnerRegion), g5, posStr(pos5), g3, posStr(pos3), frame,
                ",".join(sorted(orfClasses.get(key, []))),
                ",".join(sorted(inFrameTx.get(key, []))),
                ",".join(sorted(ev["sources"])),
                ",".join(sorted(cancers)), str(len(samples)), ",".join(samples),
                ",".join(accs), mo))
            ixNames[featName].update([name, g5 + "-" + g3, g5 + "_" + g3])

    rows.sort(key=lambda r: (r[0], r[1], r[3]))
    with open(outBed, "w") as ofh:
        for r in rows:
            ofh.write("\t".join(str(x) for x in r) + "\n")
    with open(outIx, "w") as ofh:
        for name in sorted(ixNames):
            ofh.write(name + " " + " ".join(sorted(ixNames[name])) + "\n")

    print("%s: %d fusion events, %d features written" % (db, len(events), len(rows)), file=sys.stderr)
    for frame, count in frameCounts.most_common():
        print("  frame %s: %d events" % (frame, count), file=sys.stderr)
    for msg, count in warnCounts.most_common():
        print("  warning: %s: %d" % (msg, count), file=sys.stderr)

if __name__ == "__main__":
    main()

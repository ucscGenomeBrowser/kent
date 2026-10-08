#!/usr/bin/env python3
"""Convert FusionGDB fusion breakpoints to bigGenePred features: for each fusion
event, the part of a 5' partner transcript and of a 3' partner transcript that is kept
in the fusion transcript.

The 5' partner keeps its transcript from the transcription start to the breakpoint,
the 3' partner from the breakpoint to the transcription end. Breakpoints are 1-based
and are the last kept base of the 5' partner and the first kept base of the 3'
partner.

Transcripts are GENCODE V19, the gene set that FusionGDB used to annotate the reading
frame. For an event with FusionGDB frame annotation, we take the transcript pair with
the best frame (In-frame, then Frame-shift, then any other) and, among those, the
transcripts with the best GENCODE tags (appris_principal, CCDS, basic), then the
longest CDS and transcript. Events without frame annotation get the best transcript of
each partner gene that contains the breakpoint. If no transcript is found, the
breakpoint is written as a 1 bp non-coding feature.

The features are built on hg19. For hg38, the BED is converted with liftOver and a
feature is dropped if liftOver changes its number of exons.

usage: fusionGdbToBigGenePred.py infoFile orfFile gencodeDir db outBed
  gencodeDir has gencodeV19.gp, gencodeV19.attrs and gencodeV19.tags
"""
import sys, os, subprocess, tempfile
from collections import defaultdict, Counter

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from fusionGdbToBed import parseInfo, summarizeFrame, liftBreakpoints, readChromSizes, \
        posStr, colors, otherColor, partnerQuery, partnerField, makeEventIds, featureName

warnCounts = Counter()

frameRank = {"In-frame": 2, "Frame-shift": 1}
tagScores = {"appris_principal": 4, "CCDS": 2, "basic": 1}

class Transcript:
    def __init__(self, row):
        (self.id, self.chrom, self.strand, txStart, txEnd, cdsStart, cdsEnd, exonCount,
            exonStarts, exonEnds, score, self.gene, self.cdsStartStat, self.cdsEndStat,
            exonFrames) = row
        self.txStart, self.txEnd = int(txStart), int(txEnd)
        self.cdsStart, self.cdsEnd = int(cdsStart), int(cdsEnd)
        self.starts = [int(x) for x in exonStarts.rstrip(",").split(",")]
        self.ends = [int(x) for x in exonEnds.rstrip(",").split(",")]
        self.frames = [int(x) for x in exonFrames.rstrip(",").split(",")]
        self.tagScore = 0
        self.type = ""

    def contains(self, bp):
        " is the 1-based breakpoint inside the transcript? "
        return self.txStart < bp <= self.txEnd

    def rank(self):
        return (self.tagScore, self.cdsEnd - self.cdsStart, self.txEnd - self.txStart, self.id)

def readGencode(gencodeDir):
    """ return dict accession without version -> Transcript and dict gene -> list of Transcripts """
    txs = {}
    for line in open(os.path.join(gencodeDir, "gencodeV19.gp")):
        tx = Transcript(line.rstrip("\n").split("\t"))
        txs[tx.id.split(".")[0]] = tx
    for line in open(os.path.join(gencodeDir, "gencodeV19.attrs")):
        txId, gene, txType = line.rstrip("\n").split("\t")
        tx = txs.get(txId.split(".")[0])
        if tx:
            tx.type = txType
    for line in open(os.path.join(gencodeDir, "gencodeV19.tags")):
        txId, tag = line.rstrip("\n").split("\t")
        tx = txs.get(txId.split(".")[0])
        if tx:
            tx.tagScore += tagScores[tag]
    byGene = defaultdict(list)
    for tx in txs.values():
        byGene[tx.gene].append(tx)
    return txs, byGene

def parseOrfPairs(fname):
    " return dict breakpoint key -> list of (orfClass, tx5, tx3) "
    pairs = defaultdict(list)
    classes = defaultdict(set)
    for line in open(fname):
        row = line.rstrip("\r\n").split("\t")
        orfClass, tx5, tx3 = row[0], row[1], row[2]
        g5, c5, p5, s5, g3, c3, p3, s3 = row[8:16]
        key = (g5, c5, int(p5), s5, g3, c3, int(p3), s3)
        pairs[key].append((orfClass, tx5, tx3))
        classes[key].add(orfClass)
    return pairs, classes

def pickPair(key, pairs, txs):
    """ return (tx5, tx3) from FusionGDB's transcript pairs for this event, either can be None """
    g5, c5, p5, s5, g3, c3, p3, s3 = key
    best = None
    for orfClass, id5, id3 in pairs.get(key, []):
        tx5, tx3 = txs.get(id5), txs.get(id3)
        if tx5 is None or tx3 is None:
            warn("FusionGDB transcript not in GENCODE V19 table")
            continue
        if tx5.chrom != c5 or tx3.chrom != c3 or not tx5.contains(p5) or not tx3.contains(p3):
            continue
        rank = (frameRank.get(orfClass, 0), tx5.rank(), tx3.rank())
        if best is None or rank > best[0]:
            best = (rank, tx5, tx3)
    if best is None:
        return None, None
    return best[1], best[2]

# candidate GENCODE V19 symbols for each month that Excel produces, e.g. 1-Mar is MARCH1 or MARC1
excelMonths = {"Mar": ["MARCH", "MARC"], "Sep": ["SEPT"], "Dec": ["DEC"]}

def fixExcelGene(gene, chrom, bp, byGene):
    """ some ChiTaRS gene symbols in FusionGDB were converted to dates by Excel, e.g. 9-Sep for
    SEPT9, 6-Mar for MARCH6 or MARC6, 1-Dec for DEC1. Two-digit numbers became a date with a
    year: 1-Sep-04 is SEPT14, 1-Mar-01 is MARCH11. Return the GENCODE V19 symbol whose
    transcript contains the breakpoint, the first candidate if none does. """
    parts = gene.split("-")
    if len(parts) == 2 and parts[0].isdigit() and parts[1] in excelMonths:
        number = parts[0]
    elif len(parts) == 3 and parts[0].isdigit() and parts[1] in excelMonths and parts[2].isdigit():
        number = parts[0] + str(int(parts[2]))
    else:
        return gene
    cands = [prefix + number for prefix in excelMonths[parts[1]]]
    for cand in cands:
        if any(tx.chrom == chrom and tx.contains(bp) for tx in byGene.get(cand, [])):
            warn("Excel date gene symbol fixed")
            return cand
    warn("Excel date gene symbol fixed, but breakpoint not in a V19 transcript of the gene")
    return cands[0]

def fixExcelGenes(events, byGene):
    " return events with Excel-converted gene symbols fixed "
    fixed = {}
    for key, ev in events.items():
        g5, c5, p5, s5, g3, c3, p3, s3 = key
        new5, new3 = fixExcelGene(g5, c5, p5, byGene), fixExcelGene(g3, c3, p3, byGene)
        newKey = (new5, c5, p5, s5, new3, c3, p3, s3)
        assert newKey not in fixed, newKey
        fixed[newKey] = ev
    return fixed

def pickByGene(gene, chrom, bp, byGene):
    " return best transcript of gene on chrom that contains bp, or None "
    cands = [tx for tx in byGene.get(gene, []) if tx.chrom == chrom and tx.contains(bp)]
    if not cands:
        return None
    return max(cands, key=lambda tx: tx.rank())

def warn(key):
    warnCounts[key] += 1

def clipTranscript(tx, bp, side):
    """ return the part of tx kept in the fusion as (chromStart, chromEnd, thickStart, thickEnd,
    exon starts, exon ends, exon frames, cdsStartStat, cdsEndStat), or None if no exon is kept """
    keepLeft = (side == "5'") == (tx.strand == "+")
    if keepLeft:
        lo, hi = tx.txStart, bp
    else:
        lo, hi = bp - 1, tx.txEnd
    starts, ends, frames = [], [], []
    for s, e, f in zip(tx.starts, tx.ends, tx.frames):
        ns, ne = max(s, lo), min(e, hi)
        if ns >= ne:
            continue
        if f != -1:
            # the frame is that of the first coding base in transcription direction. It
            # changes only if the cut removes coding bases from that end of the exon.
            if tx.strand == "+":
                oldFirst = max(s, tx.cdsStart)
                if ns > oldFirst:
                    f = (f + ns - oldFirst) % 3
            else:
                oldFirst = min(e, tx.cdsEnd)
                if ne < oldFirst:
                    f = (f + oldFirst - ne) % 3
            # the exon may now have no coding bases at all
            if min(ne, tx.cdsEnd) <= max(ns, tx.cdsStart):
                f = -1
        starts.append(ns)
        ends.append(ne)
        frames.append(f)
    if not starts:
        return None
    chromStart, chromEnd = starts[0], ends[-1]
    thickStart, thickEnd = max(tx.cdsStart, chromStart), min(tx.cdsEnd, chromEnd)
    cdsStartStat, cdsEndStat = tx.cdsStartStat, tx.cdsEndStat
    if thickStart >= thickEnd:
        thickStart = thickEnd = chromStart
        cdsStartStat = cdsEndStat = "none"
    else:
        if thickStart > tx.cdsStart:
            cdsStartStat = "incmpl"
        if thickEnd < tx.cdsEnd:
            cdsEndStat = "incmpl"
    return chromStart, chromEnd, thickStart, thickEnd, starts, ends, frames, cdsStartStat, cdsEndStat

def bedRow(chrom, strand, clip, featName, fusion, color, gene, txId, txType, extra):
    " featName is the unique feature ID, fusion goes into geneName, which is the label "
    chromStart, chromEnd, thickStart, thickEnd, starts, ends, frames, css, ces = clip
    sizes = ",".join(str(e - s) for s, e in zip(starts, ends))
    relStarts = ",".join(str(s - chromStart) for s in starts)
    return [chrom, chromStart, chromEnd, featName, 0, strand, thickStart, thickEnd, color,
        len(starts), sizes, relStarts,
        gene, css, ces, ",".join(str(f) for f in frames),
        txType, fusion, txId, ""] + extra

def liftBed(inBed, outBed):
    """ liftOver a bigGenePred BED from hg19 to hg38, drop features whose exon count changes """
    tmpOut = outBed + ".lift.tmp"
    unmapped = outBed + ".unmapped"
    subprocess.run(["liftOver", "-tab", "-bedPlus=12", inBed,
        "/gbdb/hg19/liftOver/hg19ToHg38.over.chain.gz", tmpOut, unmapped], check=True,
        stderr=subprocess.DEVNULL)
    unmappedCount = sum(1 for l in open(unmapped) if not l.startswith("#"))
    for i in range(unmappedCount):
        warn("feature could not be lifted from hg19 to hg38")
    rows = []
    for line in open(tmpOut):
        row = line.rstrip("\n").split("\t")
        if len(row[15].rstrip(",").split(",")) != int(row[9]):
            warn("feature dropped on hg38, liftOver changed the number of exons")
            continue
        rows.append(row)
    os.remove(tmpOut)
    return rows

sideCol, partnerCol, moCol = 20, 21, 34

def addPartnerLinks(rows, eventInfo):
    """ fill in the partnerPos field, the region of the other feature of the same event, and
    the mouseover, which links to it. Remove the temporary event index column. """
    byEvent = defaultdict(list)
    for r in rows:
        byEvent[int(r[-1])].append(r)
    for eventIdx, evRows in byEvent.items():
        name, g5, g3, pos5, pos3, frame, cancers, sampleCount, accCount, sources, orientation = \
            eventInfo[eventIdx]
        for r in evRows:
            side = r[sideCol]
            gene, partnerGene = (g5, g3) if side == "5'" else (g3, g5)
            partnerSide = "3'" if side == "5'" else "5'"
            partnerPos = pos3 if side == "5'" else pos5
            others = [o for o in evRows if o is not r]
            partnerText = "%s (%s partner)" % (partnerGene, partnerSide)
            loc = ""
            if partnerPos:
                chrom, bp = partnerPos.split(":")
                loc = "%s %.1f Mb" % (chrom, int(bp) / 1e6)
            query = ""
            if others:
                o = others[0]
                region = "%s:%d-%d" % (o[0], int(o[1]) + 1, int(o[2]))
                query = partnerQuery(region, o[3])
            else:
                warn("partner feature not on this assembly, no link")
            # no commas in partnerPos, hgc splits a urls field on them
            r[partnerCol] = partnerField(query, (partnerText + " " + loc).strip())
            r[moCol] = txMouseOver(name, gene, side, partnerText, loc, query, frame, cancers,
                sampleCount, accCount, sources, orientation)
            del r[-1]

def txMouseOver(name, gene, side, partnerText, loc, query, frame, cancers, sampleCount, accCount,
        sources, orientation):
    """ mouseover: the fusion, which part this is, a link to the fusion partner """
    lines = ["<b>%s</b>, %s part (%s partner)" % (name, gene, side)]
    if query:
        lines.append('<a href="hgTracks?position=%s">Fused to: %s, %s</a>' %
            (query.replace("&", "&amp;"), partnerText, loc))
    else:
        lines.append("Fused to: %s, not on this assembly" % partnerText)
    lines.append("Frame: " + frame)
    if sampleCount:
        lines.append("TCGA samples: %d (%s)" % (sampleCount, ", ".join(sorted(cancers))))
    if accCount:
        lines.append("GenBank transcripts: %d" % accCount)
    lines.append("Sources: " + ", ".join(sorted(sources)))
    if orientation:
        lines.append(orientation)
    return "<br>".join(lines)

def main():
    infoFile, orfFile, gencodeDir, db, outBed = sys.argv[1:]
    assert db in ("hg19", "hg38")
    events = parseInfo(infoFile)
    pairs, orfClasses = parseOrfPairs(orfFile)
    txs, byGene = readGencode(gencodeDir)
    events = fixExcelGenes(events, byGene)
    chromSizes = readChromSizes(db)

    # partner positions shown on the details page are on the target assembly
    bps = set()
    for g5, c5, p5, s5, g3, c3, p3, s3 in events:
        for c, p in ((c5, p5), (c3, p3)):
            if c != "chr?":
                bps.add((c, p))
    if db == "hg38":
        mapping = liftBreakpoints(bps, chromSizes)
    else:
        mapping = {bp: bp for bp in bps}

    eventIds = makeEventIds(events)
    txSource = Counter()
    rows = []
    # mouseover arguments of each event. The mouseover and the link to the partner feature
    # are filled in at the end, when the partner's coordinates on db are known.
    eventInfo = []
    for key, ev in events.items():
        g5, c5, p5, s5, g3, c3, p3, s3 = key
        name = g5 + "::" + g3
        frame = summarizeFrame(orfClasses.get(key, set()))
        color = colors.get(frame, otherColor)
        pos5 = mapping.get((c5, p5)) if c5 != "chr?" else None
        pos3 = mapping.get((c3, p3)) if c3 != "chr?" else None
        tx5, tx3 = pickPair(key, pairs, txs)
        pairSource = "FusionGDB transcript pair" if tx5 else None
        cancers = ev["cancers"]
        samples = sorted(ev["samples"])
        accs = sorted(ev["accs"])
        src5 = src3 = pairSource
        if tx5 is None and c5 != "chr?":
            tx5 = pickByGene(g5, c5, p5, byGene)
            src5 = "GENCODE transcript of the gene" if tx5 else None
        if tx3 is None and c3 != "chr?":
            tx3 = pickByGene(g3, c3, p3, byGene)
            src3 = "GENCODE transcript of the gene" if tx3 else None
        # FusionGDB strands are those of the fusion sequence. Many ChiTaRS GenBank
        # sequences are reverse-complemented, then both genes are on the opposite strand
        # and the sequence reads 3' partner first: draw the roles swapped.
        role5, role3 = "5'", "3'"
        orientation = ""
        flip5 = tx5 is not None and tx5.strand != s5
        flip3 = tx3 is not None and tx3.strand != s3
        if flip5 and flip3:
            role5, role3 = "3'", "5'"
            orientation = "Both genes are antisense to the fusion sequence, drawn as %s::%s" % (g3, g5)
            warn("both genes antisense to the fusion sequence, roles swapped")
        elif flip5 or flip3:
            orientation = "%s is antisense to the fusion sequence" % (g5 if flip5 else g3)
            warn("one gene antisense to the fusion sequence, drawn as given by FusionGDB")
        eventIdx = len(eventInfo)
        eventInfo.append((name, g5, g3, posStr(pos5), posStr(pos3), frame, cancers,
            len(samples), len(accs), ev["sources"], orientation))
        for side, gene, chrom, bp, strand, tx, src, role in (
                ("5'", g5, c5, p5, s5, tx5, src5, role5), ("3'", g3, c3, p3, s3, tx3, src3, role3)):
            if chrom == "chr?":
                warn("breakpoint has no chromosome in FusionGDB (chr?), side not shown")
                continue
            clip = clipTranscript(tx, bp, role) if tx else None
            if clip is None:
                src = "breakpoint only"
                clip = (bp-1, bp, bp-1, bp-1, [bp-1], [bp], [-1], "none", "none")
                txId, txType, txStrand = "", "", strand
            else:
                txId, txType, txStrand = tx.id, tx.type, tx.strand
            txSource[src] += 1
            # partnerPos and mouseover are placeholders, the last column is temporary
            extra = [side, "", g5, posStr(pos5), g3, posStr(pos3), frame, orientation,
                ",".join(sorted(orfClasses.get(key, []))),
                ",".join(sorted(ev["sources"])),
                ",".join(sorted(cancers)), len(samples), ",".join(samples),
                ",".join(accs), "", eventIdx]
            rows.append(bedRow(chrom, txStrand, clip, featureName(eventIds[key], side), name, color,
                gene, txId, txType, extra))

    if db == "hg38":
        hg19Bed = outBed + ".hg19.tmp"
        # chrMT is rCRS on hg19, the same sequence as hg38 chrM, so it is not lifted
        mitoRows = [r for r in rows if r[0] == "chrMT"]
        with open(hg19Bed, "w") as ofh:
            for r in rows:
                if r[0] != "chrMT":
                    ofh.write("\t".join(str(x) for x in r) + "\n")
        lifted = liftBed(hg19Bed, outBed)
        os.remove(hg19Bed)
        rows = []
        for r in mitoRows:
            r[0] = "chrM"
            rows.append(r)
        for r in lifted:
            if r[0] not in chromSizes:
                warn("feature lifted to a sequence not in hg38 chrom.sizes")
                continue
            rows.append(r)

    addPartnerLinks(rows, eventInfo)
    rows.sort(key=lambda r: (r[0], int(r[1]), r[3]))
    with open(outBed, "w") as ofh:
        for r in rows:
            ofh.write("\t".join(str(x) for x in r) + "\n")

    for r in rows:
        if int(r[2]) > chromSizes[r[0]]:
            print("feature beyond chrom end: %s %s:%s" % (r[3], r[0], r[2]), file=sys.stderr)
            sys.exit(1)

    print("%s: %d fusion events, %d features written" % (db, len(events), len(rows)), file=sys.stderr)
    for src, count in txSource.most_common():
        print("  partner drawn from %s: %d" % (src, count), file=sys.stderr)
    for msg, count in warnCounts.most_common():
        print("  warning: %s: %d" % (msg, count), file=sys.stderr)

if __name__ == "__main__":
    main()

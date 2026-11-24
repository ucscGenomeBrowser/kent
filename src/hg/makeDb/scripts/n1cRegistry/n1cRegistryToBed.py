#!/usr/bin/env python3
"""Convert the N=1 Collaborative (N1C) Gene Registry JSON tables to three hg38 BED
files for bigBed tracks:

  n1cVariants.bed   variants assessed for eligibility for an antisense oligonucleotide
                    (ASO) or other genetic therapy, bed9+20, colored by eligibility
  n1cProjects.bed   N-of-1 therapy development projects, bed9+24, colored by status
  n1cMarketed.bed   marketed genetic therapies, one feature per drug and target gene,
                    placed on the MANE Select transcript of the gene, bed9+11, colored
                    by modality

Where a feature is placed, in this order:
  1. the genomic HGVS (NC_ accession, GRCh38), e.g. NC_000002.12:g.71600804dup.
     The feature covers the bases named in the HGVS; an insertion covers the two
     flanking bases.
  2. a genomic variant in SPDI notation, NC_:pos:deleted:inserted, pos is 0-based
  3. the RefSeq transcript plus coding HGVS, mapped with the kent tool hgvsToVcf.
     A repeat expansion such as c.88CAG[>=37] is placed on its first repeat unit.
  4. for projects: a genomic range "start-end" in the Genomic Position column
  5. for projects and marketed drugs: the MANE Select transcript span of the gene

The registry tables are typed in by hand, so the categorical columns have many
spellings of the same value. Each one goes through a normalization table below; a
value that is not in the table stops the script, so a new spelling is noticed.
Two checks, reported as warnings: the reference base of each substitution in a genomic
HGVS must match hg38, and where a row has both a genomic HGVS and a transcript + coding
HGVS, hgvsToVcf must place the coding HGVS within 12 bp of the genomic one (the margin
allows for left- vs right-shifted indels). The registry's separate Chromosome / Genomic
Position / Reference Allele columns are not used for variants: they contain hg19
positions, strand-flipped alleles and typos, while the HGVS is consistent.

usage: n1cRegistryToBed.py inDir outDir
  inDir has assessed_variants.json, N1C_projects.json and marketed_drugs.json
"""
import sys, os, re, json, subprocess, tempfile, datetime, collections

DB = "hg38"
TWOBIT = "/hive/data/genomes/hg38/hg38.2bit"
MANE = "/gbdb/hg38/mane/mane.bb"

warnings = collections.Counter()
def warn(msg):
    " print a warning and count it by its first words "
    print("warning: " + msg, file=sys.stderr)
    warnings[" ".join(msg.split()[:3])] += 1

def clean(val):
    " registry value as one line of text: float IDs to int, whitespace runs to one space "
    if val is None:
        return ""
    if isinstance(val, float) and val.is_integer():
        val = int(val)
    s = str(val).replace("\t", " ")
    s = s.replace("™", "")  # the trademark sign in some drug names
    return re.sub(r"\s+", " ", s).strip()

def norm(table, val, field):
    " look up a normalized value, keys are lowercase; stop on an unknown spelling "
    key = clean(val).lower()
    if key not in table:
        sys.exit("unknown value %r in field %r, add it to the normalization table" % (val, field))
    return table[key]

# ---- normalization tables, key: lowercase value as in the registry -----------------

ELIGIBILITY = {
    "eligible": "Eligible", "eligible for knockdown": "Eligible",
    "likely eligible": "Likely eligible", "likely eligibe": "Likely eligible",
    "unlikely eligible": "Unlikely eligible",
    "not eligible": "Not eligible", "not eligiblle": "Not eligible",
    "noteligible": "Not eligible", "not eligoble": "Not eligible",
    "unable to assess": "Unable to assess", "unable to asssess": "Unable to assess",
    # one variant could not be assessed for one approach and was not eligible for another
    "unable to assess/not eligible": "Unable to assess",
}
ELIG_COLORS = collections.OrderedDict([
    ("Eligible", "0,107,27"),
    ("Likely eligible", "0,119,158"),
    ("Unlikely eligible", "224,75,0"),
    ("Not eligible", "216,38,38"),
    ("Unable to assess", "120,120,120"),
])

APPROACH = {
    "": "",
    "exon skipping": "Exon skipping",
    "splice correction": "Splice correction",
    "splice correction/exon skipping": "Splice correction or exon skipping",
    "exon skipping/splice correction": "Splice correction or exon skipping",
    "knockdown": "Knockdown",
    "knockdown (allele-selective)": "Knockdown (allele-selective)",
    "knockdown (allele-selective, variant)": "Knockdown (allele-selective)",
    "exon inclusion": "Exon inclusion",
    "upregulation": "Upregulation",
    "gene replacement": "Gene replacement",
    "unable to assess": "Unable to assess",
    "other": "Other",
}

MODALITY = {
    "": "",
    "antisense oligonucleotide": "Antisense oligonucleotide",
    "gene replacement": "Gene replacement",
    "gene editing": "Gene editing",
    "other": "Other",
}

ASSESSOR = {
    "n=1 collaborative": "N=1 Collaborative",
    "upnat": "UPNAT",
    "yu lab": "Yu Lab",
    "genesteps": "GeneSteps",
}

PATHOMECH = {
    "": "",
    "loss of function": "Loss of function", "loss-of-function": "Loss of function",
    "lof": "Loss of function",
    "gain of function": "Gain of function", "gain-of-function": "Gain of function",
    "gof": "Gain of function",
    "dominant negative": "Dominant negative", "dominnat negative": "Dominant negative",
    "dn": "Dominant negative",
    "gof/dominant negative": "Gain of function or dominant negative",
    "mixed dominant negative and gain of function": "Gain of function or dominant negative",
    "mixed gain-/loss of function": "Mixed gain and loss of function",
    "unknown": "Unknown",
    "unknown (likely lof by phenotype)": "Unknown, likely loss of function",
    "unknown (likely gof by phenotype)": "Unknown, likely gain of function",
}

SPLICING = {"": "", "yes": "Yes", "no": "No", "non": "No", "unknown": "Unknown"}

INHERITANCE = {
    "": "",
    "autosomal recessive": "Autosomal recessive",
    "autosomal dominant": "Autosomal dominant",
    "x-linked dominant": "X-linked dominant",
    "x-linked recessive": "X-linked recessive",
    "x linked": "X-linked",
}

STATUS_COLORS = collections.OrderedDict([
    ("Clinical Application", "0,107,27"),
    ("Developed", "46,0,184"),
    ("Under Development", "0,119,158"),
    ("Currently Not Pursued for Development", "120,120,120"),
    ("Other", "120,120,120"),
])
STATUS = {k.lower(): k for k in STATUS_COLORS}

MARKET_MODALITY_COLORS = collections.OrderedDict([
    ("RNA therapy", "189,0,157"),
    ("Gene therapy", "0,0,255"),
    ("Cell/Gene therapy", "224,75,0"),
    ("Cell therapy", "153,38,0"),
    ("Other", "120,120,120"),
])
MARKET_MODALITY = {k.lower(): k for k in MARKET_MODALITY_COLORS}

# Corrections to the marketed drugs table. Applied only while the registry still has
# the wrong value, so a corrected table passes through unchanged. Remove an entry when
# the registry is fixed.
# Upstaza (eladocagene exuparvovec) treats AADC deficiency, which is caused by
# variants in DDC, not DCC. The registry lists it under DCC. Found by Claude/max,
# #38853.
MARKETED_GENE_FIX = {("Upstaza", "DCC"): "DDC"}

# ---- helpers ---------------------------------------------------------------------

def ncToChrom(acc):
    " NC_000023.11 -> chrX "
    num = int(acc.split(".")[0].replace("NC_", ""))
    return "chr" + {23: "X", 24: "Y"}.get(num, str(num))

def chromName(val):
    " registry chromosome column (7, '7', 'chr7', 'X') -> chr7 "
    c = clean(val)
    if not c:
        return ""
    return c if c.startswith("chr") else "chr" + c

def normDate(val):
    " registry dates come as 'Wed, 08 Jan 2025 00:00:00 GMT' or dd/mm/yyyy -> yyyy-mm-dd "
    s = clean(val)
    if not s:
        return ""
    try:
        return datetime.datetime.strptime(s, "%a, %d %b %Y %H:%M:%S GMT").strftime("%Y-%m-%d")
    except ValueError:
        pass
    m = re.match(r"^(\d{1,2})/(\d{1,2})/?(\d{4})$", s)  # 13/072026 has a missing slash
    if m and 1990 < int(m.group(3)) < 2100:
        return "%s-%02d-%02d" % (m.group(3), int(m.group(2)), int(m.group(1)))
    warn("cannot parse date %r, kept as is" % s)
    return s

def normMondo(val):
    " MONDO_0016145, MONDO:0016145 or a purl -> MONDO:0016145 "
    m = re.search(r"MONDO[_:](\d+)", clean(val))
    return "MONDO:" + m.group(1) if m else ""

def clinvarId(val):
    m = re.search(r"clinvar/variation/(\d+)", clean(val))
    return m.group(1) if m else ""

def pubLinks(*vals):
    """ publication URLs -> 'url|label' list for a trackDb urls field, comma-separated.
    Commas inside a URL would split the link, so they are encoded. """
    out = []
    for val in vals:
        for url in re.findall(r"https?://\S+", clean(val)):
            url = url.rstrip(";,.")
            m = re.match(r"https?://pubmed\.ncbi\.nlm\.nih\.gov/(\d+)", url)
            mc = re.search(r"/(PMC\d+)", url)
            if m:
                label = "PMID " + m.group(1)
            elif mc:
                label = mc.group(1)
            else:
                label = re.sub(r"https?://(www\.)?([^/]+).*", r"\2", url)
            item = url.replace(",", "%2C") + "|" + label
            if item not in out:
                out.append(item)
    return ",".join(out)

def htmlEsc(s):
    return s.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")

def readMane():
    " gene symbol -> (chrom, start, end, strand) of its MANE Select transcript "
    out = subprocess.run(["bigBedToBed", MANE, "stdout"], check=True,
                         capture_output=True, text=True).stdout
    genes = {}
    for line in out.splitlines():
        f = line.split("\t")
        sym, stat = f[18], f[24]
        if stat == "MANE Select" or sym not in genes:
            genes[sym] = (f[0], int(f[1]), int(f[2]), f[5])
    return genes

def genomeSeqs(regions):
    " list of (chrom, start, end) -> dict of (chrom, start, end) -> upper case sequence "
    if not regions:
        return {}
    with tempfile.NamedTemporaryFile("w", suffix=".bed", delete=False) as bed:
        for i, (c, s, e) in enumerate(regions):
            bed.write("%s\t%d\t%d\tr%d\n" % (c, s, e, i))
    fa = subprocess.run(["twoBitToFa", "-bed=" + bed.name, TWOBIT, "stdout"], check=True,
                        capture_output=True, text=True).stdout
    os.unlink(bed.name)
    seqs, name = {}, None
    for line in fa.splitlines():
        if line.startswith(">"):
            name = int(line[2:])
            seqs[name] = ""
        else:
            seqs[name] += line.strip().upper()
    return {regions[i]: s for i, s in seqs.items()}

def mapTranscriptHgvs(hgvsList):
    """ transcript HGVS -> (chrom, start, end) with hgvsToVcf. Returns a dict with the
    HGVS terms that could be mapped. """
    if not hgvsList:
        return {}
    with tempfile.TemporaryDirectory() as tmp:
        inF, outF = tmp + "/in.txt", tmp + "/out.vcf"
        with open(inF, "w") as fh:
            fh.write("\n".join(hgvsList) + "\n")
        subprocess.run(["hgvsToVcf", DB, inF, outF], check=True, capture_output=True)
        res = {}
        for line in open(outF):
            if line.startswith("#"):
                continue
            f = line.rstrip("\n").split("\t")
            chrom, pos, hgvs, ref, alt, info = f[0], int(f[1]), f[2], f[3], f[4], f[7]
            if len(ref) > 1 and len(alt) >= 1 and ref[0] == alt[0]:
                # deletion or delins after a padding base: the deleted bases
                res[hgvs] = (chrom, pos, pos - 1 + len(ref))
            elif len(ref) == 1 and len(alt) > 1:
                # insertion (or duplication written as insertion) after the base at pos
                res[hgvs] = (chrom, pos - 1, pos)
            else:
                res[hgvs] = (chrom, pos - 1, pos - 1 + len(ref))
        return res

GENOMIC_HGVS = re.compile(r"^(NC_\d+\.\d+):g\.(\d+)(?:_(\d+))?(.*)$")
SPDI_HGVS = re.compile(r"^(NC_\d+\.\d+):(\d+):([ACGTN]*):([ACGTN]*)$")
REPEAT_C = re.compile(r"^c\.(\d+)([ACGT]+)\[")

def placeGenomic(hgvsG):
    """ genomic HGVS -> (chrom, start, end, refBasesOrEmpty) or None.
    Insertions are placed on their two flanking bases. """
    m = GENOMIC_HGVS.match(hgvsG)
    if m:
        chrom = ncToChrom(m.group(1))
        a = int(m.group(2))
        b = int(m.group(3)) if m.group(3) else a
        rest = m.group(4)
        ref = ""
        sm = re.match(r"^([ACGT])>[ACGT]$", rest)
        if sm:
            ref = sm.group(1)
        return chrom, a - 1, b, ref
    m = SPDI_HGVS.match(hgvsG)
    if m:
        # SPDI: the position is 0-based, the first deleted base follows it
        chrom, pos, ref = ncToChrom(m.group(1)), int(m.group(2)), m.group(3)
        return chrom, pos, pos + max(len(ref), 1), ref
    return None

def transcriptHgvs(transcript, hgvsC):
    """ RefSeq transcript + c. term -> one HGVS term that hgvsToVcf understands, or ''.
    Repeat expansions (c.88CAG[>=37]) become a duplication of the first repeat unit, which
    only serves to find the position. """
    if not transcript.startswith("NM_") or not hgvsC.startswith("c."):
        return ""
    c = hgvsC.split()[0]
    m = REPEAT_C.match(c)
    if m:
        start = int(m.group(1))
        c = "c.%d_%ddup" % (start, start + len(m.group(2)) - 1)
    c = re.sub(r"dup[ACGT]+$", "dup", c)  # c.696dupG -> c.696dup
    return transcript + ":" + c

def registryRows(fname, keyField="Gene"):
    " rows of a registry table, without the blank spreadsheet lines "
    rows = json.load(open(fname))
    keep = [r for r in rows if clean(r.get(keyField)) or clean(r.get("Genomic HGVS"))]
    print("%s: %d rows, %d not blank" % (os.path.basename(fname), len(rows), len(keep)))
    return keep

def placeRows(rows, maneGenes, allowRange, allowGene, label):
    """ find coordinates for registry rows (variants or projects). Returns a list of
    (row, chrom, start, end, how) and reports rows that could not be placed. """
    placed, needTx, refChecks = [], {}, []
    for r in rows:
        hgvsG = clean(r.get("Genomic HGVS"))
        pos = placeGenomic(hgvsG) if hgvsG else None
        if pos:
            chrom, start, end, ref = pos
            placed.append([r, chrom, start, end, "genomic HGVS"])
            if ref:
                refChecks.append((chrom, start, start + len(ref), ref, hgvsG))
            continue
        tx = transcriptHgvs(clean(r.get("RefSeq Transcript")), clean(r.get("Coding DNA change (c.)")))
        if tx:
            needTx[id(r)] = tx
            placed.append([r, None, None, None, "transcript HGVS"])
            continue
        gpos = clean(r.get("Genomic Position"))
        rm = re.match(r"^(\d+)-(\d+)$", gpos)
        if allowRange and rm and chromName(r.get("Chromosome")):
            a, b = sorted([int(rm.group(1)), int(rm.group(2))])
            placed.append([r, chromName(r.get("Chromosome")), a - 1, b, "genomic range"])
            continue
        gene = clean(r.get("Gene"))
        if allowGene and gene in maneGenes:
            c, s, e, strand = maneGenes[gene]
            placed.append([r, c, s, e, "gene"])
            continue
        placed.append([r, None, None, None, "none"])

    txPos = mapTranscriptHgvs(sorted(set(needTx.values())))
    out = []
    for p in placed:
        r = p[0]
        if p[4] == "transcript HGVS":
            tx = needTx[id(r)]
            if tx in txPos:
                p[1], p[2], p[3] = txPos[tx]
            else:
                warn("%s %s: transcript HGVS %s could not be mapped, skipped" % (label, clean(r.get("ID")), tx))
                continue
        if p[4] == "none":
            warn("%s %s (%s): no genomic position, transcript HGVS or gene, skipped" %
                 (label, clean(r.get("ID")), clean(r.get("Gene"))))
            continue
        out.append(p)

    # cross-check: the transcript HGVS must map to the same place as the genomic HGVS
    cross = {}
    for r, chrom, start, end, how in out:
        if how == "genomic HGVS":
            tx = transcriptHgvs(clean(r.get("RefSeq Transcript")), clean(r.get("Coding DNA change (c.)")))
            if tx and not REPEAT_C.match(clean(r.get("Coding DNA change (c.)"))):
                cross[id(r)] = (tx, chrom, start, clean(r.get("ID")))
    crossPos = mapTranscriptHgvs(sorted(set(v[0] for v in cross.values())))
    nBad = 0
    for tx, chrom, start, rid in cross.values():
        if tx not in crossPos:
            warn("%s %s: transcript HGVS %s could not be mapped for the cross-check" % (label, rid, tx))
            continue
        c, s, e = crossPos[tx]
        if c != chrom or abs(s - start) > 12:
            nBad += 1
            warn("%s %s: transcript HGVS %s maps to %s:%d, genomic HGVS to %s:%d" % (label, rid, tx, c, s + 1, chrom, start + 1))
    print("%s: %d genomic HGVS cross-checked with the transcript HGVS, %d disagree" % (label, len(cross), nBad))

    # reference base of the substitutions in the genomic HGVS
    seqs = genomeSeqs(sorted(set((c, s, e) for c, s, e, ref, desc in refChecks)))
    bad = 0
    for c, s, e, ref, desc in refChecks:
        if seqs.get((c, s, e)) != ref:
            bad += 1
            warn("%s: reference allele %s does not match hg38 (%s) at %s" % (label, ref, seqs.get((c, s, e)), desc))
    print("%s: %d reference alleles checked, %d mismatches" % (label, len(refChecks), bad))
    return out

def bedLine(chrom, start, end, name, strand, color, extra):
    if end <= start:
        end = start + 1
    f = [chrom, str(start), str(end), name, "0", strand, str(start), str(end), color] + extra
    for x in f:
        assert "\t" not in x and "\n" not in x, x
    return "\t".join(f)

def writeBed(fname, lines):
    lines.sort(key=lambda l: (l.split("\t")[0], int(l.split("\t")[1])))
    with open(fname, "w") as fh:
        for l in lines:
            fh.write(l + "\n")
    print("wrote %d features to %s" % (len(lines), fname))

# ---- the three tables ------------------------------------------------------------

def variants(inDir, outDir, maneGenes):
    rows = registryRows(os.path.join(inDir, "assessed_variants.json"))
    lines = []
    for r, chrom, start, end, how in placeRows(rows, maneGenes, False, False, "variant"):
        gene = clean(r["Gene"])
        hgvsC = clean(r["Coding DNA change (c.)"])
        elig = norm(ELIGIBILITY, r["Eligibility"], "Eligibility")
        approach = norm(APPROACH, r["Approach"], "Approach")
        disease = clean(r["Disease"])
        assessor = norm(ASSESSOR, r["Submitter/Assessor"], "Submitter/Assessor")
        hgvsP = clean(r["Predicted protein change (p.)"])
        name = (gene + " " + hgvsC).strip()
        mouseOver = "<b>%s</b> %s %s<br>Eligibility: %s%s<br>%s<br>Assessed by %s" % (
            htmlEsc(gene), htmlEsc(hgvsC), htmlEsc(hgvsP), elig,
            " (" + approach + ")" if approach else "", htmlEsc(disease), assessor)
        extra = [
            gene, clean(r["RefSeq Transcript"]), hgvsC, hgvsP,
            clean(r["Confirmed protein or RNA change"]), clean(r["Genomic HGVS"]), how,
            disease, normMondo(r["Mondo"]),
            norm(INHERITANCE, r["Inheritance"], "Inheritance"),
            norm(PATHOMECH, r["Pathomechanism"], "Pathomechanism"),
            norm(SPLICING, r["Splicing effect?"], "Splicing effect?"),
            elig, approach,
            norm(MODALITY, r["Therapeutic Modality"], "Therapeutic Modality"),
            assessor, normDate(r["Date assessment"]), clean(r["Assessment text"]),
            clinvarId(r["Clinvar "]),
            pubLinks(r["Variant publication"], r["Publication"]),
            clean(r["ID"]), mouseOver,
        ]
        lines.append(bedLine(chrom, start, end, name, ".", ELIG_COLORS[elig], extra))
    writeBed(os.path.join(outDir, "n1cVariants.bed"), lines)

def projects(inDir, outDir, maneGenes):
    rows = registryRows(os.path.join(inDir, "N1C_projects.json"))
    lines = []
    for r, chrom, start, end, how in placeRows(rows, maneGenes, True, True, "project"):
        gene = clean(r["Gene"])
        hgvsC = clean(r["Coding DNA change (c.)"])
        status = norm(STATUS, r["Status"], "Status")
        approach = norm(APPROACH, r["Approach"], "Approach")
        disease = clean(r["Disease"])
        inst = clean(r["Institution"])
        name = gene + (" " + hgvsC if hgvsC.startswith("c.") else "")
        mouseOver = "<b>%s</b> %s<br>%s<br>Status: %s%s<br>%s" % (
            htmlEsc(gene), htmlEsc(hgvsC if hgvsC.startswith("c.") else ""), htmlEsc(disease),
            status, "<br>Approach: " + approach if approach else "", htmlEsc(inst))
        if how == "gene":
            mouseOver += "<br>(placed on the whole gene)"
        extra = [
            gene, clean(r["RefSeq Transcript"]), hgvsC,
            clean(r["Predicted protein change (p.)"]),
            clean(r["Confirmed protein or RNA change (for splicing variants)"]),
            clean(r["Genomic HGVS"]), how, disease, normMondo(r["Mondo"]),
            status, approach,
            norm(MODALITY, r["Therapeutic Modality"], "Therapeutic Modality"),
            clean(r["Indication "]), inst, clean(r["Contact Person"]),
            clean(r["Contact Information (email)"]),
            clean(r["Cell Line or Animal Model Availability"]),
            clean(r["IND available?"]), clean(r["Information on IND"]),
            pubLinks(r["Variant Publication"]), pubLinks(r["Therapy Publication"]),
            clean(r["Comment"]), normDate(r["Current Date"]), clean(r["ID"]), mouseOver,
        ]
        lines.append(bedLine(chrom, start, end, name, ".", STATUS_COLORS[status], extra))
    writeBed(os.path.join(outDir, "n1cProjects.bed"), lines)

def marketed(inDir, outDir, maneGenes):
    rows = json.load(open(os.path.join(inDir, "marketed_drugs.json")))
    print("marketed_drugs.json: %d rows" % len(rows))
    lines, noGene = [], 0
    for r in rows:
        drug = clean(r["Marketed drug"])
        gene = clean(r["Gene"])
        for (fixDrug, wrongGene), rightGene in MARKETED_GENE_FIX.items():
            if drug.startswith(fixDrug) and gene == wrongGene:
                print("marketed: %s: gene %s corrected to %s" % (drug, gene, rightGene))
                gene = rightGene
        if not gene:
            noGene += 1
            continue
        if gene not in maneGenes:
            warn("marketed %s: gene %s not in MANE, skipped" % (drug, gene))
            continue
        chrom, start, end, strand = maneGenes[gene]
        modality = norm(MARKET_MODALITY, r["Modality"] or "Other", "Modality")
        generic = clean(r["Generic Name"])
        cond = clean(r["Condition/indication"])
        mouseOver = "<b>%s</b>%s<br>%s<br>Target gene: %s<br>%s" % (
            htmlEsc(drug), " (" + htmlEsc(generic) + ")" if generic else "",
            modality + (", " + htmlEsc(clean(r["Type"])) if clean(r["Type"]) else ""),
            gene, htmlEsc(cond))
        extra = [
            gene, generic, clean(r["Company"]), clean(r["Market"]), modality,
            clean(r["Type"]), clean(r["Mechanism"]), cond, clean(r["DrugBank ID"]),
            "; ".join(x for x in [clean(r["Additional Information"]), clean(r["Comment"])] if x),
            mouseOver,
        ]
        lines.append(bedLine(chrom, start, end, drug, strand, MARKET_MODALITY_COLORS[modality], extra))
    print("marketed: %d rows have no target gene and were not placed" % noGene)
    writeBed(os.path.join(outDir, "n1cMarketed.bed"), lines)

def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    inDir, outDir = sys.argv[1:]
    maneGenes = readMane()
    variants(inDir, outDir, maneGenes)
    projects(inDir, outDir, maneGenes)
    marketed(inDir, outDir, maneGenes)
    if warnings:
        print("warning summary:", file=sys.stderr)
        for w, n in warnings.most_common():
            print("  %s ...: %d" % (w, n), file=sys.stderr)

main()

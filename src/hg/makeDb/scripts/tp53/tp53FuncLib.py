"""
Shared helpers for the TP53 VCEP hub build scripts.

All TP53 tracks use the same canonical transcript (NM_000546.6, chr17 minus
strand, 393 aa) and share the amino-acid-to-genomic mapping logic. Centralizing
it here keeps the minus-strand handling consistent across tracks, avoiding the
off-by-one class of bugs that bit InSiGHT's PMS2 before being fixed.

Canonical transcript: NM_000546.6 / NP_000537.3 (MANE Select)
"""

import subprocess

TRANSCRIPT = "NM_000546.6"
PROTEIN = "NP_000537.3"
GENE = "TP53"


def bash(cmd):
    """Run cmd in bash subprocess, return stdout; raise on non-zero exit."""
    try:
        out = subprocess.run(
            cmd, check=True, shell=True,
            stdout=subprocess.PIPE, universal_newlines=True,
            stderr=subprocess.STDOUT,
        )
        return out.stdout
    except subprocess.CalledProcessError as e:
        raise RuntimeError(
            "command '{}' returned error (code {}): {}".format(
                e.cmd, e.returncode, e.output)
        )


def get_transcript_info(db, accession=TRANSCRIPT):
    """Query hgsql ncbiRefSeq and return a dict with tx/cds/exon info."""
    query = (
        "SELECT name, chrom, strand, txStart, txEnd, cdsStart, cdsEnd, "
        "exonStarts, exonEnds FROM ncbiRefSeq WHERE name='{}'"
    ).format(accession)
    result = bash('hgsql {} -Ne "{}"'.format(db, query))
    if not result.strip():
        raise ValueError("Transcript {} not found in {}.ncbiRefSeq".format(accession, db))
    fields = result.strip().split('\t')
    exon_starts = [int(x) for x in fields[7].rstrip(',').split(',')]
    exon_ends = [int(x) for x in fields[8].rstrip(',').split(',')]
    return {
        'name': fields[0],
        'chrom': fields[1],
        'strand': fields[2],
        'txStart': int(fields[3]),
        'txEnd': int(fields[4]),
        'cdsStart': int(fields[5]),
        'cdsEnd': int(fields[6]),
        'exonStarts': exon_starts,
        'exonEnds': exon_ends,
    }


def build_cds_regions(tx):
    """Return list of (start, end, exon_num) CDS segments, reversed for minus strand."""
    cds = []
    for i in range(len(tx['exonStarts'])):
        ex_start = tx['exonStarts'][i]
        ex_end = tx['exonEnds'][i]
        if ex_end <= tx['cdsStart'] or ex_start >= tx['cdsEnd']:
            continue
        cds.append((max(ex_start, tx['cdsStart']),
                    min(ex_end, tx['cdsEnd']),
                    i + 1))
    if tx['strand'] == '-':
        cds = cds[::-1]
    return cds


def aa_to_genomic(aa_start, aa_end, tx):
    """Map an AA range (1-based inclusive) to genomic segments in BED half-open.

    Uses the +/- strand-aware implementation from InSiGHT's clinDomains
    (post-PMS2-off-by-one-fix). Do not re-derive this math.
    """
    cds = build_cds_regions(tx)
    nt_start = (aa_start - 1) * 3 + 1
    nt_end = aa_end * 3
    segments = []
    cumulative = 0
    for start, end, exon_num in cds:
        region_len = end - start
        r_nt_start = cumulative + 1
        r_nt_end = cumulative + region_len
        if r_nt_end >= nt_start and r_nt_start <= nt_end:
            ov_nt_start = max(nt_start, r_nt_start)
            ov_nt_end = min(nt_end, r_nt_end)
            off_start = ov_nt_start - r_nt_start
            off_end = ov_nt_end - r_nt_start
            if tx['strand'] == '+':
                g_start = start + off_start
                g_end = start + off_end + 1
            else:
                g_end = end - off_start
                g_start = end - off_end - 1
                if g_start > g_end:
                    g_start, g_end = g_end, g_start
            segments.append((g_start, g_end, exon_num))
        cumulative += region_len
    return segments


def aa_codon_genomic(aa_pos, tx):
    """Convenience: genomic segments covering a single codon."""
    return aa_to_genomic(aa_pos, aa_pos, tx)


def cdna_coding_to_genomic(c_pos, tx):
    """Map a CDS-relative cDNA position (1-based, c.1..c.CDSlen) to a genomic
    coord (0-based inclusive). Returns None if position is outside the CDS.
    """
    aa_pos = (c_pos - 1) // 3 + 1
    within = (c_pos - 1) % 3
    segs = aa_to_genomic(aa_pos, aa_pos, tx)
    if not segs:
        return None
    # Collapse the (up to 2) segments for a codon into 3 nucleotide positions
    # in mRNA order.
    if tx['strand'] == '+':
        nts = []
        for g_start, g_end, _ex in segs:
            for g in range(g_start, g_end):
                nts.append(g)
    else:
        # Minus strand: mRNA order is reverse of genomic order
        nts = []
        for g_start, g_end, _ex in segs:
            # This segment's mRNA order = highest-to-lowest genomic
            for g in range(g_end - 1, g_start - 1, -1):
                nts.append(g)
        # Segments on minus strand came in "reversed exon order" &#8212; confirm by
        # checking that nts is monotonically decreasing. If a codon spans an
        # intron the two segments should already be ordered correctly.
    if within >= len(nts):
        return None
    return nts[within]


def write_autosql(path, content):
    with open(path, 'w') as f:
        f.write(content)
        if not content.endswith('\n'):
            f.write('\n')


def run_bedToBigBed(bed, as_file, bb, chrom_sizes, bed_type):
    """Invoke bedToBigBed with the given type (e.g., 'bed9+5')."""
    bash("bedToBigBed -as={} -type={} -tab {} {} {}".format(
        as_file, bed_type, bed, chrom_sizes, bb))


def chrom_sizes_path(db):
    return "/cluster/data/{}/chrom.sizes".format(db)


PER_PAPER_BEDS = {
    'kato':       ("/hive/users/lrnassar/claude/RM37399/functionalAssays/kato/TP53FuncKato_hg38.bed", 10),
    'giacomelli': ("/hive/users/lrnassar/claude/RM37399/functionalAssays/giacomelli/TP53FuncGiacomelli_hg38.bed", 10),
    'kawaguchi':  ("/hive/users/lrnassar/claude/RM37399/functionalAssays/kawaguchi/TP53FuncKawaguchi_hg38.bed", 9),
    'funk':       ("/hive/users/lrnassar/claude/RM37399/functionalAssays/funk/TP53FuncFunk_hg38.bed", 10),
}


def load_per_paper_raw_scores():
    """Read the per-paper functional assay BED files and return
    {short_protein_change: {kato_pct, giac_z, kaw_state, funk_rfs}}.

    Score column index (0-based):
      kato       -> col 10  (median % WT transactivation)
      giacomelli -> col 10  (A549 etoposide Z-score)
      kawaguchi  -> col 9   (oligomer state: Tetramer / Dimer / Monomer)
      funk       -> col 10  (CRISPR RFS median)
    """
    import os
    out = {}
    for paper, (path, score_col) in PER_PAPER_BEDS.items():
        if not os.path.exists(path):
            continue
        with open(path) as f:
            for line in f:
                flds = line.rstrip("\n").split("\t")
                if len(flds) <= score_col:
                    continue
                name = flds[3]
                val = flds[score_col]
                out.setdefault(name, {})[paper] = val
    return out


def html_ascii_safe(s):
    """Encode any non-ASCII character as an HTML numeric entity (&#NNNN;).

    UCSC hgTracks renders mouseover text through a pipeline that does not
    always preserve raw UTF-8 bytes; non-ASCII characters show up as
    mojibake (e.g. em dash as 'â€"'). Converting to &#NNNN; is reliable.
    Keeps & < > " ' as literals since they are valid in the existing
    mouseover HTML (the templates already escape user-controlled data
    separately where needed).
    """
    if s is None:
        return s
    return "".join(
        ch if ord(ch) < 128 else "&#{};".format(ord(ch))
        for ch in str(s)
    )

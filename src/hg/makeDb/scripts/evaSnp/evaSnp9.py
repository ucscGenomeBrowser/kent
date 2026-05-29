#!/usr/bin/env python3
"""Build EVA SNP tracks (native + GenArk contributed) — unified pipeline.

Replaces the two separate v8 pipelines (evaSnp8.py for native; evaSnpGenArk.py
for contrib). One discovery step routes each EVA-9 assembly to one of three
buckets — native (active UCSC db), contrib (GenArk hub only), or skip — and a
single per-assembly pipeline emits the bigBed. Deployment is bucket-specific:
native gets a /gbdb symlink + the evaSnpContainer composite subtrack; contrib
gets injected into <hub>/contrib/evaSnp9/ via Hiram's symlink pattern.

Usage:
    ./evaSnp9.py classify
    ./evaSnp9.py build all [-j N]
    ./evaSnp9.py build <db_or_accession>
    ./evaSnp9.py deploy native
    ./evaSnp9.py deploy contrib

See ~/kent/src/hg/makeDb/doc/evaSnp9.txt for the release-side recipe.
"""

import argparse
import collections
import concurrent.futures
import datetime
import glob
import gzip
import json
import logging
import os
import random
import re
import shutil
import stat
import subprocess
import sys
import time

# --- Version-specific constants ---

VERSION = "9"
TRACK_NAME = f"evaSnp{VERSION}"
EVA_FTP_BASE = f"https://ftp.ebi.ac.uk/pub/databases/eva/rs_releases/release_{VERSION}/by_assembly/"
OUTPUT_BASE = f"/hive/data/outside/eva{VERSION}"
CONTRIB_DIR = f"{OUTPUT_BASE}/contributedTracks"
CONTRIB_STAGING = f"/hive/data/outside/genark/{TRACK_NAME}"
GENBANK_SUMMARY = f"{OUTPUT_BASE}/assembly_summary_genbank.txt"

# --- Cross-release constants ---

GENARK_API_URL = "https://api.genome.ucsc.edu/list/genarkGenomes"
GENARK_HUB_BASE = "/hive/data/genomes/asmHubs"
GBDB_BASE = "/gbdb"

BGZIP = "/cluster/bin/tabix-0.2.6/bgzip"
TABIX = "/cluster/bin/tabix-0.2.6/tabix"
EVASNP_AS = os.path.expanduser("~/kent/src/hg/lib/evaSnp.as")
HGDB_CONF = "/usr/local/apache/cgi-bin/hg.conf"
VAI_PL = "/cluster/bin/scripts/vai.pl"

HGVAI_CHUNK_SIZE = 5_000_000
REF_VALIDATION_SAMPLE = 200
REF_VALIDATION_MIN_RATE = 95.0
VERSION_MISMATCH_BUILD_THRESHOLD = 10.0    # below this -> skip
VERSION_MISMATCH_REPORT_THRESHOLD = 40.0   # >this unmapped at build -> flag in summary
# A new build with item count below this fraction of the previous release's
# item count is treated as suspect and rejected. This catches cases where a
# native-db build silently picks a sparsely-populated EVA file when a richer
# one exists (e.g. mm10 picking GCA_000001635.4 / 20K vars instead of .6 /
# 165M vars). Only applies when the previous release built >1000 items
# for the same key, so missing/tiny prior releases don't false-fail.
PRIOR_RELEASE_MIN_RATIO = 0.5

GENE_TRACK_PRIORITY = [
    "ncbiRefSeqCurated", "ncbiRefSeq", "ncbiGene", "ensGene",
    "xenoRefGene", "augustus",
]

# Assemblies to skip outright in v9. Either the EVA data is for a different
# assembly version than what UCSC hosts (so ref alleles don't match), or the
# chrom-naming conventions don't bridge cleanly. The build would otherwise
# fail at the chrom-coverage or REF-validation gate, so this just moves them
# to the skip bucket up front for cleaner reporting.
SKIP_KEYS = {
    # Native — confirmed in first v9 build pass
    "micMur1": "EVA-9 GCA_000165445.3 chrom names don't bridge to /gbdb micMur1 (GCA_000165445.1); 0% chroms map",
    "melGal1": "EVA-9 GCA_000146605.3 REF alleles match /gbdb melGal1 only 22%, well below 95% threshold",
    "melGal5": "EVA-9 GCA_000146605.3 REF alleles match /gbdb melGal5 only 19.4%, well below 95% threshold",
    # Contrib — confirmed in first v9 build pass; all chrom-naming or REF mismatches
    "GCA_002915635.3": "Axolotl: 0% chroms map (chrom-naming mismatch)",
    "GCF_000002335.3": "Red flour beetle: 0% chroms map (chrom-naming mismatch)",
    "GCF_000002775.4": "Black cottonwood: 0% chroms map (chrom-naming mismatch)",
    "GCF_000188115.4": "Tomato: 0% chroms map (chrom-naming mismatch)",
    "GCF_000372685.2": "Mexican tetra: 0% chroms map (chrom-naming mismatch)",
    "GCF_000309985.2": "Brassica rapa: REF alleles match <95% (version mismatch)",
    "GCF_000686985.2": "Brassica napus: REF alleles match <95% (version mismatch)",
    "GCF_000803125.2": "Dromedary camel: REF alleles match <95% (version mismatch)",
    "GCF_002162155.1": "REF alleles match <95% (version mismatch); flagged in v9 audit",
}

# SO term -> v9 canonical varClass label. Six values; anything outside this map
# (e.g., SO:0001019 copy_number_variation, SO:0000705 tandem_repeat) falls back
# to sequence_alteration so it still passes the trackDb filterValues.varClass.
SO_TO_CLASS = {
    "SO:0001483": "SNV",
    "SO:0000159": "deletion",
    "SO:0000667": "insertion",
    "SO:0002007": "MNV",
    "SO:1000032": "indel",
    "SO:0001059": "sequence_alteration",
}
SO_FALLBACK_CLASS = "sequence_alteration"

CONSEQUENCE_RANK = {
    "transcript_ablation": 1, "splice_acceptor_variant": 2, "splice_donor_variant": 3,
    "stop_gained": 4, "frameshift_variant": 5, "stop_lost": 6, "start_lost": 7,
    "transcript_amplification": 8, "inframe_insertion": 9, "inframe_deletion": 10,
    "missense_variant": 11, "protein_altering_variant": 12, "splice_region_variant": 13,
    "incomplete_terminal_codon_variant": 14, "start_retained_variant": 15,
    "stop_retained_variant": 16, "synonymous_variant": 17, "coding_sequence_variant": 18,
    "mature_miRNA_variant": 19, "5_prime_UTR_variant": 20, "3_prime_UTR_variant": 21,
    "non_coding_transcript_exon_variant": 22, "intron_variant": 23,
    "NMD_transcript_variant": 24, "non_coding_transcript_variant": 25,
    "upstream_gene_variant": 26, "downstream_gene_variant": 27,
    "TFBS_ablation": 28, "TFBS_amplification": 29, "TF_binding_site_variant": 30,
    "regulatory_region_ablation": 31, "regulatory_region_amplification": 32,
    "feature_elongation": 33, "regulatory_region_variant": 34,
    "feature_truncation": 35, "intergenic_variant": 36,
}

# 4-color scheme matching native evaSnp tracks
CONSEQUENCE_COLORS = {
    # Red — protein-altering and splice
    "exon_loss_variant": "255,0,0", "frameshift_variant": "255,0,0",
    "inframe_deletion": "255,0,0", "inframe_insertion": "255,0,0",
    "initiator_codon_variant": "255,0,0", "missense_variant": "255,0,0",
    "splice_acceptor_variant": "255,0,0", "splice_donor_variant": "255,0,0",
    "splice_region_variant": "255,0,0", "stop_gained": "255,0,0",
    "stop_lost": "255,0,0", "start_lost": "255,0,0",
    "coding_sequence_variant": "255,0,0", "transcript_ablation": "255,0,0",
    "protein_altering_variant": "255,0,0",
    # Green — synonymous
    "synonymous_variant": "0,128,0", "stop_retained_variant": "0,128,0",
    "start_retained_variant": "0,128,0",
    # Blue — non-coding transcript / UTR
    "5_prime_UTR_variant": "0,0,255", "3_prime_UTR_variant": "0,0,255",
    "complex_transcript_variant": "0,0,255",
    "non_coding_transcript_exon_variant": "0,0,255",
    # Black — intergenic / intronic
    "upstream_gene_variant": "0,0,0", "downstream_gene_variant": "0,0,0",
    "intron_variant": "0,0,0", "intergenic_variant": "0,0,0",
    "NMD_transcript_variant": "0,0,0", "no_sequence_alteration": "0,0,0",
    "non_coding_transcript_variant": "0,0,0",
}
DEFAULT_COLOR = "0,0,0"

VEP_SKIP_PREFIXES = ("Content", "Error", "needMem", "vpExpand", "Location",
                     "Uploaded", "\n", "#")

# Shared filterValues / filterLabel block used by both trackDb variants.
_TRACKDB_FILTER_BLOCK = """\
filterValues.varClass SNV|SNV,deletion|Deletion,insertion|Insertion,indel|Indel,MNV|MNV,sequence_alteration|Sequence alteration
filterLabel.varClass Variant class from EVA SO term
filterLabel.ucscClass Functional effect per UCSC Variant Annotation
filterValues.ucscClass downstream_gene_variant|Downstream gene variant,upstream_gene_variant|Upstream gene variant,intron_variant|Intron variant,NMD_transcript_variant|Nonsense-mediated mRNA decay (NMD) variant,5_prime_UTR_variant|5 prime UTR variant,3_prime_UTR_variant|3 prime UTR variant,missense_variant|Missense variant,synonymous_variant|Synonymous variant,non_coding_transcript_exon_variant|Non-coding transcript exon variant,no_sequence_alteration|No sequence alteration,splice_region_variant|Splice region variant,frameshift_variant|Frameshift variant,stop_gained|Stop gained,splice_acceptor_variant|Splice acceptor variant,inframe_deletion|Inframe deletion,inframe_insertion|Inframe insertion,splice_donor_variant|Splice donor variant,coding_sequence_variant|Coding sequence variant,initiator_codon_variant|Initiator codon variant,stop_lost|Stop lost,stop_retained_variant|Stop retained variant,intergenic_variant|Intergenic variant
filterType.ucscClass multipleListOnlyOr
filterValues.itemRgb 255,,0,,0|Protein-altering and splice variants,0,,128,,0|Synonymous variants,0,,0,,255|Non-coding transcripts or UTR variants,0,,0,,0|Intergenic and intronic variants
filterLabel.itemRgb General variant types by color grouping
maxWindowCoverage 250000
maxItems 1000000
"""

# Per-assembly trackDb.txt for the standalone hub layout — paths are RELATIVE
# to the per-assembly directory under contributedTracks/<acc>/. Used when
# users subscribe to the hub via its hub.txt URL.
STANDALONE_TRACKDB_TEMPLATE = f"""\
track {TRACK_NAME}
shortLabel EVA SNP Release {VERSION}
longLabel Short Genetic Variants from European Variant Archive Release {VERSION}
type bigBed 9 +
visibility dense
itemRgb on
bigDataUrl {TRACK_NAME}.bb
url https://www.ebi.ac.uk/eva/?variant&accessionID=$$
searchIndex name
mouseOver <b>Ref/Alt allele(s)</b>: $ref>$alt<br> <b>Var type</b>: $ucscClass<br> <b>AA change</b>: $aaChange
{_TRACKDB_FILTER_BLOCK}\
html description
"""

# GenArk-injection trackDb.txt — symlinked into each hub's contrib/<TRACK>/
# directory by Hiram's mkLinks.sh. Paths are prefixed with contrib/<TRACK>/
# because they're read from the hub root, not the contrib subdir.
CONTRIB_TRACKDB_TEMPLATE = f"""\
track {TRACK_NAME}
shortLabel EVA SNP Release {VERSION}
longLabel Short Genetic Variants from European Variant Archive Release {VERSION}
type bigBed 9 +
visibility dense
group varRep
itemRgb on
bigDataUrl contrib/{TRACK_NAME}/{TRACK_NAME}.bb
url https://www.ebi.ac.uk/eva/?variant&accessionID=$$
searchIndex name
mouseOver <b>Ref/Alt allele(s)</b>: $ref>$alt<br> <b>Var type</b>: $ucscClass<br> <b>AA change</b>: $aaChange
{_TRACKDB_FILTER_BLOCK}\
html contrib/{TRACK_NAME}/description
"""

log = logging.getLogger(TRACK_NAME)


class RefValidationError(RuntimeError):
    """Raised by validateRefAlleles when the sampled REF rate falls below
    the threshold. Caught by the per-candidate retry loop in processOne."""


# --- Subprocess wrapper ---

def runCmd(cmd, timeout=600, allowedExitCodes=None, env=None):
    """Run a shell command. Raises RuntimeError on unexpected exit codes."""
    if allowedExitCodes is None:
        allowedExitCodes = {0}
    log.debug("CMD: %s", cmd)
    result = subprocess.run(
        cmd, shell=True, capture_output=True, text=True,
        timeout=timeout, env=env,
    )
    if result.returncode not in allowedExitCodes:
        raise RuntimeError(
            f"Command failed (exit {result.returncode}): {cmd}\n"
            f"STDERR: {result.stderr[:2000]}"
        )
    return result.stdout, result.stderr, result.returncode


def hgsqlRows(db, query):
    """Run `hgsql -N` (no header) and return one list-of-strings per row."""
    stdout, _, _ = runCmd(f"hgsql -N -e {sh(query)} {db}", timeout=120)
    rows = []
    for line in stdout.splitlines():
        rows.append(line.split("\t"))
    return rows


def sh(s):
    """Quote a string for safe shell interpolation."""
    return "'" + s.replace("'", "'\\''") + "'"


# --- GenArk hub paths ---

def getGenArkHubPath(accession):
    """Filesystem path for a GenArk assembly hub.

    Returns the canonical short path (no buildType, no suffix), which is a
    symlink to the actual build dir and is where hub files are read from at
    runtime. mkLinks.sh injects symlinks into the build dir separately
    (refseqBuild/genbankBuild with the suffixed dir name), via its own glob.
    """
    prefix = accession[:3]
    digits = accession.split("_")[1].split(".")[0].zfill(9)
    d1, d2, d3 = digits[0:3], digits[3:6], digits[6:9]
    return os.path.join(GENARK_HUB_BASE, prefix, d1, d2, d3, accession)


# --- AssemblyTarget: kind-agnostic per-assembly inputs ---

class AssemblyTarget:
    """Per-assembly inputs resolved once. Pipeline stages read from this.

    kind          'native' or 'contrib'
    evaGca        EVA-side accession (e.g. GCA_000146045.2)
    key           native db (e.g. sacCer3) or GenArk accession (e.g. GCF_xxx)
    hgVaiDb       db argument for vai.pl (native db or accession)
    workDir       per-assembly build directory
    versionMismatch  True if EVA and assembly versions differ
    """
    def __init__(self, kind, evaGca, key, versionMismatch=False, genarkAcc=None):
        if kind not in ("native", "contrib"):
            raise ValueError(f"bad kind: {kind}")
        self.kind = kind
        self.evaGca = evaGca
        self.key = key
        self.hgVaiDb = key
        self.versionMismatch = versionMismatch
        self.genarkAcc = genarkAcc  # for native, the GCA/GCF that matched; for contrib, same as key

        if kind == "native":
            self.workDir = os.path.join(OUTPUT_BASE, key)
            self.twoBit = os.path.join(GBDB_BASE, key, f"{key}.2bit")
            self._hub = None
        else:
            self.workDir = os.path.join(CONTRIB_DIR, key)
            self._hub = getGenArkHubPath(key)
            self.twoBit = os.path.join(self._hub, f"{key}.2bit")

    def __repr__(self):
        return f"<AssemblyTarget {self.kind} {self.key} <- {self.evaGca}>"

    def loadChromMap(self):
        """EVA-style name -> primary chrom name."""
        if self.kind == "native":
            return self._loadNativeChromMap()
        return self._loadContribChromMap()

    def _loadNativeChromMap(self):
        chromMap = {}
        rows = hgsqlRows(self.key, "select alias, chrom from chromAlias")
        for r in rows:
            if len(r) < 2:
                continue
            alias, chrom = r[0], r[1]
            if alias and chrom:
                chromMap.setdefault(alias, chrom)
        # identity for known chroms, plus a "chr"-prefix fallback so legacy
        # native dbs whose chromAlias lacks bare-name aliases (e.g. bosTau7's
        # chromAlias only has refseq names) still resolve EVA's "1"/"2"/"X"/
        # "MT" chrom-name convention to chr1/chr2/chrX/chrMT.
        for r in hgsqlRows(self.key, "select chrom from chromInfo"):
            if not r or not r[0]:
                continue
            chrom = r[0]
            chromMap.setdefault(chrom, chrom)
            if chrom.startswith("chr"):
                chromMap.setdefault(chrom[3:], chrom)
        if not chromMap:
            raise RuntimeError(f"empty chromAlias/chromInfo for {self.key}")
        return chromMap

    def _loadContribChromMap(self):
        aliasFile = os.path.join(self._hub, f"{self.key}.chromAlias.txt")
        if not os.path.exists(aliasFile):
            raise RuntimeError(f"chromAlias.txt not found: {aliasFile}")
        chromMap = {}
        with open(aliasFile) as f:
            for line in f:
                if line.startswith("#"):
                    continue
                fields = line.rstrip("\n").split("\t")
                if len(fields) < 2:
                    continue
                native = fields[0]
                chromMap.setdefault(native, native)
                for col in fields[1:]:
                    if col:
                        chromMap.setdefault(col, native)
        return chromMap

    def loadChromSizes(self):
        sizes = {}
        if self.kind == "native":
            for r in hgsqlRows(self.key, "select chrom, size from chromInfo"):
                if len(r) >= 2:
                    sizes[r[0]] = int(r[1])
        else:
            sizesFile = os.path.join(self._hub, f"{self.key}.chrom.sizes.txt")
            if not os.path.exists(sizesFile):
                raise RuntimeError(f"chrom.sizes not found: {sizesFile}")
            with open(sizesFile) as f:
                for line in f:
                    fields = line.rstrip("\n").split("\t")
                    if len(fields) >= 2:
                        sizes[fields[0]] = int(fields[1])
        if not sizes:
            raise RuntimeError(f"empty chrom.sizes for {self.key}")
        return sizes

    def getGeneTrack(self):
        if self.kind == "native":
            tables = {r[0] for r in hgsqlRows(self.key, "show tables") if r}
            for t in GENE_TRACK_PRIORITY:
                if t in tables:
                    return t
            # legacy native fallbacks not in the GenArk priority list
            for t in ("refGene", "sgdGene"):
                if t in tables:
                    return t
            return None
        # contrib: scan bbi/
        bbiPath = os.path.join(self._hub, "bbi")
        if not os.path.isdir(bbiPath):
            return None
        for t in GENE_TRACK_PRIORITY:
            if glob.glob(os.path.join(bbiPath, f"*.{t}.bb")):
                return t
        return None


# --- Discovery: three buckets ---

def fetchEvaAssemblies():
    url = EVA_FTP_BASE
    log.info("Fetching EVA release %s assembly list...", VERSION)
    stdout, _, _ = runCmd(f"curl -s {url}", timeout=120)
    accessions = set()
    for m in re.finditer(r'href="(GCA_\d+\.\d+)/"', stdout):
        accessions.add(m.group(1))
    log.info("  %d EVA-%s assemblies", len(accessions), VERSION)
    return accessions


def fetchGenArkAssemblies():
    log.info("Fetching GenArk assembly catalog...")
    stdout, _, _ = runCmd(f"curl -s {GENARK_API_URL}", timeout=120)
    data = json.loads(stdout)
    genomes = data.get("genarkGenomes", {})
    log.info("  %d GenArk assemblies", len(genomes))
    return genomes


def downloadGenbankSummary():
    """Download/refresh NCBI genbank assembly summary used for GCA<->GCF mapping."""
    os.makedirs(OUTPUT_BASE, exist_ok=True)
    if not os.path.exists(GENBANK_SUMMARY):
        log.info("Downloading NCBI genbank summary...")
        runCmd(
            f"wget -q -O {GENBANK_SUMMARY} "
            f"https://ftp.ncbi.nlm.nih.gov/genomes/ASSEMBLY_REPORTS/"
            f"assembly_summary_genbank.txt",
            timeout=600,
        )


def loadGcaToGcfMapping():
    """GCA -> GCF mapping from NCBI summary."""
    downloadGenbankSummary()
    mapping = {}
    with open(GENBANK_SUMMARY) as f:
        for line in f:
            if line.startswith("#"):
                continue
            fields = line.rstrip("\n").split("\t")
            if len(fields) < 18:
                continue
            gca, gcf = fields[0], fields[17]
            if gcf and gcf != "na" and gcf.startswith("GCF_"):
                mapping[gca] = gcf
    log.info("  %d GCA->GCF mappings", len(mapping))
    return mapping


def fetchActiveNativeDbs():
    """Return dict mapping UCSC db -> set of GCA/GCF accessions in its description.html.

    Limited to dbs marked active=1 in hgcentral.dbDb. Native build skips dbs that
    don't have at least one GCA/GCF in their description page.
    """
    log.info("Loading active UCSC dbs from hgcentral...")
    stdout, _, _ = runCmd(
        "hgsql -N -h genome-centdb -e "
        "\"select name from dbDb where active=1\" hgcentral",
        timeout=60,
    )
    dbs = [d for d in stdout.strip().split("\n") if d]
    log.info("  %d active dbs", len(dbs))

    accPattern = re.compile(r"GC[AF]_\d+\.\d+")
    result = {}
    for db in dbs:
        desc = os.path.join(GBDB_BASE, db, "html", "description.html")
        if not os.path.exists(desc):
            continue
        try:
            # Some description pages have non-UTF8 bytes (older legacy pages);
            # we only grep for accession patterns so replacement is harmless.
            with open(desc, errors="replace") as f:
                text = f.read()
        except OSError:
            continue
        accs = set(accPattern.findall(text))
        if accs:
            result[db] = accs
    log.info("  %d active dbs have an accession in description.html", len(result))
    return result


def _evaVcfSize(evaGca):
    """HEAD-request the EVA VCF for `evaGca`, return Content-Length in bytes.

    Returns -1 if the URL can't be resolved or the HEAD fails — callers treat
    that as "smallest" so a successful sibling wins the tie.
    """
    try:
        url = resolveEvaVcfUrl(evaGca)
    except RuntimeError:
        return -1
    return _contentLength(url)


def _orderEvaCandidatesBySize(db, candidates):
    """Return EVA candidates ordered largest-VCF-first.

    The first candidate is the default pick. If it fails REF validation in
    the pipeline, the next one is tried. This handles the felCat8 / galGal4
    case where the largest EVA file is a different assembly version that
    happens to be incompatible at the sequence level.
    """
    sized = []
    for evaGca, mm in candidates:
        size = _evaVcfSize(evaGca)
        sized.append((size, evaGca, mm))
    sized.sort(key=lambda x: (-x[0], x[1]))  # largest size first, lex tiebreaker
    if len(candidates) > 1:
        log.info("  native %s: ordered candidates: %s",
                 db,
                 ", ".join(f"{g}({s/1e6:.1f}MB)" for s, g, _ in sized))
    return [(g, mm) for _, g, mm in sized]


def buildMatchList():
    """Build the three-bucket classification.

    Returns (native, contrib, skip):
      native: list of AssemblyTarget (kind=native)
      contrib: list of AssemblyTarget (kind=contrib)
      skip:   list of (evaGca, reason)
    """
    evaAsm = fetchEvaAssemblies()
    nativeDbs = fetchActiveNativeDbs()
    genarkAsm = fetchGenArkAssemblies()
    gcaToGcf = loadGcaToGcfMapping()
    genarkSet = set(genarkAsm.keys())
    genarkByBase = {acc.rsplit(".", 1)[0]: acc for acc in genarkSet}

    # Index native dbs by every accession (exact + base) so a GCA/GCF lookup is O(1).
    nativeByAcc = {}
    nativeByBase = {}
    for db, accs in nativeDbs.items():
        for a in accs:
            nativeByAcc.setdefault(a, db)
            nativeByBase.setdefault(a.rsplit(".", 1)[0], db)

    nativeCandidates = collections.defaultdict(list)  # db -> [(evaGca, mismatch), ...]
    contribCandidates = {}  # genarkAcc -> (evaGca, mismatch)

    for evaGca in sorted(evaAsm):
        # gcaToGcf misses some entries (NCBI suppresses some GCAs even though the
        # GCF still exists at the same base+version). As a fallback, try the
        # naive prefix swap GCA_xxx.N -> GCF_xxx.N — same assembly, RefSeq
        # blessing only. If the synthesized GCF doesn't appear in any lookup
        # table, it goes nowhere.
        gcf = gcaToGcf.get(evaGca) or "GCF_" + evaGca[4:]
        # --- native? ---
        db = nativeByAcc.get(evaGca)
        nativeMismatch = False
        if db is None and gcf:
            db = nativeByAcc.get(gcf)
        if db is None:
            db2 = nativeByBase.get(evaGca.rsplit(".", 1)[0])
            if db2:
                db = db2
                nativeMismatch = True
            elif gcf:
                db2 = nativeByBase.get(gcf.rsplit(".", 1)[0])
                if db2:
                    db = db2
                    nativeMismatch = True
        if db is not None:
            nativeCandidates[db].append((evaGca, nativeMismatch))
            continue

        # --- contrib? ---
        genarkAcc = None
        mismatch = False
        if evaGca in genarkSet:
            genarkAcc = evaGca
        elif gcf and gcf in genarkSet:
            genarkAcc = gcf
        else:
            if gcf:
                b = gcf.rsplit(".", 1)[0]
                if b in genarkByBase:
                    genarkAcc = genarkByBase[b]
                    mismatch = True
            if genarkAcc is None:
                b = evaGca.rsplit(".", 1)[0]
                if b in genarkByBase:
                    genarkAcc = genarkByBase[b]
                    mismatch = True
        if genarkAcc is not None:
            prev = contribCandidates.get(genarkAcc)
            if prev is None or (prev[1] and not mismatch):
                contribCandidates[genarkAcc] = (evaGca, mismatch)

    # Resolve multi-candidate native dbs into an ORDERED list of EVA versions
    # (largest VCF first). pipeline tries them in order, falling back on REF
    # validation failure — that handles cases like felCat8/galGal4 where the
    # largest EVA version is for a sequence-incompatible assembly variant.
    nativeMatches = {}  # db -> [(evaGca, mismatch), ...] ordered
    for db, candidates in nativeCandidates.items():
        if len(candidates) == 1:
            nativeMatches[db] = list(candidates)
        else:
            nativeMatches[db] = _orderEvaCandidatesBySize(db, candidates)

    # Runners-up still get recorded in the skip report for auditability —
    # they aren't "lost" anymore (pipeline may fall back to them), but the
    # default pick is the first in the ordered list.
    nativeCollisions = []
    for db, ordered in nativeMatches.items():
        chosen = ordered[0][0]
        for evaGca, _ in ordered[1:]:
            nativeCollisions.append((evaGca, db))

    # Skip-list lookup that matches on either the exact key OR the base accession
    # (so adding "GCF_000188115.4" to SKIP_KEYS also catches an alternate
    # build picking up "GCF_000188115.5"). For native db keys, base matching
    # is a no-op since they're not accessions.
    skipBases = {k.rsplit(".", 1)[0]: r
                 for k, r in SKIP_KEYS.items() if k.startswith(("GCA_", "GCF_"))}

    def skipReason(key):
        if key in SKIP_KEYS:
            return SKIP_KEYS[key]
        if key.startswith(("GCA_", "GCF_")):
            return skipBases.get(key.rsplit(".", 1)[0])
        return None

    skip = []
    nativeTargets = []
    contribTargets = []
    skippedByList = []  # (kind, key, evaGca, reason)

    for db, ordered in sorted(nativeMatches.items()):
        primaryEvaGca, primaryMm = ordered[0]
        reason = skipReason(db)
        if reason is not None:
            skippedByList.append(("native", db, primaryEvaGca, reason))
            continue
        t = AssemblyTarget(
            "native", primaryEvaGca, db, versionMismatch=primaryMm,
            genarkAcc=primaryEvaGca if primaryEvaGca in genarkSet
            else gcaToGcf.get(primaryEvaGca),
        )
        t.evaGcaCandidates = ordered  # for retry fallback
        nativeTargets.append(t)
    for acc, (evaGca, mm) in sorted(contribCandidates.items()):
        reason = skipReason(acc)
        if reason is not None:
            skippedByList.append(("contrib", acc, evaGca, reason))
            continue
        t = AssemblyTarget(
            "contrib", evaGca, acc, versionMismatch=mm, genarkAcc=acc,
        )
        t.evaGcaCandidates = [(evaGca, mm)]  # contrib has only one candidate
        contribTargets.append(t)

    matchedEva = ({t.evaGca for t in nativeTargets}
                  | {t.evaGca for t in contribTargets}
                  | {g for _, _, g, _ in skippedByList})
    for kind, key, evaGca, reason in skippedByList:
        skip.append((evaGca, f"in SKIP_KEYS ({kind} {key}): {reason}"))
    for evaGca, db in sorted(nativeCollisions):
        skip.append((evaGca, f"matched native db {db} but lost slot to another EVA GCA"))
    for evaGca in sorted(evaAsm - matchedEva - {g for g, _ in nativeCollisions}):
        skip.append((evaGca, "no native or GenArk assembly"))

    log.info("Discovery: %d native, %d contrib, %d skipped (of %d EVA-%s)",
             len(nativeTargets), len(contribTargets), len(skip),
             len(evaAsm), VERSION)
    return nativeTargets, contribTargets, skip


# --- EVA download ---

def _contentLength(url):
    """HEAD-request `url`, return Content-Length (or -1 on failure)."""
    try:
        stdout, _, _ = runCmd(f"curl -sIL --max-time 30 {url}", timeout=60)
    except RuntimeError:
        return -1
    size = -1
    for line in stdout.splitlines():
        if line.lower().startswith("content-length:"):
            try:
                size = int(line.split(":", 1)[1].strip())
            except ValueError:
                pass
    return size


def resolveEvaVcfUrl(gcaAccession):
    """Locate the EVA VCF URL for a GCA accession.

    Some EVA assemblies have multiple species subdirectories under a single
    GCA (e.g. GCA_000003055.3 has bison_bison_bison / bos_indicus_x_bos_taurus
    / bos_taurus); each species' VCF can differ wildly in variant count. The
    legacy v8 native script hardcoded the right species per db. v9 instead
    HEAD-requests every species' candidate file and picks the largest.
    """
    baseUrl = EVA_FTP_BASE
    dirUrl = f"{baseUrl}{gcaAccession}/"
    stdout, _, _ = runCmd(f"curl -s {dirUrl}", timeout=60)
    speciesDirs = re.findall(r'href="([^"]+/)"\s*>', stdout)
    speciesDirs = [d for d in speciesDirs if not d.startswith("..")
                   and not d.startswith("/")]
    if not speciesDirs:
        raise RuntimeError(f"no species dirs at {dirUrl}")

    # Find the VCF inside each species dir
    candidates = []  # (url, size)
    for species in speciesDirs:
        speciesUrl = f"{dirUrl}{species}"
        try:
            stdout, _, _ = runCmd(f"curl -s {speciesUrl}", timeout=60)
        except RuntimeError:
            continue
        vcfFiles = re.findall(r'href="([^"]*current_ids\.vcf\.gz)"', stdout)
        if not vcfFiles:
            vcfFiles = re.findall(r'href="([^"]*\.vcf\.gz)"', stdout)
        if not vcfFiles:
            continue
        fullUrl = f"{speciesUrl}{vcfFiles[0]}"
        candidates.append(fullUrl)

    if not candidates:
        raise RuntimeError(f"no VCF in any species dir at {dirUrl}")
    if len(candidates) == 1:
        return candidates[0]

    sized = [(c, _contentLength(c)) for c in candidates]
    sized.sort(key=lambda x: -x[1])
    log.info("  %s has %d species variants — picking %s (%.1f MB)",
             gcaAccession, len(candidates), sized[0][0].rsplit("/", 1)[-1],
             max(sized[0][1], 0) / 1e6)
    return sized[0][0]


def downloadVcf(url, workDir):
    fileName = url.rsplit("/", 1)[-1]
    outPath = os.path.join(workDir, fileName)
    if os.path.exists(outPath):
        log.info("  VCF cached: %s", fileName)
        return outPath
    log.info("  downloading: %s", fileName)
    for attempt in range(3):
        try:
            runCmd(f"wget -q -N -P {workDir} {url}", timeout=3600)
            return outPath
        except RuntimeError:
            if attempt == 2:
                raise
            log.warning("  download attempt %d failed, retrying", attempt + 1)
    return outPath


# --- VCF chrom conversion ---

def enrichChromMapFromVcfContigs(vcfGzPath, chromMap):
    """Extend chromMap using `##contig=<ID=X,accession=Y>` lines from the VCF.

    EVA VCFs use bare assembly chrom names like "1" or scaffold IDs like
    "scf7180017457635" that don't appear in the native UCSC chromAlias table.
    But their ##contig headers carry an `accession="..."` attribute pointing
    to the genbank/refseq name that *is* in chromAlias. Resolving the chain
    `ID -> accession -> chromMap[accession] -> native chrom` lets the
    downstream conversion proceed without per-db hacks.

    Mutates and returns the same dict so the caller doesn't need to re-bind.
    """
    contigPattern = re.compile(
        r'^##contig=<ID=([^,>]+),(?:[^>]*?,)?accession="?([^",>]+)"?')
    added = 0
    orphans = []  # ##contig IDs we couldn't bridge via accession
    with gzip.open(vcfGzPath, "rt") as f:
        for line in f:
            if not line.startswith("##"):
                if line.startswith("#"):
                    break  # end of header
                break
            if not line.startswith("##contig"):
                continue
            m = contigPattern.match(line)
            if not m:
                continue
            chromId, accession = m.group(1), m.group(2)
            if chromId in chromMap:
                continue
            target = chromMap.get(accession)
            if target:
                chromMap[chromId] = target
                added += 1
            else:
                orphans.append(chromId)
    if added:
        log.info("  ##contig enrichment: added %d ID->native mappings", added)
    return orphans


def bridgeMitochondrialOrphan(vcfGzPath, chromMap, orphans, mitoChrom):
    """Map a single low-volume orphan ##contig ID to the assembly's mito chrom.

    EVA periodically uses a mitochondrial accession (e.g. KP263414.1 for
    sacCer3, AC024175.3 for danRer, X54252.1 for ce11) that doesn't appear in
    the assembly's chromAlias table. v8 had per-db hardcodes for these. The
    REF allele validation step that follows will reject the bridge if the
    sequence content doesn't actually match the assembly's mito.
    """
    if not orphans or not mitoChrom:
        return
    # Count variants per orphan in the VCF
    counts = {}
    totalRows = 0
    orphanSet = set(orphans)
    with gzip.open(vcfGzPath, "rt") as f:
        for line in f:
            if line.startswith("#"):
                continue
            totalRows += 1
            chrom = line.split("\t", 1)[0]
            if chrom in orphanSet:
                counts[chrom] = counts.get(chrom, 0) + 1
    if not counts or totalRows == 0:
        return
    # Pick the orphan with the most variants; require it to be small (<5% of
    # total) so we don't bridge a real chromosome by mistake.
    candidate = max(counts.items(), key=lambda kv: kv[1])
    pct = 100.0 * candidate[1] / totalRows
    if pct >= 5.0:
        log.info("  mito bridge skipped: largest orphan %s has %.1f%% of "
                 "variants (>=5%%) — likely a real chrom, not mito",
                 candidate[0], pct)
        return
    chromMap[candidate[0]] = mitoChrom
    log.info("  mito bridge: %s (%d variants, %.2f%%) -> %s",
             candidate[0], candidate[1], pct, mitoChrom)


def _findMitoChrom(chromInfoNames):
    """Return the assembly's mito chrom name (chrM/chrMT/chrMt) or None."""
    for name in chromInfoNames:
        if name in ("chrM", "chrMT", "chrMt"):
            return name
    return None


def convertAndDeduplicateVcf(vcfGzPath, chromMap, workDir):
    outPath = os.path.join(workDir, "converted.vcf")
    mapped = unmapped = dupes = total = 0
    chromsSeen = set()
    chromsUnmapped = set()
    prevLine = None
    with gzip.open(vcfGzPath, "rt") as inf, open(outPath, "w") as outf:
        for line in inf:
            if line.startswith("#"):
                outf.write(line)
                continue
            total += 1
            if line == prevLine:
                dupes += 1
                continue
            prevLine = line
            origChrom, rest = line.split("\t", 1)
            chromsSeen.add(origChrom)
            newChrom = chromMap.get(origChrom)
            if newChrom is None:
                unmapped += 1
                chromsUnmapped.add(origChrom)
                continue
            mapped += 1
            outf.write(newChrom + "\t" + rest)
    log.info("  chrom convert: %d mapped, %d unmapped, %d dupes (of %d)",
             mapped, unmapped, dupes, total)
    if mapped == 0:
        raise RuntimeError("no variants mapped to assembly chromosomes")
    pctMapped = 100.0 * (len(chromsSeen) - len(chromsUnmapped)) / max(len(chromsSeen), 1)
    return outPath, len(chromsSeen), len(chromsUnmapped), pctMapped


def validateRefAlleles(vcfPath, twoBitPath, sampleSize=REF_VALIDATION_SAMPLE):
    if not os.path.exists(twoBitPath):
        log.warning("  2bit not found at %s — skipping ref check", twoBitPath)
        return
    snvLines = []
    with open(vcfPath) as f:
        for line in f:
            if line.startswith("#"):
                continue
            fields = line.split("\t", 5)
            if len(fields) < 5:
                continue
            ref, alt = fields[3], fields[4]
            if (len(ref) == 1 and len(alt) == 1
                    and ref in "ACGTacgt" and alt in "ACGTacgt"):
                snvLines.append((fields[0], int(fields[1]), ref.upper()))
                if len(snvLines) >= sampleSize * 10:
                    break
    if len(snvLines) < 20:
        log.warning("  only %d SNVs sampled — skipping ref check", len(snvLines))
        return
    sample = random.sample(snvLines, min(sampleSize, len(snvLines)))
    match = mismatch = 0
    for chrom, pos, ref in sample:
        try:
            stdout, _, _ = runCmd(
                f"twoBitToFa {twoBitPath} stdout -seq={chrom} "
                f"-start={pos - 1} -end={pos}",
                timeout=10,
            )
            actual = "".join(stdout.split("\n")[1:]).upper()
            if ref == actual:
                match += 1
            else:
                mismatch += 1
        except RuntimeError:
            pass
    total = match + mismatch
    if total == 0:
        log.warning("  could not validate any ref alleles")
        return
    rate = 100.0 * match / total
    log.info("  ref validation: %d/%d match (%.1f%%)", match, total, rate)
    if rate < REF_VALIDATION_MIN_RATE:
        raise RefValidationError(
            f"ref allele validation failed: only {rate:.1f}% match "
            f"(threshold {REF_VALIDATION_MIN_RATE}%). Likely a version mismatch."
        )


def sortVcf(vcfPath):
    """Sort a VCF in place, preserving any header lines.

    `grep '^#'` exits non-zero when zero header lines match, which would
    short-circuit the && chain and leave an empty sorted file. EVA VCFs
    always have headers, but we tolerate the headerless case defensively.
    """
    sortedPath = vcfPath + ".sorted"
    runCmd(
        f"(grep '^#' {vcfPath} || true) > {sortedPath}"
        f" && (grep -v '^#' {vcfPath} | sort -S 4G -k1,1 -k2,2n >> {sortedPath})",
        timeout=7200,
    )
    if os.path.getsize(sortedPath) == 0:
        os.remove(sortedPath)
        raise RuntimeError(f"sortVcf produced an empty file from {vcfPath}")
    os.replace(sortedPath, vcfPath)


def bgzipAndTabix(vcfPath):
    sortVcf(vcfPath)
    gzPath = vcfPath + ".gz"
    if os.path.exists(gzPath):
        os.remove(gzPath)
    runCmd(f"{BGZIP} {vcfPath}", timeout=1800)
    runCmd(f"{TABIX} -p vcf {gzPath}", timeout=1800)
    return gzPath


def vcfToBed(vcfGzPath, workDir):
    outPrefix = os.path.join(workDir, "evaSnp")
    runCmd(f"vcfToBed {vcfGzPath} {outPrefix} -fields=VC,SID", timeout=3600)
    bedPath = outPrefix + ".bed"
    if not os.path.exists(bedPath):
        raise RuntimeError(f"vcfToBed did not produce {bedPath}")
    return bedPath


# --- hgVai ---

def probeVaiResolvesNative(target, vcfGzPath, geneTrack):
    """Detect if vai.pl resolves a GenArk accession to a native UCSC db.

    For contrib targets only — a native AssemblyTarget always uses the native
    db directly, so this never applies. Returns (db_name or None).
    """
    if target.kind == "native":
        return None
    env = os.environ.copy()
    env["HGDB_CONF"] = HGDB_CONF
    cmd = (
        f"{VAI_PL} --variantLimit=1 --position=chr1:0-1"
        f" --geneTrack={geneTrack}"
        f" --hgvsG=off --hgvsCN=off --hgvsP=on"
        f" {target.hgVaiDb} {vcfGzPath}"
    )
    try:
        stdout, _, _ = runCmd(cmd, timeout=60, allowedExitCodes={0, 9}, env=env)
        for line in stdout.split("\n"):
            if "Connected to UCSC database" in line:
                dbName = line.split("Connected to UCSC database")[-1].strip()
                if not dbName.startswith("hub_"):
                    log.info("  vai.pl resolves %s -> native db %s",
                             target.key, dbName)
                    return dbName
    except RuntimeError:
        pass
    return None


def reconvertVcfChromsForNativeDb(vcfGzPath, target, workDir):
    """When a contrib accession resolves to a native db (e.g. GCF mm39 -> mm39),
    re-translate VCF chroms to UCSC names so hgVai queries the right region."""
    aliasFile = os.path.join(getGenArkHubPath(target.key),
                             f"{target.key}.chromAlias.txt")
    nameToUcsc = {}
    ucscCol = None
    with open(aliasFile) as f:
        for line in f:
            if line.startswith("#"):
                cols = line.lstrip("#").strip().split("\t")
                for i, col in enumerate(cols):
                    if col.strip() == "ucsc":
                        ucscCol = i
                        break
                continue
            if ucscCol is None:
                break
            fields = line.rstrip("\n").split("\t")
            if len(fields) > ucscCol:
                ucscName = fields[ucscCol]
                if ucscName:
                    nameToUcsc[fields[0]] = ucscName
    if not nameToUcsc:
        log.warning("  no native->ucsc mapping built — leaving VCF alone")
        return vcfGzPath, {}
    newVcf = os.path.join(workDir, "converted_ucsc.vcf")
    renamed = kept = 0
    with gzip.open(vcfGzPath, "rt") as inf, open(newVcf, "w") as outf:
        for line in inf:
            if line.startswith("#"):
                outf.write(line)
                continue
            chrom, rest = line.split("\t", 1)
            mapped = nameToUcsc.get(chrom)
            if mapped:
                outf.write(mapped + "\t" + rest)
                renamed += 1
            else:
                outf.write(line)
                kept += 1
    log.info("  reconverted: %d renamed to UCSC, %d kept", renamed, kept)
    return bgzipAndTabix(newVcf), nameToUcsc


def runHgVai(vcfGzPath, hgVaiDb, geneTrack, chromSizes, workDir):
    outPath = os.path.join(workDir, "vep_all.txt")
    env = os.environ.copy()
    env["HGDB_CONF"] = HGDB_CONF
    stdout, _, _ = runCmd(
        f"zcat {vcfGzPath} | (grep -v '^#' || true) | cut -f1 | sort -u",
        timeout=1800,
    )
    vcfChroms = [c for c in stdout.strip().split("\n") if c]
    totalChunks = failedChunks = 0
    with open(outPath, "w") as outf:
        for i, chrom in enumerate(vcfChroms):
            chromSize = chromSizes.get(chrom)
            if chromSize is None:
                log.warning("  chrom %s not in sizes — skipping hgVai", chrom)
                continue
            if (i + 1) % 50 == 0:
                log.info("  hgVai progress: %d/%d chroms", i + 1, len(vcfChroms))
            chunks = []
            if chromSize <= HGVAI_CHUNK_SIZE:
                chunks.append((0, chromSize))
            else:
                start = 0
                while start < chromSize:
                    end = min(start + HGVAI_CHUNK_SIZE, chromSize)
                    chunks.append((start, end))
                    start = end
            for chunkStart, chunkEnd in chunks:
                totalChunks += 1
                cmd = (
                    f"{VAI_PL} --variantLimit=200000000"
                    f" --position={chrom}:{chunkStart}-{chunkEnd}"
                    f" --geneTrack={geneTrack}"
                    f" --hgvsG=off --hgvsCN=off --hgvsP=on"
                    f" {hgVaiDb} {vcfGzPath}"
                )
                try:
                    out, _, rc = runCmd(
                        cmd, timeout=600, allowedExitCodes={0, 9}, env=env,
                    )
                    if rc == 9:
                        failedChunks += 1
                        log.debug("  SIGSEGV on %s:%d-%d (partial kept)",
                                  chrom, chunkStart, chunkEnd)
                    if out:
                        outf.write(out)
                except RuntimeError as e:
                    failedChunks += 1
                    log.warning("  hgVai failed on %s:%d-%d: %s",
                                chrom, chunkStart, chunkEnd, str(e)[:200])
    log.info("  hgVai done: %d chunks, %d failed/partial",
             totalChunks, failedChunks)
    return outPath


# --- BED finalization ---

def parseVep(vepFile):
    vep = {}
    skipped = 0
    with open(vepFile) as f:
        for line in f:
            if any(line.startswith(p) for p in VEP_SKIP_PREFIXES):
                skipped += 1
                continue
            fields = line.rstrip("\n").split("\t")
            if len(fields) < 14:
                skipped += 1
                continue
            rsid = fields[0]
            consequence = fields[6]
            aaChange = ""
            extra = fields[13]
            if extra != "-" and "p." in extra and "?" not in extra:
                for part in extra.split(";"):
                    if "p." in part:
                        pDot = part.split("p.", 1)[1] if "p." in part else ""
                        if pDot:
                            aaChange = "p." + pDot
                            break
            rank = CONSEQUENCE_RANK.get(consequence, 99)
            if rsid not in vep or rank < vep[rsid][0]:
                vep[rsid] = (rank, consequence, aaChange)
    if skipped:
        log.debug("  VEP: skipped %d noise/header lines", skipped)
    log.info("  VEP: %d unique rsIDs annotated", len(vep))
    return vep


def buildFinalBed(bedFile, vepData, workDir):
    outPath = os.path.join(workDir, "evaSnp_final.bed")
    annotated = total = 0
    with open(bedFile) as inf, open(outPath, "w") as outf:
        for line in inf:
            if line.startswith("#"):
                continue
            fields = line.rstrip("\n").split("\t")
            if len(fields) < 14:
                continue
            chrom = fields[0]
            start, end = fields[1], fields[2]
            rsid = fields[3]
            ref, alt = fields[9], fields[10]
            vcSo = fields[12]
            sid = fields[13]
            varClass = SO_TO_CLASS.get(vcSo, SO_FALLBACK_CLASS)
            ucscClass = aaChange = ""
            color = DEFAULT_COLOR
            if rsid in vepData:
                _, ucscClass, aaChange = vepData[rsid]
                color = CONSEQUENCE_COLORS.get(ucscClass, DEFAULT_COLOR)
                annotated += 1
            total += 1
            outf.write(
                f"{chrom}\t{start}\t{end}\t{rsid}\t0\t.\t{start}\t{end}\t{color}"
                f"\t{ref}\t{alt}\t{varClass}\t{sid}\t{ucscClass}\t{aaChange}\n"
            )
    pct = 100.0 * annotated / total if total > 0 else 0
    log.info("  final BED: %d total, %d annotated (%.1f%%)",
             total, annotated, pct)
    return outPath, total, annotated


def validateFinalBed(finalBed, vcfGzPath, chromSizes):
    stdout, _, _ = runCmd(
        f"zcat {vcfGzPath} | (grep -v '^#' || true) | cut -f3 | sort -u | wc -l",
        timeout=1800,
    )
    vcfRsidCount = int(stdout.strip())
    stdout, _, _ = runCmd(f"cut -f4 {finalBed} | sort -u | wc -l", timeout=1800)
    bedRsidCount = int(stdout.strip())
    log.info("  validation: VCF rsIDs=%d, BED rsIDs=%d",
             vcfRsidCount, bedRsidCount)

    filteredPath = finalBed + ".filtered"
    removedChrom = 0
    removedCoordByChrom = {}
    kept = 0
    with open(finalBed) as inf, open(filteredPath, "w") as outf:
        for line in inf:
            fields = line.split("\t")
            chrom = fields[0]
            chromSize = chromSizes.get(chrom)
            if chromSize is None:
                removedChrom += 1
                continue
            endPos = int(fields[2])
            if endPos > chromSize:
                e = removedCoordByChrom.get(chrom, (0, 0, chromSize))
                removedCoordByChrom[chrom] = (e[0] + 1, max(e[1], endPos),
                                              chromSize)
                continue
            outf.write(line)
            kept += 1
    removedCoord = sum(v[0] for v in removedCoordByChrom.values())
    if removedChrom or removedCoord:
        if removedChrom:
            log.warning("  removed %d lines with unknown chroms", removedChrom)
        if removedCoord:
            log.warning("  removed %d lines past chrom end:", removedCoord)
            for chrom, (cnt, maxEnd, size) in sorted(
                    removedCoordByChrom.items(), key=lambda x: -x[1][0]):
                log.warning("    %s: %d variants, chromSize=%d, maxEnd=%d",
                            chrom, cnt, size, maxEnd)
        os.replace(filteredPath, finalBed)
    else:
        os.remove(filteredPath)
    log.info("  validated: %d BED lines kept", kept)
    return finalBed


def _bigBedItemCount(bbPath):
    """Return the itemCount from `bigBedInfo` output, or None on failure."""
    try:
        stdout, _, _ = runCmd(f"bigBedInfo {bbPath}", timeout=30)
    except RuntimeError:
        return None
    for line in stdout.splitlines():
        if line.startswith("itemCount:"):
            return int(line.split(":", 1)[1].strip().replace(",", ""))
    return None


def validateAgainstPriorRelease(target, bbFile):
    """Reject a build whose item count is well below the previous release's.

    Belt-and-braces against EVA-version selection accidents on multi-version
    native dbs. If the previous release for this key built substantially more
    items, the new build almost certainly picked the wrong EVA VCF.
    """
    priorVersion = str(int(VERSION) - 1)
    if target.kind == "native":
        priorPath = os.path.join(GBDB_BASE, target.key, "bbi",
                                 f"evaSnp{priorVersion}.bb")
    else:
        priorPath = os.path.join(
            f"/hive/data/outside/eva{priorVersion}/contributedTracks",
            target.key, f"evaSnp{priorVersion}.bb")
    if not os.path.exists(priorPath):
        log.info("  prior-release check: no evaSnp%s.bb at %s — skipping",
                 priorVersion, priorPath)
        return
    priorCount = _bigBedItemCount(priorPath)
    currentCount = _bigBedItemCount(bbFile)
    if priorCount is None or currentCount is None:
        log.warning("  prior-release check: could not read item counts")
        return
    if priorCount < 1000:
        log.info("  prior-release check: prior had only %d items — skipping",
                 priorCount)
        return
    ratio = currentCount / priorCount
    log.info("  prior-release check: this=%d, evaSnp%s=%d (%.1f%%)",
             currentCount, priorVersion, priorCount, 100.0 * ratio)
    if ratio < PRIOR_RELEASE_MIN_RATIO:
        # Remove the bigBed so the quarantine path catches this build as failed
        # rather than letting it sit there marked "complete" for the next run.
        try:
            os.remove(bbFile)
        except OSError:
            pass
        raise RuntimeError(
            f"item-count regression vs evaSnp{priorVersion}: this build has "
            f"{currentCount:,} items, previous release had {priorCount:,} "
            f"({100.0 * ratio:.1f}%, threshold "
            f"{100.0 * PRIOR_RELEASE_MIN_RATIO:.0f}%). Almost certainly the "
            f"discovery step picked the wrong EVA VCF version."
        )


def createBigBed(finalBed, target, chromSizes, workDir):
    sizesFile = os.path.join(workDir, "chrom.sizes")
    with open(sizesFile, "w") as f:
        for chrom, size in chromSizes.items():
            f.write(f"{chrom}\t{size}\n")
    sortedBed = os.path.join(workDir, "evaSnp_sorted.bed")
    bbFile = os.path.join(workDir, f"{TRACK_NAME}.bb")
    runCmd(f"bedSort {finalBed} {sortedBed}", timeout=3600)
    runCmd(
        f"bedToBigBed -tab -as={EVASNP_AS} -type=bed9+6"
        f" -extraIndex=name {sortedBed} {sizesFile} {bbFile}",
        timeout=1800,
    )
    if not os.path.exists(bbFile):
        raise RuntimeError(f"bedToBigBed did not produce {bbFile}")
    stdout, _, _ = runCmd(f"bigBedInfo {bbFile}", timeout=30)
    log.info("  bigBed created: %s", bbFile)
    for infoLine in stdout.strip().split("\n"):
        if any(k in infoLine for k in ("itemCount", "fieldCount", "chromCount")):
            log.info("    %s", infoLine.strip())
    return bbFile


# --- Per-assembly pipeline (kind-agnostic) ---

def processOne(target):
    """Run the per-assembly pipeline; on REF validation failure, fall back to
    the next-largest EVA candidate (relevant for natives with multi-version
    EVA-9 listings where the largest VCF is the wrong assembly variant)."""
    bbFile = os.path.join(target.workDir, f"{TRACK_NAME}.bb")
    if os.path.exists(bbFile):
        log.info("  SKIPPING: bigBed already exists")
        return {"status": "skipped", "bbFile": bbFile}

    candidates = getattr(target, "evaGcaCandidates", None)
    if not candidates:
        candidates = [(target.evaGca, target.versionMismatch)]
    for idx, (candGca, candMm) in enumerate(candidates):
        if idx > 0:
            log.warning("REF validation failed for previous candidate — "
                        "retrying with %s (#%d of %d)",
                        candGca, idx + 1, len(candidates))
            for fname in ("converted.vcf", "converted.vcf.gz",
                          "converted.vcf.gz.tbi", "converted_ucsc.vcf.gz",
                          "converted_ucsc.vcf.gz.tbi", "evaSnp.bed",
                          "evaSnp_final.bed", "evaSnp_sorted.bed",
                          "vep_all.txt"):
                fpath = os.path.join(target.workDir, fname)
                if os.path.exists(fpath):
                    os.remove(fpath)
        target.evaGca = candGca
        target.versionMismatch = candMm
        try:
            return _processOneAttempt(target)
        except RefValidationError as e:
            if idx == len(candidates) - 1:
                raise
            log.warning("  candidate %s rejected: %s", candGca, e)
    raise RuntimeError("processOne: exhausted all candidates without success")


def _processOneAttempt(target):
    """Single-EVA-version attempt at the per-assembly pipeline. See processOne
    for the retry-on-REF-failure wrapper."""
    label = f"{target.kind:>7} {target.key} (<-{target.evaGca}" + (
        " MM" if target.versionMismatch else "") + ")"
    log.info("=" * 70)
    log.info("Processing: %s", label)
    log.info("  work dir: %s", target.workDir)

    bbFile = os.path.join(target.workDir, f"{TRACK_NAME}.bb")
    os.makedirs(target.workDir, exist_ok=True)

    # 1. resolve + download VCF
    log.info("Step 1: resolve EVA VCF URL...")
    vcfUrl = resolveEvaVcfUrl(target.evaGca)
    log.info("  %s", vcfUrl)
    log.info("Step 2: download VCF...")
    vcfGzPath = downloadVcf(vcfUrl, target.workDir)

    # 3. chrom map
    log.info("Step 3: load chrom map...")
    chromMap = target.loadChromMap()
    log.info("  %d name mappings from assembly", len(chromMap))
    orphans = enrichChromMapFromVcfContigs(vcfGzPath, chromMap)
    chromInfoNames = list(target.loadChromSizes().keys())
    mitoChrom = _findMitoChrom(chromInfoNames)
    if orphans and mitoChrom:
        bridgeMitochondrialOrphan(vcfGzPath, chromMap, orphans, mitoChrom)
    log.info("  %d name mappings after enrichment", len(chromMap))

    # Chrom-coverage gate — same threshold applied to every build regardless
    # of version-mismatch flag, so a non-mismatch target with surprisingly
    # bad chrom alignment fails fast instead of producing a near-empty bigBed.
    stdout, _, _ = runCmd(
        f"zcat {vcfGzPath} | (grep -v '^#' || true) | cut -f1 | sort -u",
        timeout=1800,
    )
    evaChroms = {c for c in stdout.strip().split("\n") if c}
    mapped = sum(1 for c in evaChroms if c in chromMap)
    pct = 100.0 * mapped / max(len(evaChroms), 1)
    chromsSeenPre = len(evaChroms)
    chromsUnmappedPre = len(evaChroms) - mapped
    pctUnmappedPre = 100.0 - pct
    flag = "version mismatch" if target.versionMismatch else "chrom coverage"
    log.info("  %s: %d/%d chroms map (%.1f%%)",
             flag, mapped, len(evaChroms), pct)
    if pct < VERSION_MISMATCH_BUILD_THRESHOLD:
        raise RuntimeError(
            f"chrom coverage too low: only {pct:.1f}% chroms map "
            f"(threshold {VERSION_MISMATCH_BUILD_THRESHOLD}%)"
        )

    # 4. convert + dedup
    log.info("Step 4: convert chrom names + dedup...")
    convertedVcf, _, _, _ = convertAndDeduplicateVcf(
        vcfGzPath, chromMap, target.workDir)

    # 4b. ref validation
    log.info("Step 4b: validate ref alleles...")
    validateRefAlleles(convertedVcf, target.twoBit)

    # 5. bgzip + tabix
    log.info("Step 5: bgzip + tabix...")
    convertedVcfGz = bgzipAndTabix(convertedVcf)

    # 6. vcfToBed
    log.info("Step 6: vcfToBed...")
    bedFile = vcfToBed(convertedVcfGz, target.workDir)
    stdout, _, _ = runCmd(f"wc -l < {bedFile}", timeout=300)
    log.info("  BED has %s lines", stdout.strip())

    # 7. gene track
    log.info("Step 7: pick gene track...")
    geneTrack = target.getGeneTrack()
    if geneTrack:
        log.info("  using gene track: %s", geneTrack)
    else:
        log.warning("  no gene track — hgVai will be skipped")

    # 8. hgVai
    chromSizes = target.loadChromSizes()
    vepData = {}
    if geneTrack:
        nativeDb = probeVaiResolvesNative(target, convertedVcfGz, geneTrack)
        if nativeDb:
            log.info("  reconverting to UCSC names for native db %s",
                     nativeDb)
            convertedVcfGz, ucscRename = reconvertVcfChromsForNativeDb(
                convertedVcfGz, target, target.workDir)
            if ucscRename:
                chromSizes = {ucscRename.get(k, k): v
                              for k, v in chromSizes.items()}
        log.info("Step 8: run hgVai...")
        vepFile = runHgVai(convertedVcfGz, target.hgVaiDb, geneTrack,
                           chromSizes, target.workDir)
        log.info("Step 9a: parse VEP...")
        vepData = parseVep(vepFile)
    else:
        log.info("Steps 8-9a: skipped (no gene track)")

    # 9b. final BED
    log.info("Step 9b: build final BED...")
    finalBed, totalVariants, annotatedVariants = buildFinalBed(
        bedFile, vepData, target.workDir)
    annotationPct = (100.0 * annotatedVariants / totalVariants
                     if totalVariants > 0 else 0.0)

    # 10. validate + bigBed (use the kind's native-naming chrom.sizes)
    log.info("Step 10: validate + bigBed...")
    chromSizesNative = target.loadChromSizes()
    finalBed = validateFinalBed(finalBed, convertedVcfGz, chromSizesNative)
    bbFile = createBigBed(finalBed, target, chromSizesNative, target.workDir)

    # 10b. cross-check against prior release item count
    log.info("Step 10b: validate against prior release...")
    validateAgainstPriorRelease(target, bbFile)

    # 11. cleanup intermediates
    log.info("Step 11: clean up intermediates...")
    for fname in ("converted.vcf", "converted.vcf.gz", "converted.vcf.gz.tbi",
                  "evaSnp.bed", "evaSnp_final.bed", "evaSnp_sorted.bed",
                  "vep_all.txt", "chrom.sizes",
                  "converted_ucsc.vcf.gz", "converted_ucsc.vcf.gz.tbi"):
        fpath = os.path.join(target.workDir, fname)
        if os.path.exists(fpath):
            os.remove(fpath)

    # Capture some bigBed stats for the contrib description.html.
    itemCount = chromCount = annotatedVariants = totalVariants = 0
    try:
        stdout, _, _ = runCmd(f"bigBedInfo {bbFile}", timeout=30)
        for line in stdout.splitlines():
            if line.startswith("itemCount:"):
                itemCount = int(line.split(":", 1)[1].strip().replace(",", ""))
            elif line.startswith("chromCount:"):
                chromCount = int(line.split(":", 1)[1].strip().replace(",", ""))
    except RuntimeError:
        pass

    log.info("DONE: %s", bbFile)
    return {
        "status": "succeeded",
        "bbFile": bbFile,
        "chromsSeen": chromsSeenPre,
        "chromsUnmapped": chromsUnmappedPre,
        "pctUnmapped": pctUnmappedPre,
        "itemCount": itemCount,
        "chromCount": chromCount,
        "geneTrack": geneTrack or "none",
        "totalVariants": totalVariants,
        "annotatedVariants": annotatedVariants,
        "annotationPct": f"{annotationPct:.1f}",
    }


# --- Failure handling ---

def _quarantineFailedDir(workDir):
    """Move a failed build dir aside so the pipeline.log survives for debugging.

    Removes any prior <workDir>.failed first so repeated retries don't pile up.
    Preserves the log but lets the next build attempt start clean.
    """
    failedDir = workDir + ".failed"
    if os.path.exists(failedDir):
        shutil.rmtree(failedDir)
    os.rename(workDir, failedDir)


# --- Parallel worker ---

def _workerProcess(args):
    target, verbose = args
    bbFile = os.path.join(target.workDir, f"{TRACK_NAME}.bb")
    if os.path.exists(bbFile):
        return target.kind, target.key, "skipped", {}, None
    os.makedirs(target.workDir, exist_ok=True)
    asmLog = logging.getLogger(f"{TRACK_NAME}.{target.key}")
    asmLog.setLevel(logging.DEBUG if verbose else logging.INFO)
    asmLog.propagate = False
    logFile = os.path.join(target.workDir, "pipeline.log")
    fh = logging.FileHandler(logFile, mode="w")
    fh.setFormatter(logging.Formatter(
        "%(asctime)s %(levelname)s %(message)s", datefmt="%Y-%m-%d %H:%M:%S"))
    asmLog.addHandler(fh)
    global log
    origLog = log
    log = asmLog
    try:
        result = processOne(target)
        return target.kind, target.key, result["status"], result, None
    except Exception as e:
        asmLog.error("FAILED: %s", e)
        return target.kind, target.key, "failed", {}, str(e)
    finally:
        log = origLog
        fh.close()
        asmLog.removeHandler(fh)
        if not os.path.exists(bbFile) and os.path.isdir(target.workDir):
            _quarantineFailedDir(target.workDir)


# --- Orchestration ---

def runBuild(targets, numWorkers, verbose):
    succeeded = failed = skipped = 0
    failures = []
    versionMismatchAtBuild = []   # (kind, key, pctUnmapped)
    chromUnmappedFlags = []       # (kind, key, pctUnmapped)
    start = time.time()
    n = len(targets)

    if numWorkers == 1:
        for t in targets:
            bbFile = os.path.join(t.workDir, f"{TRACK_NAME}.bb")
            if os.path.exists(bbFile):
                skipped += 1
                log.info("SKIP (already done): %s/%s", t.kind, t.key)
                continue
            os.makedirs(t.workDir, exist_ok=True)
            fh = logging.FileHandler(os.path.join(t.workDir, "pipeline.log"),
                                     mode="w")
            fh.setFormatter(logging.Formatter(
                "%(asctime)s %(levelname)s %(message)s",
                datefmt="%Y-%m-%d %H:%M:%S"))
            log.addHandler(fh)
            try:
                result = processOne(t)
                if result["status"] == "skipped":
                    skipped += 1
                else:
                    succeeded += 1
                    pct = result.get("pctUnmapped", 0.0)
                    if t.versionMismatch:
                        versionMismatchAtBuild.append((t.kind, t.key, pct))
                    if pct > VERSION_MISMATCH_REPORT_THRESHOLD:
                        chromUnmappedFlags.append((t.kind, t.key, pct))
                    if t.kind == "contrib":
                        writeContribAssemblyFiles(t, result)
            except Exception as e:
                failed += 1
                failures.append((t.kind, t.key, t.evaGca, str(e)))
                log.error("FAILED: %s/%s: %s", t.kind, t.key, e)
            finally:
                fh.close()
                log.removeHandler(fh)
                bb = os.path.join(t.workDir, f"{TRACK_NAME}.bb")
                if not os.path.exists(bb) and os.path.isdir(t.workDir):
                    _quarantineFailedDir(t.workDir)
    else:
        items = [(t, verbose) for t in targets]
        with concurrent.futures.ProcessPoolExecutor(
                max_workers=numWorkers) as ex:
            futureMap = {ex.submit(_workerProcess, it): it for it in items}
            for future in concurrent.futures.as_completed(futureMap):
                t = futureMap[future][0]
                try:
                    kind, key, status, result, err = future.result()
                except Exception as e:
                    failed += 1
                    failures.append((t.kind, t.key, t.evaGca, str(e)))
                    log.error("FAIL %s/%s: %s", t.kind, t.key, str(e)[:200])
                    continue
                done = succeeded + failed + skipped
                if status == "succeeded":
                    succeeded += 1
                    pct = result.get("pctUnmapped", 0.0)
                    if t.versionMismatch:
                        versionMismatchAtBuild.append((kind, key, pct))
                    if pct > VERSION_MISMATCH_REPORT_THRESHOLD:
                        chromUnmappedFlags.append((kind, key, pct))
                    if t.kind == "contrib":
                        writeContribAssemblyFiles(t, result)
                    log.info("OK   [%d/%d] %s/%s (%.0fs)",
                             done + 1, n, kind, key, time.time() - start)
                elif status == "skipped":
                    skipped += 1
                    log.info("SKIP [%d/%d] %s/%s", done + 1, n, kind, key)
                else:
                    failed += 1
                    failures.append((kind, key, t.evaGca, err))
                    log.error("FAIL [%d/%d] %s/%s: %s",
                              done + 1, n, kind, key,
                              (err or "unknown")[:200])

    elapsed = time.time() - start
    log.info("=" * 70)
    log.info("SUMMARY: %d succeeded, %d failed, %d skipped (of %d) in %.0fs",
             succeeded, failed, skipped, n, elapsed)
    if versionMismatchAtBuild:
        log.info("Version-mismatched builds (passed %.0f%% threshold):",
                 VERSION_MISMATCH_BUILD_THRESHOLD)
        for kind, key, pct in sorted(versionMismatchAtBuild):
            log.info("  %s/%s — %.1f%% chroms unmapped", kind, key, pct)
    if chromUnmappedFlags:
        log.warning("Builds with >%.0f%% chroms unmapped — please QA-spot-check:",
                    VERSION_MISMATCH_REPORT_THRESHOLD)
        for kind, key, pct in sorted(chromUnmappedFlags):
            log.warning("  %s/%s — %.1f%% chroms unmapped", kind, key, pct)
    if failures:
        log.info("Failed:")
        for kind, key, evaGca, err in failures:
            log.info("  %s/%s (%s): %s", kind, key, evaGca, (err or "")[:150])

    # Per-assembly match-rate report — written as TSV under OUTPUT_BASE.
    # Covers every target from buildMatchList that has a completed bigBed,
    # regardless of which build pass produced it. Reads pipeline.log for the
    # source-VCF size and chrom-convert stats; reads the bigBed for the final
    # item count and items-with-annotation count.
    writeBuildReport(targets)


# --- Build report (per-assembly match-rate audit) ---

def _statsFromPipelineLog(logPath):
    """Pull source-VCF and chrom-convert counters from a per-assembly log.

    Returns a dict with whatever could be parsed; missing fields are None.
    """
    info = {"vcfLines": None, "mapped": None, "unmapped": None, "dupes": None,
            "finalTotal": None, "annotated": None, "annotationPct": None,
            "geneTrack": None}
    if not os.path.exists(logPath):
        return info
    with open(logPath, errors="replace") as f:
        for line in f:
            m = re.search(r"chrom convert: (\d+) mapped, (\d+) unmapped, "
                          r"(\d+) dupes \(of (\d+)(?: total)?\)", line)
            if m:
                info["mapped"] = int(m.group(1))
                info["unmapped"] = int(m.group(2))
                info["dupes"] = int(m.group(3))
                info["vcfLines"] = int(m.group(4))
                continue
            m = re.search(r"final BED: (\d+) total, (\d+) annotated "
                          r"\(([0-9.]+)%\)", line)
            if m:
                info["finalTotal"] = int(m.group(1))
                info["annotated"] = int(m.group(2))
                info["annotationPct"] = m.group(3)
                continue
            m = re.search(r"using gene track: (\S+)", line)
            if m:
                info["geneTrack"] = m.group(1)
    return info


def _v8PriorItemCount(target):
    """Look up the previous release's bigBed item count for this target."""
    priorVersion = str(int(VERSION) - 1)
    if target.kind == "native":
        path = os.path.join(GBDB_BASE, target.key, "bbi",
                            f"evaSnp{priorVersion}.bb")
    else:
        path = os.path.join(
            f"/hive/data/outside/eva{priorVersion}/contributedTracks",
            target.key, f"evaSnp{priorVersion}.bb")
    if not os.path.exists(path):
        return None
    return _bigBedItemCount(path)


def writeBuildReport(targets):
    """Write /hive/data/outside/eva{VERSION}/buildReport.tsv.

    One row per AssemblyTarget that has a completed bigBed. Columns:
      kind, key, evaGca, vcf_lines, v9_items, match_pct, v9_v8_pct,
      annotation_pct, gene_track, status

    `match_pct` = v9_items / vcf_lines * 100 — how much of the source EVA
    VCF survived through chrom mapping, dedup, and validation.
    `v9_v8_pct` = v9_items / v8_items * 100 — pure-growth check (EVA should
    only add data between releases).
    """
    reportPath = os.path.join(OUTPUT_BASE, "buildReport.tsv")
    with open(reportPath, "w") as out:
        out.write("kind\tkey\teva_gca\tvcf_lines\tv9_items\tmatch_pct"
                  "\tv8_items\tv9_v8_pct\tannotation_pct\tgene_track\tstatus\n")
        for t in sorted(targets, key=lambda x: (x.kind, x.key)):
            bb = os.path.join(t.workDir, f"{TRACK_NAME}.bb")
            failedDir = t.workDir + ".failed"
            if os.path.exists(bb):
                v9Items = _bigBedItemCount(bb)
                logInfo = _statsFromPipelineLog(
                    os.path.join(t.workDir, "pipeline.log"))
                status = "succeeded"
            elif os.path.isdir(failedDir):
                v9Items = None
                logInfo = _statsFromPipelineLog(
                    os.path.join(failedDir, "pipeline.log"))
                status = "failed"
            else:
                v9Items = None
                logInfo = _statsFromPipelineLog(
                    os.path.join(t.workDir, "pipeline.log"))
                status = "not-built"
            vcfLines = logInfo.get("vcfLines")
            matchPct = ""
            if v9Items is not None and vcfLines and vcfLines > 0:
                matchPct = f"{100.0 * v9Items / vcfLines:.2f}"
            v8Items = _v8PriorItemCount(t)
            v9v8 = ""
            if v9Items is not None and v8Items and v8Items > 0:
                v9v8 = f"{100.0 * v9Items / v8Items:.2f}"
            out.write(
                f"{t.kind}\t{t.key}\t{t.evaGca}\t"
                f"{vcfLines or ''}\t{v9Items if v9Items is not None else ''}\t"
                f"{matchPct}\t{v8Items if v8Items else ''}\t{v9v8}\t"
                f"{logInfo.get('annotationPct') or ''}\t"
                f"{logInfo.get('geneTrack') or ''}\t{status}\n"
            )
    log.info("Build report: %s", reportPath)

    # Inline summary of suspect rows
    suspects = []
    with open(reportPath) as f:
        next(f)  # header
        for line in f:
            fields = line.rstrip("\n").split("\t")
            if len(fields) < 11:
                continue
            kind, key, _, _, _, mp, _, vp, _, _, status = fields
            if status != "succeeded":
                continue
            try:
                mpf = float(mp) if mp else 100.0
                vpf = float(vp) if vp else 100.0
            except ValueError:
                continue
            if mpf < 99.0 or vpf < 100.0:
                suspects.append((kind, key, mpf, vpf))
    if suspects:
        log.warning("%d builds below 99%% match or below 100%% of v8 — "
                    "QA-spot-check:", len(suspects))
        for kind, key, mp, vp in sorted(suspects, key=lambda x: x[2]):
            log.warning("  %s/%s  match=%.2f%%  v9/v8=%.2f%%", kind, key, mp, vp)


# --- Standalone hub (hub.txt + genomes.txt + documentation.html) ---

ASMHUB_LIST = "/hive/data/genomes/asmHubs/UCSC_GI.assemblyHubList.txt"


def _loadGenArkResolver():
    """Build (accSet, baseIndex) from the live GenArk API for resolving
    GCA -> GCF when both exist.

    The browser resolves GCA accessions to their GCF equivalents via asmAlias
    when both are in GenArk, so the standalone hub's `genome` line must use
    whichever accession GenArk actually serves. The historical static file
    UCSC_GI.assemblyHubList.txt is stale (2022); the live API has ~50k.
    """
    genomes = fetchGenArkAssemblies()
    accSet = set(genomes.keys())
    baseIndex = {acc.rsplit(".", 1)[0]: acc for acc in accSet}
    return accSet, baseIndex


def _resolveGenArkAccession(dirAccession, genarkSet, genarkByBase):
    """Map a per-assembly directory accession to the accession GenArk serves.

    Mirrors buildContribHub.py's resolveGenArkAccession from RM36957: prefer
    GCF over GCA when both exist, then try same-base alternates.
    """
    if dirAccession in genarkSet:
        if dirAccession.startswith("GCA_"):
            base = dirAccession.split("_", 1)[1].rsplit(".", 1)[0]
            gcfBase = "GCF_" + base
            if gcfBase in genarkByBase:
                return genarkByBase[gcfBase]
        return dirAccession
    base = dirAccession.split("_", 1)[1].rsplit(".", 1)[0]
    for prefix in ("GCF_", "GCA_"):
        b = prefix + base
        if b in genarkByBase:
            return genarkByBase[b]
    return None


def writeHubTxt():
    path = os.path.join(CONTRIB_DIR, "hub.txt")
    with open(path, "w") as f:
        f.write(
            f"hub {TRACK_NAME}\n"
            f"shortLabel EVA SNP Release {VERSION}\n"
            f"longLabel Short Genetic Variants from European Variation Archive Release {VERSION}\n"
            f"genomesFile genomes.txt\n"
            f"email genome-www@soe.ucsc.edu\n"
            f"descriptionUrl documentation.html\n"
        )
    log.info("  wrote %s", path)


def writeGenomesTxt():
    """Write genomes.txt — one entry per completed contrib build, with the
    `genome` line set to the GenArk-resolved accession."""
    genarkSet, genarkByBase = _loadGenArkResolver()
    path = os.path.join(CONTRIB_DIR, "genomes.txt")
    written = 0
    skipped = []
    with open(path, "w") as f:
        for d in sorted(os.listdir(CONTRIB_DIR)):
            asmDir = os.path.join(CONTRIB_DIR, d)
            if not os.path.isdir(asmDir):
                continue
            if not os.path.exists(os.path.join(asmDir, f"{TRACK_NAME}.bb")):
                continue
            resolved = _resolveGenArkAccession(d, genarkSet, genarkByBase)
            if resolved is None:
                skipped.append(d)
                continue
            f.write(f"genome {resolved}\n")
            f.write(f"trackDb {d}/trackDb.txt\n\n")
            written += 1
    log.info("  wrote %s (%d genomes)", path, written)
    if skipped:
        log.warning("  %d contrib dirs skipped (not in GenArk hub list): %s",
                    len(skipped),
                    ", ".join(skipped[:5]) + ("..." if len(skipped) > 5 else ""))


def writeDocumentationHtml():
    path = os.path.join(CONTRIB_DIR, "documentation.html")
    with open(path, "w") as f:
        f.write(_renderHubDocumentationHtml())
    log.info("  wrote %s", path)


def _renderHubDocumentationHtml():
    """Hub-level documentation.html — describes the whole release, not a
    single assembly. Linked from hub.txt's descriptionUrl."""
    return f"""\
<h2>Description</h2>
<p>
This track contains mappings of single nucleotide variants and small
insertions and deletions (indels) from the European Variation Archive
(<a href="https://www.ebi.ac.uk/eva/" target="_blank">EVA</a>) Release {VERSION}
for GenArk assemblies. The dbSNP database at NCBI no longer hosts non-human
variants.
</p>

<h2>Interpreting and Configuring the Graphical Display</h2>
<p>
Variants are shown as single tick marks at most zoom levels. When viewing the
track at or near base-level resolution, the displayed width corresponds to
the width of the variant in the reference sequence. Insertions are indicated
by a single tick mark between two nucleotides; single nucleotide variants
display as one base wide; multiple-nucleotide variants span two or more
bases. Display automatically collapses to dense visibility when more than
100k variants are in view; above 250 kb of window, the display switches to
density-graph mode.
</p>

<h3>Searching, details, and filtering</h3>
<p>
Navigation to an individual variant can be done by typing or pasting the
rsID or genomic coordinates into the Position/Search box. Clicking an item
opens a details page with the Reference and Alternate alleles, the EVA
variant class, the source study, any amino acid change, and the functional
class predicted by UCSC's Variant Annotation Integrator.
</p>
<p>
Variants can be filtered using the track controls by EVA Sequence Ontology
term, UCSC-generated functional effect, or color.
</p>

<h3>Mouse-over</h3>
<p>
Mousing over an item shows the ucscClass (consequence per the
<a target="_blank" href="/cgi-bin/hgVai">Variant Annotation Integrator</a>)
and the aaChange in HGVS.p form when available. Starting with EVA SNP
Release {VERSION}, each variant carries the single most-severe consequence
ranked by Sequence Ontology severity, so ucscClass is one term per variant.
Earlier releases (3 through 8) listed every overlapping consequence in a
comma-separated string.
</p>
<p>
The gene model used for functional annotation varies by assembly based on
availability, with the following priority:
ncbiRefSeqCurated, ncbiRefSeq, ncbiGene, ensGene, xenoRefGene, augustus.
</p>

<h3>Track colors</h3>
<p>
Variants are colored by the most potentially deleterious functional effect
predicted by the Variant Annotation Integrator:
</p>
<table cellpadding='2'>
  <thead><tr>
    <th style="border-bottom: 2px solid;">Color</th>
    <th style="border-bottom: 2px solid;">Variant Type</th>
  </tr></thead>
  <tr><td style="background-color: red">&nbsp;&nbsp;</td><td>Protein-altering variants and splice site variants</td></tr>
  <tr><td style="background-color: green">&nbsp;&nbsp;</td><td>Synonymous codon variants</td></tr>
  <tr><td style="background-color: blue">&nbsp;&nbsp;</td><td>Non-coding transcript or UTR variants</td></tr>
  <tr><td style="background-color: black">&nbsp;&nbsp;</td><td>Intergenic and intronic variants</td></tr>
</table>

<h3>Sequence Ontology (SO) variant classes</h3>
<p>
Starting with EVA SNP Release 9, the variant class labels follow the short
Sequence Ontology names used by modern VCF / dbSNP exports (<b>SNV</b>,
<b>deletion</b>, <b>insertion</b>, <b>indel</b>, <b>MNV</b>,
<b>sequence_alteration</b>). Releases 3 through 8 retained their original
labels (<b>substitution</b>, <b>delins</b>, <b>multipleNucleotideVariant</b>,
etc.). The underlying SO terms and the biology they describe are unchanged.
</p>
<ul>
  <li><b>SNV</b> &mdash; single nucleotide variant.</li>
  <li><b>deletion</b> &mdash; one or more nucleotides deleted (Ref = GA, Alt = G).</li>
  <li><b>insertion</b> &mdash; one or more nucleotides inserted (Ref = G, Alt = GT).</li>
  <li><b>indel</b> &mdash; reference and alternate alleles differ in length and content.</li>
  <li><b>MNV</b> &mdash; multiple nucleotide variant (Ref = AA, Alt = GC).</li>
  <li><b>sequence_alteration</b> &mdash; generic class for uncharacterized variants.</li>
</ul>

<h2>Methods</h2>
<p>
Data were downloaded from the European Variation Archive
<a href="https://ftp.ebi.ac.uk/pub/databases/eva/rs_releases/release_{VERSION}/by_assembly/" target="_blank">release {VERSION}</a>
<code>current_ids.vcf.gz</code> files corresponding to each assembly.
Chromosome names were converted to the assembly's native naming convention
using GenArk chromAlias tables. Variants were passed through the
<a target="_blank" href="/cgi-bin/hgVai">Variant Annotation Integrator</a>
to predict consequence. The gene model used varies by assembly based on
availability.
</p>

<h2>Data Access</h2>
<p>
Per-assembly bigBed files live under
<code>/hive/data/outside/eva{VERSION}/contributedTracks/&lt;accession&gt;/{TRACK_NAME}.bb</code>.
Individual regions can be obtained via
<a href="https://hgdownload.soe.ucsc.edu/downloads.html#utilities_downloads"
target="_blank"><tt>bigBedToBed</tt></a>.
</p>

<h2>Credits</h2>
<p>
Produced from the <a target="_blank" href="https://www.ebi.ac.uk/eva/">European
Variation Archive release {VERSION}</a> data. Consequences were predicted using
UCSC's Variant Annotation Integrator and the best available gene model for
each assembly.
</p>

<h2>References</h2>
<p>
Cezard T, Cunningham F, Hunt SE, Koylass B, Kumar N, Saunders G, Shen A,
Silva AF, Tsukanov K, Venkataraman S <em>et al.</em>
<a href="https://doi.org/10.1093/nar/gkab960" target="_blank">The European
Variation Archive: a FAIR resource of genomic variation for all species</a>.
<em>Nucleic Acids Res.</em> 2021. PMID:
<a href="https://pubmed.ncbi.nlm.nih.gov/34718739/" target="_blank">34718739</a>.
</p>
<p>
Hinrichs AS, Raney BJ, Speir ML, Rhead B, Casper J, Karolchik D, Kuhn RM,
Rosenbloom KR, Zweig AS, Haussler D, Kent WJ.
<a href="https://academic.oup.com/bioinformatics/article/32/9/1430/1744314/" target="_blank">UCSC
Data Integrator and Variant Annotation Integrator</a>.
<em>Bioinformatics</em>. 2016;32(9):1430-2. PMID:
<a href="https://www.ncbi.nlm.nih.gov/pubmed/26740527" target="_blank">26740527</a>.
</p>
"""


def writeStandaloneHub():
    """Write hub.txt + genomes.txt + documentation.html for the standalone
    contrib hub at /hive/data/outside/eva{VERSION}/contributedTracks/."""
    if not os.path.isdir(CONTRIB_DIR):
        log.warning("No contrib dir at %s — nothing to do", CONTRIB_DIR)
        return
    log.info("Writing standalone hub files under %s ...", CONTRIB_DIR)
    writeHubTxt()
    writeGenomesTxt()
    writeDocumentationHtml()
    # Refresh per-assembly trackDb.txt so they use the standalone-hub layout
    # (paths relative to the per-assembly directory, not contrib/<TRACK>/).
    refreshed = 0
    for d in sorted(os.listdir(CONTRIB_DIR)):
        asmDir = os.path.join(CONTRIB_DIR, d)
        if not os.path.isdir(asmDir):
            continue
        if not os.path.exists(os.path.join(asmDir, f"{TRACK_NAME}.bb")):
            continue
        with open(os.path.join(asmDir, "trackDb.txt"), "w") as f:
            f.write(STANDALONE_TRACKDB_TEMPLATE)
        refreshed += 1
    log.info("  refreshed %d per-assembly trackDb.txt to standalone layout",
             refreshed)


# --- Deployment ---

def deployNative(targets):
    """For each native build, ensure /gbdb/<db>/bbi/<TRACK_NAME>.bb symlink exists.
    Write assemblyReleaseList.txt for the makedoc record."""
    if not targets:
        log.warning("No native targets to deploy.")
        return
    deployed = []
    for t in targets:
        bbSrc = os.path.join(t.workDir, f"{TRACK_NAME}.bb")
        if not os.path.exists(bbSrc):
            log.warning("  no bigBed for %s — skipping deploy", t.key)
            continue
        gbdbDir = os.path.join(GBDB_BASE, t.key, "bbi")
        if not os.path.isdir(gbdbDir):
            log.warning("  %s does not exist — skipping", gbdbDir)
            continue
        gbdbLink = os.path.join(gbdbDir, f"{TRACK_NAME}.bb")
        if os.path.islink(gbdbLink) or os.path.exists(gbdbLink):
            existing = (os.path.realpath(gbdbLink) if os.path.islink(gbdbLink)
                        else gbdbLink)
            if os.path.realpath(bbSrc) == existing:
                log.info("  %s already symlinked", t.key)
                deployed.append(t.key)
                continue
            log.warning("  %s exists but doesn't match — leaving alone", gbdbLink)
            continue
        os.symlink(bbSrc, gbdbLink)
        log.info("  symlinked %s -> %s", gbdbLink, bbSrc)
        deployed.append(t.key)
    if not deployed:
        log.warning("No native bigBeds deployed — release list NOT written.")
        return
    listPath = os.path.join(OUTPUT_BASE, "assemblyReleaseList.txt")
    with open(listPath, "w") as f:
        for db in sorted(deployed):
            f.write(db + "\n")
    log.info("Wrote %s (%d dbs)", listPath, len(deployed))
    log.info(
        "Next: add the %s subtrack to ~/kent/src/hg/makeDb/trackDb/evaSnp.ra "
        "(parent evaSnpContainer on), flip %s to off, add searchTable, "
        "commit, run 'make alpha DBS=\"%s\"' from src/hg/makeDb/trackDb.",
        TRACK_NAME, f"evaSnp{int(VERSION) - 1}", " ".join(sorted(deployed)),
    )


def deployContrib(targets):
    """Recreate Hiram's contrib-injection: staging dir + mkLinks.sh, run it."""
    if not targets:
        log.warning("No contrib targets to deploy.")
        return

    # Make sure each completed contrib build has the per-assembly trackDb.txt
    # and description.html the mkLinks.sh script will try to symlink. If the
    # build step already wrote them (normal case), this is a no-op.
    regenerated = 0
    for t in targets:
        bb = os.path.join(t.workDir, f"{TRACK_NAME}.bb")
        desc = os.path.join(t.workDir, "description.html")
        if os.path.exists(bb) and not os.path.exists(desc):
            writeContribAssemblyFiles(t)  # parses pipeline.log
            regenerated += 1
    if regenerated:
        log.info("  regenerated description.html for %d contrib builds",
                 regenerated)

    deployed = [t.key for t in targets
                if os.path.exists(os.path.join(t.workDir, f"{TRACK_NAME}.bb"))]
    if not deployed:
        log.warning("No contrib bigBeds present — release list NOT written, "
                    "mkLinks.sh skipped.")
        return

    os.makedirs(CONTRIB_STAGING, exist_ok=True)
    contribLink = os.path.join(CONTRIB_STAGING, "contributedTracks")
    if os.path.islink(contribLink) or os.path.exists(contribLink):
        try:
            os.remove(contribLink)
        except OSError:
            pass
    os.symlink(CONTRIB_DIR, contribLink)
    log.info("  staged contributedTracks symlink: %s -> %s",
             contribLink, CONTRIB_DIR)

    trackdbPath = os.path.join(CONTRIB_STAGING, f"{TRACK_NAME}.trackDb.txt")
    with open(trackdbPath, "w") as f:
        f.write(CONTRIB_TRACKDB_TEMPLATE)
    log.info("  wrote %s", trackdbPath)

    listPath = os.path.join(OUTPUT_BASE, "contribReleaseList.txt")
    with open(listPath, "w") as f:
        for acc in sorted(deployed):
            f.write(acc + "\n")
    log.info("Wrote %s (%d accessions)", listPath, len(deployed))

    mkLinks = os.path.join(CONTRIB_STAGING, "mkLinks.sh")
    with open(mkLinks, "w") as f:
        f.write(_renderMkLinksScript())
    os.chmod(mkLinks, os.stat(mkLinks).st_mode | stat.S_IXUSR | stat.S_IXGRP)
    log.info("  wrote %s", mkLinks)

    log.info("Running mkLinks.sh to wire symlinks into GenArk hubs...")
    runCmd(f"cd {CONTRIB_STAGING} && ./mkLinks.sh > do.log 2>&1",
           timeout=3600)
    log.info("  mkLinks done. See %s/do.log", CONTRIB_STAGING)
    log.info(
        "Next: add '%s' to ~/kent/src/hg/makeDb/trackDb/betaGenArk.txt and "
        "publicGenArk.txt, commit, then run the per-clade GenArk makes "
        "(see evaSnp9.txt).",
        TRACK_NAME,
    )


def _renderMkLinksScript():
    """Generate the shell script that injects symlinks under each GenArk hub."""
    return f"""\
#!/bin/bash
# Inject {TRACK_NAME} into each GenArk hub via contrib/{TRACK_NAME}/ symlinks.
# Generated by evaSnp9.py deploy contrib.

set -u

ls -d contributedTracks/GC* 2>/dev/null | sed -e 's#contributedTracks/##;' | while read acc
do
  gcX="${{acc:0:3}}"
  d0="${{acc:4:3}}"
  d1="${{acc:7:3}}"
  d2="${{acc:10:3}}"
  P="${{gcX}}/${{d0}}/${{d1}}/${{d2}}/${{acc}}"
  aB="genbankBuild"
  if [ "${{gcX}}" = "GCF" ]; then
    aB="refseqBuild"
  fi
  buildPath=$(ls -d /hive/data/genomes/asmHubs/$aB/${{P}}* 2>/dev/null)
  if [ -d "${{buildPath}}" ]; then
    mkdir -p "${{buildPath}}/contrib/{TRACK_NAME}"
    for F in {TRACK_NAME}.bb description.html
    do
      rm -f "${{buildPath}}/contrib/{TRACK_NAME}/${{F}}"
      ln -s "$(pwd -P)/contributedTracks/${{acc}}/${{F}}" \\
            "${{buildPath}}/contrib/{TRACK_NAME}/"
    done
    rm -f "${{buildPath}}/contrib/{TRACK_NAME}/{TRACK_NAME}.trackDb.txt"
    ln -s "$(pwd -P)/{TRACK_NAME}.trackDb.txt" \\
          "${{buildPath}}/contrib/{TRACK_NAME}/{TRACK_NAME}.trackDb.txt"
    printf "%s\\n" "${{acc}}"
  else
    printf "ERROR: build dir not found for %s\\n" "${{acc}}" 1>&2
  fi
done
"""


# --- Description / trackDb writers (contrib only — native uses evaSnp.ra) ---

_ORGANISM_CACHE = None


def loadOrganismNames():
    """Lazy-load GCA -> organism name from the NCBI genbank summary."""
    global _ORGANISM_CACHE
    if _ORGANISM_CACHE is not None:
        return _ORGANISM_CACHE
    organisms = {}
    if os.path.exists(GENBANK_SUMMARY):
        with open(GENBANK_SUMMARY) as f:
            for line in f:
                if line.startswith("#"):
                    continue
                fields = line.rstrip("\n").split("\t")
                if len(fields) >= 8:
                    acc, organism = fields[0], fields[7]
                    organisms[acc] = organism
                    organisms[acc.rsplit(".", 1)[0]] = organism
    _ORGANISM_CACHE = organisms
    return organisms


def parsePipelineLog(logPath):
    """Recover the result info dict from a per-assembly pipeline.log."""
    info = {}
    if not os.path.exists(logPath):
        return info
    with open(logPath, errors="replace") as f:
        for line in f:
            m = re.search(r"Processing: \s*\S+\s+(\S+)\s+\(<-(\S+?)\)?\s", line)
            if m:
                info["key"] = m.group(1)
                info["evaGca"] = m.group(2).rstrip(")")
            m = re.search(r"by_assembly/GCA_[^/]+/([^/]+)/", line)
            if m:
                info["species"] = m.group(1).replace("_", " ").capitalize()
            m = re.search(r"final BED: (\d+) total, (\d+) annotated \(([0-9.]+)%\)",
                          line)
            if m:
                info["totalVariants"] = int(m.group(1))
                info["annotatedVariants"] = int(m.group(2))
                info["annotationPct"] = m.group(3)
            m = re.search(r"using gene track: (\S+)", line)
            if m:
                info["geneTrack"] = m.group(1)
            m = re.search(r"itemCount: ([0-9,]+)", line)
            if m:
                info["itemCount"] = m.group(1).replace(",", "")
            m = re.search(r"chromCount: (\d+)", line)
            if m:
                info["chromCount"] = m.group(1)
            m = re.match(r"(\d{4}-\d{2}-\d{2})", line)
            if m and "processDate" not in info:
                info["processDate"] = m.group(1)
    return info


def _renderDescriptionHtml(target, info):
    """Build the per-assembly contrib description.html."""
    acc = target.key
    evaGca = info.get("evaGca", target.evaGca)
    species = info.get("species") or loadOrganismNames().get(target.evaGca, "Unknown")
    totalVariants = int(info.get("totalVariants") or info.get("itemCount") or 0)
    annotationPct = info.get("annotationPct", "?")
    geneTrack = info.get("geneTrack", "none")
    geneTrackDisplay = geneTrack if geneTrack != "none" else "none (no gene models)"
    processDate = info.get("processDate") or datetime.date.today().isoformat()
    chromCount = info.get("chromCount", "?")
    mismatchNote = ""
    if target.versionMismatch:
        mismatchNote = (
            f"\n<p><b>Note:</b> The EVA data was generated for "
            f"<code>{evaGca}</code>, a different version than the GenArk "
            f"assembly <code>{acc}</code>. Chromosome coordinates were mapped "
            f"where possible; some variants on version-specific scaffolds may "
            f"have been excluded.</p>"
        )

    return f"""\
<h2>Build Information</h2>
<table cellpadding='2' cellspacing='0' border='1' style='border-collapse:collapse;'>
<tr><td><b>GenArk Assembly</b></td><td>{acc}</td></tr>
<tr><td><b>EVA Source Assembly</b></td><td>{evaGca}</td></tr>
<tr><td><b>Organism</b></td><td><em>{species}</em></td></tr>
<tr><td><b>Total Variants</b></td><td>{totalVariants:,}</td></tr>
<tr><td><b>Annotation Rate</b></td><td>{annotationPct}%</td></tr>
<tr><td><b>Gene Model</b></td><td>{geneTrackDisplay}</td></tr>
<tr><td><b>Sequences</b></td><td>{chromCount}</td></tr>
<tr><td><b>Processed</b></td><td>{processDate}</td></tr>
<tr><td><b>EVA Release</b></td><td>{VERSION}</td></tr>
</table>
{mismatchNote}

<h2>Description</h2>
<p>
This track contains mappings of single nucleotide variants and small
insertions and deletions (indels) from the European Variation Archive
(<a href="https://www.ebi.ac.uk/eva/" target="_blank">EVA</a>) Release {VERSION}
for the <em>{species}</em> {acc} assembly.
</p>

<h2>Interpreting and Configuring the Graphical Display</h2>
<p>
Variants are shown as single tick marks at most zoom levels. When viewing the
track at or near base-level resolution, the displayed width corresponds to the
width of the variant in the reference sequence. Insertions are indicated by a
single tick mark between two nucleotides; single nucleotide variants are
displayed as one base wide; multiple-nucleotide variants span two or more
bases. Display automatically collapses to dense when more than 100k variants
are in view. When the window exceeds 250k bp the display switches to density
graph mode.
</p>

<h3>Mouse-over</h3>
<p>
Mousing over an item shows the ucscClass (consequence from the
<a target="_blank" href="/cgi-bin/hgVai">Variant Annotation Integrator</a>) and
the aaChange when available, in HGVS.p form. Multiple ucscClasses appear comma
separated; multiple HGVS.p terms appear space separated.
</p>
<p>
The gene model used for functional annotation of this assembly was
<b>{geneTrackDisplay}</b>.
</p>

<h3>Track colors</h3>
<p>
Variants are colored by the most potentially deleterious functional effect:
</p>
<table cellpadding='2'>
<tr><td style="background-color: red">&nbsp;&nbsp;</td><td>Protein-altering variants and splice site variants</td></tr>
<tr><td style="background-color: green">&nbsp;&nbsp;</td><td>Synonymous codon variants</td></tr>
<tr><td style="background-color: blue">&nbsp;&nbsp;</td><td>Non-coding transcript or UTR variants</td></tr>
<tr><td style="background-color: black">&nbsp;&nbsp;</td><td>Intergenic and intronic variants</td></tr>
</table>

<h3>Sequence Ontology (SO) variant classes</h3>
<p>Variants are classified by EVA into the following Sequence Ontology terms:</p>
<ul>
  <li><b>SNV</b> &mdash; single nucleotide variant.</li>
  <li><b>deletion</b> &mdash; one or more nucleotides deleted (Ref = GA, Alt = G).</li>
  <li><b>insertion</b> &mdash; one or more nucleotides inserted (Ref = G, Alt = GT).</li>
  <li><b>indel</b> &mdash; reference and alternate alleles differ in length and content.</li>
  <li><b>MNV</b> &mdash; multiple nucleotide variant (Ref = AA, Alt = GC).</li>
  <li><b>sequence_alteration</b> &mdash; generic class for uncharacterized variants.</li>
</ul>

<h2>Methods</h2>
<p>
Data were downloaded from the European Variation Archive
<a href="https://ftp.ebi.ac.uk/pub/databases/eva/rs_releases/release_{VERSION}/by_assembly/" target="_blank">release {VERSION}</a>
<code>current_ids.vcf.gz</code> file for assembly {evaGca}.
Chromosome names were converted to the assembly's native naming convention
using GenArk chromAlias tables. Variants were passed through the
<a target="_blank" href="/cgi-bin/hgVai">Variant Annotation Integrator</a>
using <b>{geneTrackDisplay}</b> gene models to predict consequence.
</p>

<h2>Credits</h2>
<p>
Produced from the
<a target="_blank" href="https://www.ebi.ac.uk/eva/">European Variation Archive release {VERSION}</a>
data. Consequences were predicted using UCSC's Variant Annotation Integrator
and <b>{geneTrackDisplay}</b> gene models.
</p>

<h2>References</h2>
<p>
Cezard T, Cunningham F, Hunt SE, Koylass B, Kumar N, Saunders G, Shen A,
Silva AF, Tsukanov K, Venkataraman S <em>et al.</em>
<a href="https://doi.org/10.1093/nar/gkab960" target="_blank">The European
Variation Archive: a FAIR resource of genomic variation for all species</a>.
<em>Nucleic Acids Res.</em> 2021. PMID:
<a href="https://pubmed.ncbi.nlm.nih.gov/34718739/" target="_blank">34718739</a>.
</p>
<p>
Hinrichs AS, Raney BJ, Speir ML, Rhead B, Casper J, Karolchik D, Kuhn RM,
Rosenbloom KR, Zweig AS, Haussler D, Kent WJ.
<a href="https://academic.oup.com/bioinformatics/article/32/9/1430/1744314/" target="_blank">UCSC
Data Integrator and Variant Annotation Integrator</a>.
<em>Bioinformatics</em>. 2016;32(9):1430-2. PMID:
<a href="https://www.ncbi.nlm.nih.gov/pubmed/26740527" target="_blank">26740527</a>.
</p>
"""


def writeContribAssemblyFiles(target, info=None):
    """Write per-assembly trackDb.txt + description.html for a contrib build.

    `info` is the result dict from processOne(). When None (e.g. invoked at
    deploy-time on a previously-completed build), the function recovers what
    it can from pipeline.log.
    """
    asmDir = target.workDir
    bbFile = os.path.join(asmDir, f"{TRACK_NAME}.bb")
    if not os.path.exists(bbFile):
        return
    if info is None:
        info = parsePipelineLog(os.path.join(asmDir, "pipeline.log"))
    # Per-assembly trackDb.txt uses the standalone-hub layout (relative paths).
    # The GenArk-injection variant lives separately in CONTRIB_STAGING/.
    with open(os.path.join(asmDir, "trackDb.txt"), "w") as f:
        f.write(STANDALONE_TRACKDB_TEMPLATE)
    with open(os.path.join(asmDir, "description.html"), "w") as f:
        f.write(_renderDescriptionHtml(target, info))


# --- CLI ---

def main():
    parser = argparse.ArgumentParser(
        description=f"Build {TRACK_NAME} tracks (native + GenArk contrib)")
    sub = parser.add_subparsers(dest="cmd", required=True)

    pClassify = sub.add_parser("classify",
                               help="print three-bucket assembly report")
    pClassify.add_argument("-v", "--verbose", action="store_true")

    pBuild = sub.add_parser("build", help="run per-assembly pipeline")
    pBuild.add_argument("target",
                        help="'all' or a UCSC db name or GenArk accession")
    pBuild.add_argument("-j", "--parallel", type=int, default=1)
    pBuild.add_argument("-v", "--verbose", action="store_true")

    pDeploy = sub.add_parser("deploy", help="deploy completed builds")
    pDeploy.add_argument("which", choices=["native", "contrib", "all"])
    pDeploy.add_argument("-v", "--verbose", action="store_true")

    pReport = sub.add_parser("report",
                             help="(re)generate the per-assembly match-rate "
                                  "TSV from current state on disk")
    pReport.add_argument("-v", "--verbose", action="store_true")

    pHub = sub.add_parser("hub",
                          help="write hub.txt + genomes.txt + documentation.html "
                               "for the standalone contributed-tracks hub")
    pHub.add_argument("-v", "--verbose", action="store_true")

    args = parser.parse_args()
    logging.basicConfig(
        level=logging.DEBUG if getattr(args, "verbose", False) else logging.INFO,
        format="%(asctime)s %(levelname)s %(message)s",
        datefmt="%Y-%m-%d %H:%M:%S",
    )

    if args.cmd == "classify":
        nativeT, contribT, skip = buildMatchList()
        print("=" * 80)
        print(f"EVA Release {VERSION} discovery — three-bucket classification")
        print("=" * 80)
        print(f"\nNative ({len(nativeT)} assemblies — /gbdb deployment):")
        for t in nativeT:
            mm = " *MM" if t.versionMismatch else ""
            print(f"  {t.key:<12} <- {t.evaGca}{mm}")
        print(f"\nContrib ({len(contribT)} assemblies — GenArk hub deployment):")
        for t in contribT:
            mm = " *MM" if t.versionMismatch else ""
            print(f"  {t.key:<25} <- {t.evaGca}{mm}")
        print(f"\nSkipped ({len(skip)} EVA assemblies with no matching UCSC/GenArk):")
        for evaGca, reason in skip[:25]:
            print(f"  {evaGca:<25} {reason}")
        if len(skip) > 25:
            print(f"  ... and {len(skip) - 25} more")
        return

    if args.cmd == "build":
        nativeT, contribT, _ = buildMatchList()
        allT = nativeT + contribT
        if args.target == "all":
            runBuild(allT, max(1, args.parallel), args.verbose)
        else:
            match = [t for t in allT if t.key == args.target or t.evaGca == args.target]
            if not match:
                log.error("target %s not found in match list", args.target)
                sys.exit(1)
            runBuild(match, 1, args.verbose)
        return

    if args.cmd == "deploy":
        nativeT, contribT, _ = buildMatchList()
        if args.which in ("native", "all"):
            log.info("Deploying native...")
            deployNative(nativeT)
        if args.which in ("contrib", "all"):
            log.info("Deploying contrib...")
            deployContrib(contribT)
        return

    if args.cmd == "report":
        nativeT, contribT, _ = buildMatchList()
        writeBuildReport(nativeT + contribT)
        return

    if args.cmd == "hub":
        writeStandaloneHub()
        return


if __name__ == "__main__":
    main()

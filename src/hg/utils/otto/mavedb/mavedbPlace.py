#!/usr/bin/env python3
"""Place MaveDB variants on hg38: shared by the MaveDB otto job and the MaveMD build.

A MaveDB variant arrives with the experiment's own HGVS term, numbered along the sequence
the lab tested, and usually with MaveDB's post-mapped terms on a RefSeq protein and/or the
genome. This module decides, for every single variant of a score set, which reference
protein position it sits on, and then walks that position to the three genomic bases of
its codon.

The position comes from one offset per score set (per target for multi-target sets):
reference position = experiment position + offset. Offsets are tested against the
wild-type residues the experiment states ("Thr" in p.Thr3Met):

  1. MaveDB's offset, read off its own mapped rows, if at most one distinct wild-type
     position disagrees with the reference protein.
  2. Otherwise, the one offset on the gene's RefSeq proteins at which at most one position
     disagrees, needing at least 10 positions and a unique answer. When MaveDB's offset was
     rejected, every MaveDB protein placement of that target is discarded.
  3. Otherwise nothing at protein level is placed for that target.

One disagreeing position is tolerated only from 10 positions up; variants at it are
dropped. Every placed variant must then match the reference protein and the genome codon.
Nothing is aligned; there are no identity or score cutoffs.

Python 3.9 compatible (otto's cron python). Standard library only.
"""

import collections
import os
import re
import subprocess

# ---------------------------------------------------------------- constants

THREE_TO_ONE = {
    'Ala': 'A', 'Arg': 'R', 'Asn': 'N', 'Asp': 'D', 'Cys': 'C', 'Gln': 'Q', 'Glu': 'E',
    'Gly': 'G', 'His': 'H', 'Ile': 'I', 'Leu': 'L', 'Lys': 'K', 'Met': 'M', 'Phe': 'F',
    'Pro': 'P', 'Ser': 'S', 'Thr': 'T', 'Trp': 'W', 'Tyr': 'Y', 'Val': 'V', 'Ter': '*',
    'Sec': 'U',
}
ONE_TO_THREE = {v: k for k, v in THREE_TO_ONE.items()}

# D15: a target's offset may disagree with the reference protein at this many distinct
# wild-type positions (a construct carrying one known change, e.g. MPL S505N). The data
# has 738 targets at 0, 24 at 1, none at 2 or 3, so the rule sits in an empty gap.
MAX_MISMATCHED_POSITIONS = 1

# D16: fewer distinct wild-type positions than this and a one-position disagreement is not
# tolerated, and our own offset search does not run at all: a short pattern can fit a
# protein by chance.
MIN_POSITIONS = 10

# Per-accession ceiling for reference-protein-vs-genome disagreement. Single rows that fail
# are dropped anyway; this only catches a transcript whose RefSeq sequence and genome
# translation diverge wholesale, which then fails at close to 100%.
MAX_GENOME_MISMATCH = 0.25

# Above this share of rows whose MaveDB genomic term falls outside the codon we placed the
# variant on, abort: that is a bug in the projection, not a data problem.
MAX_CROSSCHECK_MISMATCH = 0.005

# Placement provenance, shown on the details page and in the variant mouseover.
PLACED_MAVEDB = 'MaveDB'
PLACED_MAVEDB_OFFSET = 'UCSC: MaveDB offset'
PLACED_ASSAY = 'UCSC: assay numbering'
PLACED_CORRECTED = 'UCSC: corrected MaveDB offset'
PLACED_REFERENCE = 'UCSC: reference term'

# Experiment nucleotide terms already written on a reference sequence, which hgvsToVcf can
# place by themselves (saturation genome editing score sets give NM_ terms).
REFERENCE_TERM = re.compile(r'^(?:NM_|NC_|NR_|ENST)\d+\.\d+:[cgn]\.')

NA = ('NA', '', '-', 'None', None)

SAFE_ACCESSION = re.compile(r'^[A-Za-z0-9_.]+$')


def clean(value):
    """Normalise the CSV's spellings of "no value" to an empty string."""
    if value in NA:
        return ''
    return value.strip()


def checkAccession(acc):
    """Fail loudly on an accession that has no business being pasted into SQL."""
    if not SAFE_ACCESSION.match(acc):
        raise ValueError('refusing to query on accession %r' % acc)
    return acc


def hgsql(db, query):
    """Run a query and return rows as lists of strings."""
    out = subprocess.run(['hgsql', db, '-N', '-e', query], check=True,
                         stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                         universal_newlines=True)
    return [line.split('\t') for line in out.stdout.rstrip('\n').split('\n') if line]


def isMainChrom(chrom):
    """True for chr1..chr22, chrX, chrY, chrM - not alts, randoms or patches."""
    return '_' not in chrom


def baseAcc(acc):
    """Accession without its version."""
    return acc.split('.')[0]


# ---------------------------------------------------------------- HGVS terms

# One residue change: substitution, nonsense, synonymous with position, single-residue
# deletion, single-residue delins to one residue. Anything else at protein level is
# rejected with a reason (classifyProteinRest).
PROTEIN_HEAD = re.compile(r'^(?:(?P<pfx>.+):)?p\.\(?(?P<wt>[A-Z][a-z]{2})(?P<pos>\d+)'
                          r'(?P<rest>.*?)\)?$')
RES3 = r'(?:[A-Z][a-z]{2})'


class ProteinTerm(object):
    """A single-residue protein change: wt and alt as one-letter codes.

    alt is '=' for synonymous and '-' for a deletion. pos is in the term's own numbering.
    """

    __slots__ = ('prefix', 'wt', 'pos', 'alt', 'kind')

    def __init__(self, prefix, wt, pos, alt, kind):
        self.prefix = prefix
        self.wt = wt
        self.pos = pos
        self.alt = alt
        self.kind = kind

    def __repr__(self):
        return 'ProteinTerm(%r, %s%d%s)' % (self.prefix, self.wt, self.pos, self.alt)


def parseProteinTerm(term):
    """Parse one protein term; return (ProteinTerm, None) or (None, reason).

    Reasons: 'noTerm', 'multiVariant', 'synonymousNoPosition', 'frameshift',
    'multiResidue', 'extension', 'unknownResidue', 'unparsed'.
    """
    term = clean(term)
    if not term:
        return None, 'noTerm'
    body = term.split(':p.')[-1] if ':p.' in term else term
    if '[' in body or ';' in body:
        return None, 'multiVariant'
    if body.lstrip('p.(').startswith('='):
        return None, 'synonymousNoPosition'
    m = PROTEIN_HEAD.match(term)
    if not m:
        return None, 'unparsed'
    wt3, rest = m.group('wt'), m.group('rest')
    if wt3 not in THREE_TO_ONE:
        return None, 'unknownResidue'
    if rest.startswith('_') or 'ins' in rest and not rest.startswith('delins') or 'dup' in rest:
        return None, 'multiResidue'
    if 'fs' in rest:
        return None, 'frameshift'
    if 'ext' in rest:
        return None, 'extension'
    wt = THREE_TO_ONE[wt3]
    pos = int(m.group('pos'))
    pfx = m.group('pfx')
    if rest == '=':
        return ProteinTerm(pfx, wt, pos, '=', 'syn'), None
    if rest == 'del':
        return ProteinTerm(pfx, wt, pos, '-', 'del'), None
    if rest in ('*', 'X'):
        return ProteinTerm(pfx, wt, pos, '*', 'nonsense'), None
    mm = re.match(r'^(?:delins)?(' + RES3 + r')$', rest)
    if mm:
        alt3 = mm.group(1)
        if alt3 not in THREE_TO_ONE:
            return None, 'unknownResidue'
        alt = THREE_TO_ONE[alt3]
        if alt == wt:
            return ProteinTerm(pfx, wt, pos, '=', 'syn'), None
        return ProteinTerm(pfx, wt, pos, alt, 'nonsense' if alt == '*' else 'sub'), None
    if rest.startswith('delins'):
        return None, 'multiResidue'
    return None, 'unparsed'


def isMultiVariant(row):
    """True for a row that changes more than one residue or site.

    Judged by the protein term when there is one: c.[8C>T;9C>G] is p.Thr3Met, a single
    residue change made by two base changes, and is a single variant. Without a protein
    term, by the nucleotide terms.
    """
    pro = clean(row.get('hgvs_pro'))
    if pro:
        body = pro.split(':p.')[-1]
        return '[' in body or ';' in body
    for col in ('hgvs_nt', 'hgvs_splice'):
        v = clean(row.get(col))
        if '[' in v or ';' in v:
            return True
    return False


GENOMIC_TERM = re.compile(r'^(?P<acc>NC_\d+\.\d+):g\.(?P<start>\d+)(?:_(?P<end>\d+))?'
                          r'(?P<rest>.*)$')
GENOMIC_SNV = re.compile(r'^(?P<ref>[ACGT])>(?P<alt>[ACGT])$')


class GenomicTerm(object):
    """A MaveDB genomic term: chromosome accession, 1-based closed range, change text."""

    __slots__ = ('acc', 'start', 'end', 'rest', 'term')

    def __init__(self, acc, start, end, rest, term):
        self.acc = acc
        self.start = start
        self.end = end
        self.rest = rest
        self.term = term

    def isSubstitution(self):
        """A base substitution, or a delins that replaces bases with as many bases."""
        if GENOMIC_SNV.match(self.rest):
            return True
        m = re.match(r'^delins([ACGT]+)$', self.rest)
        return bool(m) and len(m.group(1)) == self.end - self.start + 1

    def snvRef(self):
        """Reference base of a substitution, or None."""
        m = GENOMIC_SNV.match(self.rest)
        return m.group('ref') if m else None

    def bedRange(self):
        """(start0, end) as BED. An insertion between N and N+1 is zero-length at N."""
        if self.rest.startswith('ins'):
            return self.start, self.start
        return self.start - 1, self.end


def parseGenomicTerm(term):
    """Parse a MaveDB post-mapped g. term; None if absent or not on an NC_ chromosome."""
    term = clean(term)
    m = GENOMIC_TERM.match(term)
    if not m:
        return None
    start = int(m.group('start'))
    end = int(m.group('end')) if m.group('end') else start
    return GenomicTerm(m.group('acc'), start, end, m.group('rest'), term)


# ---------------------------------------------------------------- reference data

class Reference(object):
    """hg38 RefSeq proteins, transcripts, genes and identifier lookups, loaded once.

    Everything the placement rules read comes from here, so tests can substitute a small
    object with the same methods.
    """

    def __init__(self, db='hg38'):
        self.db = db
        self.peptides = {}          # NP_/ENSP accession -> sequence (no trailing stop)
        self.npToNm = {}            # NP_ -> NM_, current table
        self.npToNmHist = {}        # NP_ -> NM_, historical table
        self.npGene = {}            # NP_ -> gene symbol
        self.geneNps = collections.defaultdict(list)   # gene -> NP_s, current first
        self.npVersions = collections.defaultdict(list)  # NP_ base -> versions
        self.maneSelect = set()
        self.manePlus = set()
        self.geneSymbols = set()
        self.uniprotGene = {}
        self.ensemblGene = {}       # ENSG/ENST/ENSP base -> gene symbol
        self.enspToEnst = {}        # ENSP base -> ENST versioned
        self.ncToChrom = {}
        self.chromToNc = {}
        self.gencodeAttrs = None
        self.gencodeGenePred = None

    def load(self):
        """Read all lookups from the database. A few seconds and ~300 MB."""
        db = self.db
        for table in ('ncbiRefSeqPepTable', 'ncbiRefSeqPepTableHistorical'):
            for name, seq in hgsql(db, 'select name, seq from %s' % table):
                self.peptides.setdefault(name, seq.rstrip('*'))
        for gene, nm, np in hgsql(db, 'select name, mrnaAcc, protAcc from ncbiRefSeqLink '
                                      'where protAcc != ""'):
            self.npToNm.setdefault(np, nm)
            self.npGene.setdefault(np, gene)
            self.geneNps[gene].append(np)
            self.geneSymbols.add(gene)
        for gene, nm, np in hgsql(db, 'select name, mrnaAcc, protAcc from '
                                      'ncbiRefSeqLinkHistorical where protAcc != ""'):
            self.npToNmHist.setdefault(np, nm)
            self.npGene.setdefault(np, gene)
            if np not in self.geneNps[gene]:
                self.geneNps[gene].append(np)
        for np in set(self.npToNm) | set(self.npToNmHist):
            self.npVersions[baseAcc(np)].append(np)
        for versions in self.npVersions.values():
            versions.sort(key=lambda a: -int(a.split('.')[1]) if '.' in a else 0)
        self.loadMane()
        self.gencodeAttrs = newestGencode(db, 'wgEncodeGencodeAttrsV')
        self.gencodeGenePred = self.gencodeAttrs.replace('Attrs', 'Comp')
        for geneId, geneName, txId, protId in hgsql(
                db, 'select geneId, geneName, transcriptId, proteinId from %s'
                    % self.gencodeAttrs):
            for ident in (geneId, txId, protId):
                if ident:
                    self.ensemblGene.setdefault(baseAcc(ident), geneName)
            if protId:
                self.enspToEnst.setdefault(baseAcc(protId), txId)
        for alias, chrom in hgsql(db, "select alias, chrom from chromAlias "
                                      "where source = 'refseq'"):
            self.ncToChrom[alias] = chrom
            self.chromToNc[chrom] = alias
        return self

    def loadMane(self):
        """MANE Select and Plus Clinical protein accessions from the MANE bigBed."""
        path = '/gbdb/%s/mane/mane.bb' % self.db
        if not os.path.exists(path):
            return
        out = subprocess.run(['bigBedToBed', path, 'stdout'], check=True,
                             stdout=subprocess.PIPE, universal_newlines=True).stdout
        for line in out.split('\n'):
            f = line.split('\t')
            if len(f) < 25:
                continue
            np, status = f[23], f[24]
            if status == 'MANE Select':
                self.maneSelect.add(np)
            elif status.startswith('MANE Plus'):
                self.manePlus.add(np)

    def peptide(self, acc):
        return self.peptides.get(acc)

    def loadUniprotGenes(self, accs):
        """Primary gene names for these UniProt accessions (uniProt.gene holds every
        species, 250M rows, so only the accessions a catalogue uses are read)."""
        accs = sorted({baseAcc(a.split('-')[0]) for a in accs} - set(self.uniprotGene))
        for i in range(0, len(accs), 500):
            chunk = [checkAccession(a) for a in accs[i:i + 500]]
            for acc, val in hgsql('uniProt', "select acc, val from gene where isPrimary = 1 "
                                  "and acc in (%s)" % ','.join("'%s'" % a for a in chunk)):
                self.uniprotGene.setdefault(acc, val.strip())

    def loadUniprotForCatalogue(self, scoreSets):
        """loadUniprotGenes() for every UniProt identifier on these score sets' targets."""
        accs = []
        for s in scoreSets:
            for t in s.get('targetGenes') or []:
                for ext in t.get('externalIdentifiers') or []:
                    ident = ext.get('identifier') or {}
                    if ident.get('dbName') == 'UniProt' and ident.get('identifier'):
                        accs.append(ident['identifier'])
        self.loadUniprotGenes(accs)

    def transcriptOf(self, prot):
        """(transcript, genePred table) for a protein accession, or (None, None).

        Exact version only: a different version of the same protein can be a different
        length (NP_001015049.1 is 488 aa, .2 is 447), which would shift every codon.
        Historical versions resolve through the Historical tables.
        """
        if prot.startswith('ENSP'):
            tx = self.enspToEnst.get(baseAcc(prot))
            return (tx, self.gencodeGenePred) if tx else (None, None)
        if prot in self.npToNm:
            return self.npToNm[prot], 'ncbiRefSeqCurated'
        if prot in self.npToNmHist:
            return self.npToNmHist[prot], 'ncbiRefSeqHistorical'
        return None, None

    def proteinsOfGene(self, gene):
        """Candidate proteins for the label search, in order: MANE Select, MANE Plus
        Clinical, other current RefSeq, historical RefSeq."""
        # Curated NP_ only: predicted XP_ proteins have no curated transcript to place on.
        nps = [np for np in self.geneNps.get(gene, []) if np.startswith('NP_')]
        def rank(np):
            if np in self.maneSelect:
                return (0, np)
            if np in self.manePlus:
                return (1, np)
            if np in self.npToNm:
                return (2, np)
            return (3, np)
        return sorted(nps, key=rank)

    def otherVersions(self, prot):
        """Other versions of a RefSeq protein, newest first."""
        return [v for v in self.npVersions.get(baseAcc(prot), []) if v != prot]


def newestGencode(db, prefix):
    """Name of the highest-numbered GENCODE table with this prefix (no hardcoded V50)."""
    rows = hgsql(db, "show tables like '%s%%'" % prefix)
    best, bestV = None, -1
    for (name,) in rows:
        m = re.match(re.escape(prefix) + r'(\d+)$', name)
        if m and int(m.group(1)) > bestV:
            best, bestV = name, int(m.group(1))
    if best is None:
        raise RuntimeError('no %s* table in %s' % (prefix, db))
    return best


# ---------------------------------------------------------------- gene of a target

GENE_TOKEN = re.compile(r'[A-Za-z0-9-]+')


def targetGene(target, ref):
    """Gene symbol of a MaveDB target and where it came from, or (None, None).

    Order: MaveDB's mapped HGNC name; the gene on an accession-based target; a UniProt,
    Ensembl or RefSeq identifier on the target; a word of the target name that is a
    current RefSeq gene symbol, if exactly one is. A wrong gene cannot place anything by
    itself: the label search still needs the experiment's residues to fit one offset.
    """
    if target.get('mappedHgncName'):
        return target['mappedHgncName'], 'mappedHgncName'
    ta = target.get('targetAccession') or {}
    if ta.get('gene'):
        return ta['gene'], 'targetAccession'
    for ext in target.get('externalIdentifiers') or []:
        ident = ext.get('identifier') or {}
        dbName, value = ident.get('dbName'), ident.get('identifier') or ''
        gene = None
        if dbName == 'UniProt':
            gene = ref.uniprotGene.get(value.split('-')[0])
        elif dbName == 'Ensembl':
            gene = ref.ensemblGene.get(baseAcc(value))
        elif dbName == 'RefSeq':
            gene = ref.npGene.get(value)
            if gene is None:
                for np, nm in ref.npToNm.items():
                    if baseAcc(nm) == baseAcc(value):
                        gene = ref.npGene.get(np)
                        break
        if gene:
            return gene, dbName
    words = {w for w in GENE_TOKEN.findall(target.get('name') or '') if w in ref.geneSymbols}
    if len(words) == 1:
        return words.pop(), 'targetName'
    return None, None


def targetProtein(target, ref):
    """For an accession-based target, the protein it is numbered on, else None."""
    ta = target.get('targetAccession') or {}
    acc = ta.get('accession') or ''
    if acc.startswith('NP_') or acc.startswith('ENSP'):
        return acc
    if acc.startswith('NM_'):
        for np, nm in ref.npToNm.items():
            if nm == acc:
                return np
        for np, nm in ref.npToNmHist.items():
            if nm == acc:
                return np
    if acc.startswith('ENST'):
        for ensp, enst in ref.enspToEnst.items():
            if baseAcc(enst) == baseAcc(acc):
                return ensp
    return None


# ---------------------------------------------------------------- offsets

def residueAt(seq, pos):
    """Reference residue at 1-based pos; '*' just past the end (the stop codon)."""
    if 1 <= pos <= len(seq):
        return seq[pos - 1]
    if pos == len(seq) + 1:
        return '*'
    return None


def mismatchedPositions(seq, wildType, offset, limit=None):
    """Experiment positions whose stated wild-type residue differs from seq at offset.

    wildType maps experiment position -> set of stated residues (a position with two
    different stated residues always counts as mismatched). Stops early past limit.
    """
    bad = []
    for pos, residues in wildType.items():
        refRes = residueAt(seq, pos + offset)
        if len(residues) != 1 or refRes not in residues:
            bad.append(pos)
            if limit is not None and len(bad) > limit:
                break
    return bad


def fitKind(bad, positions, tol):
    """How an offset fits, given its mismatched positions, or None if it does not.

    'exact': no position differs. 'point': at most tol positions differ (D15, a construct
    carrying one known change). 'tagStart' / 'tagEnd': the differing positions are one
    unbroken run at the start or the end of the construct, residues a tag or linker adds;
    at least MIN_POSITIONS other positions must match exactly, and more of them than the
    run (a tag is a small addition, not half the construct).
    positions: every stated wild-type position of the target, sorted.
    """
    if not bad:
        return 'exact'
    if len(bad) <= tol:
        return 'point'
    matched = len(positions) - len(bad)
    if matched < MIN_POSITIONS or len(bad) >= matched:
        return None
    b = sorted(bad)
    if b == positions[:len(b)]:
        return 'tagStart'
    if b == positions[-len(b):]:
        return 'tagEnd'
    return None


def fittingOffsets(seq, wildType, tol):
    """Every offset on seq that fitKind() accepts, as (offset, mismatched, kind).

    Candidates come from anchors: for a fit with at most tol scattered differences, one
    of the tol+1 rarest-residue positions must match; for a run at one end, the
    position at the other end must match.
    """
    positions = sorted(wildType)
    usable = [(p, next(iter(r))) for p, r in wildType.items() if len(r) == 1]
    if not usable:
        return []
    freq = collections.Counter(seq)
    anchors = sorted(usable, key=lambda pr: (freq.get(pr[1], 0), pr[0]))[:tol + 1]
    anchors += [pr for pr in usable if pr[0] in (positions[0], positions[-1])]
    offsets = set()
    for pos, res in anchors:
        if res == '*':
            offsets.add(len(seq) + 1 - pos)
            continue
        i = seq.find(res)
        while i >= 0:
            offsets.add(i + 1 - pos)
            i = seq.find(res, i + 1)
    hits = []
    for off in sorted(offsets):
        bad = mismatchedPositions(seq, wildType, off)
        kind = fitKind(bad, positions, tol)
        if kind:
            hits.append((off, bad, kind))
    return hits


class OffsetChoice(object):
    """The accepted offset for one target (assay prefix) of a score set."""

    def __init__(self, prot, offset, route, mismatched, mavedbOffset, nPositions,
                 kind='exact'):
        self.prot = prot                    # reference protein accession
        self.offset = offset
        self.route = route                  # 'mavedb' | 'label' | 'accession'
        self.mismatched = set(mismatched)   # experiment positions dropped
        self.mavedbOffset = mavedbOffset    # (prot, offset) MaveDB used, or None
        self.nPositions = nPositions
        self.kind = kind                    # fitKind(): exact | point | tagStart | tagEnd
        self.residues = None                # point: (reference residue, [construct's])

    def isTag(self):
        return self.kind in ('tagStart', 'tagEnd')

    def constructNote(self):
        """What the construct carries that the reference protein does not, or None."""
        if self.kind == 'point':
            note = 'differs from %s at position %d' % (
                self.prot, min(self.mismatched) + self.offset)
            if self.residues and self.residues[0]:
                refAa, constructAas = self.residues
                note += ' (%s in the reference, %s in the construct)' % (
                    ONE_TO_THREE.get(refAa, refAa),
                    '/'.join(ONE_TO_THREE.get(a, a) for a in constructAas))
            return note
        if self.isTag():
            return '%d added residue%s at its %s' % (
                len(self.mismatched), '' if len(self.mismatched) == 1 else 's',
                'start' if self.kind == 'tagStart' else 'end')
        return None

    def __repr__(self):
        return 'OffsetChoice(%s%+d %s %s mm=%s)' % (self.prot, self.offset, self.route,
                                                    self.kind, sorted(self.mismatched))


def chooseOffset(wildType, mavedbOffsets, gene, ref, accessionProt=None):
    """Pick the offset for one target. Returns (OffsetChoice or None, reason).

    wildType: experiment position -> set of stated wild-type residues.
    mavedbOffsets: Counter of (protein, mapped position - experiment position) over every
    row MaveDB mapped, agreeing or not (filtering first changes the dominant offset).
    """
    n = len(wildType)
    tol = MAX_MISMATCHED_POSITIONS if n >= MIN_POSITIONS else 0
    mav = mavedbOffsets.most_common(1)[0][0] if mavedbOffsets else None
    positions = sorted(wildType)

    # Order: MaveDB's offset exact; MaveDB's offset with one differing position (a real
    # construct change, e.g. TP53 Arg72; an old RefSeq version carrying that residue must
    # not win); our search exact; MaveDB's offset with a tag; our search near. A tag fit
    # on MaveDB's isoform yields to an exact fit elsewhere, or an isoform differing at its
    # last residues would pass as a tag.
    given = None
    for prot, off, route in ((accessionProt, 0, 'accession'),
                             (mav[0] if mav else None, mav[1] if mav else 0, 'mavedb')):
        seq = ref.peptide(prot) if prot else None
        if seq is None:
            continue
        bad = mismatchedPositions(seq, wildType, off)
        kind = fitKind(bad, positions, tol)
        if kind in ('exact', 'point'):
            return OffsetChoice(prot, off, route, bad, mav, n, kind), 'ok'
        if kind and given is None:
            given = OffsetChoice(prot, off, route, bad, mav, n, kind)

    found, why = searchOffset(wildType, mav, gene, ref, n)
    if found is not None and found.kind == 'exact':
        return found, 'ok'
    if given is not None:
        return given, 'ok'
    return found, why


def searchOffset(wildType, mav, gene, ref, n):
    """Our own offset search over candidate proteins; (OffsetChoice or None, reason)."""
    if n < MIN_POSITIONS:
        return None, 'tooFewPositions'

    cands = []
    if mav:
        cands.append(mav[0])
        cands.extend(ref.otherVersions(mav[0]))
    if gene:
        cands.extend(ref.proteinsOfGene(gene))
    if not cands:
        return None, 'noGene'
    # The first candidate with one exact fit wins; failing that, the first with one near
    # fit. More than one fit on any candidate protein (a repeat) is ambiguous.
    firstNear = None
    seen = set()
    for prot in cands:
        if prot in seen:
            continue
        seen.add(prot)
        seq = ref.peptide(prot)
        if not seq:
            continue
        hits = fittingOffsets(seq, wildType, MAX_MISMATCHED_POSITIONS)
        exact = [h for h in hits if h[2] == 'exact']
        near = [h for h in hits if h[2] != 'exact']
        if len(exact) > 1 or (not exact and len(near) > 1):
            return None, 'ambiguousOffset'
        if exact:
            off, bad, kind = exact[0]
            return OffsetChoice(prot, off, 'label', bad, mav, n, kind), 'ok'
        if near and firstNear is None:
            off, bad, kind = near[0]
            firstNear = OffsetChoice(prot, off, 'label', bad, mav, n, kind)
    if firstNear is not None:
        return firstNear, 'ok'
    return None, 'noFit'


# ---------------------------------------------------------------- one score set

class Placement(object):
    """Outcome for one row of a score set.

    status 'placed' or 'rejected'. A placed row has either a protein placement (prot,
    protPos, wt, alt) or only a genomic one (genomic), or both. reason says why a row was
    rejected; label is the provenance of a placed row.
    """

    __slots__ = ('row', 'status', 'reason', 'prefix', 'prot', 'protPos', 'wt', 'alt',
                 'kind', 'genomic', 'label', 'mavedbProt', 'mavedbPos', 'codon', 'chrom',
                 'strand', 'construct')

    def __init__(self, row):
        self.row = row
        self.status = 'rejected'
        self.reason = None
        self.prefix = None
        self.prot = None
        self.protPos = None
        self.wt = None
        self.alt = None
        self.kind = None
        self.genomic = None
        self.label = None
        self.mavedbProt = None      # MaveDB placed it on another protein (isoform move)
        self.mavedbPos = None
        self.codon = None           # genomic bases of the codon, from checkGenome()
        self.chrom = None
        self.strand = None
        self.construct = None

    def reject(self, reason):
        self.status = 'rejected'
        self.reason = reason
        return self


def targetForPrefix(scoreSet, prefix):
    """The target a prefixed term refers to; the only target when there is one."""
    targets = scoreSet.get('targetGenes') or []
    if len(targets) == 1:
        return targets[0]
    for t in targets:
        if t.get('name') == prefix:
            return t
        ta = t.get('targetAccession') or {}
        if prefix and ta.get('accession') == prefix:
            return t
    return None


def placeScoreSet(scoreSet, rows, ref):
    """Place every row of one score set at protein level (no genome lookups yet).

    Returns (placements, choices, reasons) where choices maps assay prefix to
    (OffsetChoice or None, reason) and placements is one Placement per row, in row order.
    checkGenome() then validates against the genome and fills in codons.
    """
    parsed = []
    wildType = collections.defaultdict(lambda: collections.defaultdict(set))
    mavedbOffsets = collections.defaultdict(collections.Counter)
    for row in rows:
        pl = Placement(row)
        if isMultiVariant(row):
            parsed.append((pl.reject('multiVariant'), None, None, None))
            continue
        assay, why = parseProteinTerm(row.get('hgvs_pro'))
        mapped, _ = parseProteinTerm(row.get('mavedb.post_mapped_hgvs_p'))
        if mapped is not None and not mapped.prefix:
            mapped = None
        genomic = parseGenomicTerm(row.get('mavedb.post_mapped_hgvs_g'))
        if assay is not None:
            wildType[assay.prefix][assay.pos].add(assay.wt)
            if mapped is not None:
                mavedbOffsets[assay.prefix][(mapped.prefix, mapped.pos - assay.pos)] += 1
        parsed.append((pl, assay, why, (mapped, genomic)))

    choices = {}
    for prefix, wt in wildType.items():
        target = targetForPrefix(scoreSet, prefix) or {}
        gene, _ = targetGene(target, ref)
        accProt = targetProtein(target, ref)
        if prefix and (prefix.startswith('NP_') or prefix.startswith('ENSP')):
            accProt = prefix
        choice, why = chooseOffset(dict(wt), mavedbOffsets[prefix], gene, ref, accProt)
        if choice is not None and choice.kind == 'point':
            seq = ref.peptide(choice.prot)
            pos = min(choice.mismatched)
            choice.residues = (residueAt(seq, pos + choice.offset), sorted(wt[pos]))
        choices[prefix] = (choice, why)

    out = []
    for pl, assay, why, terms in parsed:
        if terms is None:
            out.append(pl)
            continue
        mapped, genomic = terms
        if assay is not None:
            placeProteinRow(pl, assay, mapped, genomic, choices[assay.prefix], ref)
        elif why not in ('noTerm', 'synonymousNoPosition'):
            pl.reject(why)
        elif mapped is not None:
            # No usable experiment protein term: MaveDB's mapped term is used as given,
            # but only for a substitution. For an insertion, deletion or duplication the
            # protein term is the consequence, not the site: an in-frame deletion in a
            # repeat is shifted to the last equivalent residue (c.3456_3458del ->
            # p.Pro1153del), a frameshift names the codon where the stop appears. Those
            # rows are placed by their genomic term only.
            if genomic is not None and not genomic.isSubstitution():
                pl.status = 'placed'
                pl.genomic = genomic
                pl.label = PLACED_MAVEDB
                out.append(pl)
                continue
            seq = ref.peptide(mapped.prefix)
            if seq is not None and residueAt(seq, mapped.pos) != mapped.wt:
                pl.reject('residueVsRefSeq')
            else:
                setProtein(pl, mapped.prefix, mapped.pos, mapped, PLACED_MAVEDB)
                pl.genomic = genomic
        elif genomic is not None:
            pl.status = 'placed'
            pl.genomic = genomic
            pl.label = PLACED_MAVEDB
        elif REFERENCE_TERM.match(clean(pl.row.get('hgvs_nt'))):
            # MaveDB did not map it, but the experiment's own term is on a reference
            # sequence: placeReferenceTerms() places it and checks the stated base.
            pl.status = 'pending'
            pl.reason = 'referenceTerm'
        else:
            pl.reject('notMapped')
        out.append(pl)
    reasons = collections.Counter(p.reason for p in out if p.status == 'rejected')
    return out, choices, reasons


def placeReferenceTerms(placements, ref, workDir):
    """Place 'pending' rows from their experiment's reference nucleotide term.

    One hgvsToVcf run for all of them. Placed rows are genomic-only, labelled
    PLACED_REFERENCE; a term whose stated reference base disagrees with the genome, or that
    hgvsToVcf cannot place, is rejected. Returns a Counter of outcomes.
    """
    pending = [p for p in placements if p.status == 'pending']
    stats = collections.Counter()
    if not pending:
        return stats
    terms = {clean(p.row.get('hgvs_nt')) for p in pending}
    vcfRows = runHgvsToVcf(ref.db, terms, workDir)
    for p in pending:
        hit = vcfRows.get(clean(p.row.get('hgvs_nt')))
        if hit is None:
            p.reject('referenceTermUnplaced')
        elif hit[0] != 'PASS':
            p.reject('referenceTermMismatch')
        else:
            _, chrom, pos, refAllele, altAllele = hit
            nc = ref.chromToNc.get(chrom)
            if nc is None or not isMainChrom(chrom):
                p.reject('genomicNotMainChrom')
                continue
            p.status = 'placed'
            p.reason = None
            p.genomic = vcfToGenomic(nc, pos, refAllele, altAllele)
            p.label = PLACED_REFERENCE
        stats[p.reason or 'placed'] += 1
    return stats


def vcfToGenomic(nc, pos, refAllele, altAllele):
    """A GenomicTerm (1-based, closed) from a VCF record, anchor base removed."""
    if len(refAllele) == 1 and len(altAllele) == 1:
        return GenomicTerm(nc, pos, pos, '%s>%s' % (refAllele, altAllele),
                           '%s:g.%d%s>%s' % (nc, pos, refAllele, altAllele))
    if refAllele[0] == altAllele[0]:
        refAllele, altAllele, pos = refAllele[1:], altAllele[1:], pos + 1
    if not refAllele:                       # insertion between pos-1 and pos
        rest = 'ins' + altAllele
        return GenomicTerm(nc, pos - 1, pos, rest, '%s:g.%d_%d%s' % (nc, pos - 1, pos, rest))
    end = pos + len(refAllele) - 1
    rest = 'del' if not altAllele else 'delins' + altAllele
    span = '%d' % pos if end == pos else '%d_%d' % (pos, end)
    return GenomicTerm(nc, pos, end, rest, '%s:g.%s%s' % (nc, span, rest))


def runHgvsToVcf(db, terms, workDir):
    """Run kent's hgvsToVcf on terms; {term: (filter, chrom, pos, ref, alt)}."""
    inPath = os.path.join(workDir, 'refTerms.hgvs')
    outPath = os.path.join(workDir, 'refTerms.vcf')
    with open(inPath, 'w') as fh:
        for term in sorted(terms):
            fh.write(term + '\n')
    subprocess.run(['hgvsToVcf', db, inPath, outPath], check=True,
                   stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    out = {}
    with open(outPath) as fh:
        for line in fh:
            if line.startswith('#'):
                continue
            f = line.rstrip('\n').split('\t')
            if f[4] == '.':
                continue
            out[f[2]] = (f[6], f[0], int(f[1]), f[3], f[4])
    return out


def setProtein(pl, prot, pos, term, label):
    pl.status = 'placed'
    pl.reason = None
    pl.prot = prot
    pl.protPos = pos
    pl.wt = term.wt
    pl.alt = term.alt
    pl.kind = term.kind
    pl.label = label


def placeProteinRow(pl, assay, mapped, genomic, choiceAndReason, ref):
    """Place a row that has a single-residue experiment protein term."""
    pl.prefix = assay.prefix
    choice, why = choiceAndReason
    if choice is None:
        return pl.reject('noOffset:' + why)
    pos = assay.pos + choice.offset
    if assay.pos in choice.mismatched:
        return pl.reject('constructTag' if choice.isTag() else 'constructDiffers')
    seq = ref.peptide(choice.prot)
    if seq is None or residueAt(seq, pos) != assay.wt:
        return pl.reject('residueVsRefSeq')
    pl.construct = choice.constructNote()
    if choice.isTag() and choice.route != 'label':
        # Accepted only because of the end-run rule: a UCSC placement, even where the
        # offset is MaveDB's.
        setProtein(pl, choice.prot, pos, assay, PLACED_MAVEDB_OFFSET)
        return pl
    if choice.route == 'label':
        # D17: MaveDB's offset for this target was wrong or missing; none of its protein
        # placements are used, and its genomic terms came from the same mapping.
        label = PLACED_CORRECTED if choice.mavedbOffset else PLACED_ASSAY
        setProtein(pl, choice.prot, pos, assay, label)
        return pl
    if mapped is not None and mapped.prefix == choice.prot and mapped.pos == pos \
            and mapped.wt == assay.wt:
        label = PLACED_MAVEDB
    elif mapped is not None and mapped.prefix != choice.prot and mapped.wt == assay.wt:
        label = PLACED_MAVEDB       # isoform move; checkGenome() compares the codons
        pl.mavedbProt = mapped.prefix
        pl.mavedbPos = mapped.pos
    else:
        label = PLACED_MAVEDB_OFFSET
    setProtein(pl, choice.prot, pos, assay, label)
    if label == PLACED_MAVEDB:
        pl.genomic = genomic
    return pl


# ---------------------------------------------------------------- genome

class CodonMap(object):
    """Genomic coordinates of every codon of one transcript.

    codon(n) is the list of three 0-based genomic positions of codon n (1-based), in
    transcription order. A codon split by an intron is not a contiguous run.
    """

    def __init__(self, tx, chrom, strand, cdsBases):
        self.tx = tx
        self.chrom = chrom
        self.strand = strand
        self.cdsBases = cdsBases
        self.protLen = len(cdsBases) // 3
        self.protein = None      # genome translation, from addProteinSequence()
        self.cds = None          # genome CDS bases in transcript order, likewise

    def codon(self, protPos):
        i = (protPos - 1) * 3
        if protPos < 1 or i + 3 > len(self.cdsBases):
            return None
        return self.cdsBases[i:i + 3]

    def codonSpan(self, protPos):
        bases = self.codon(protPos)
        if bases is None:
            return None
        return min(bases), max(bases) + 1

    def codonBlock(self, protPos):
        """Longest contiguous run of a codon's bases, as (start0, size), or None."""
        bases = self.codon(protPos)
        if bases is None:
            return None
        ordered = sorted(bases)
        runs = []
        runStart, runLen = ordered[0], 1
        for prev, cur in zip(ordered, ordered[1:]):
            if cur == prev + 1:
                runLen += 1
            else:
                runs.append((runStart, runLen))
                runStart, runLen = cur, 1
        runs.append((runStart, runLen))
        return max(runs, key=lambda r: r[1])


def codonRuns(codon):
    """Contiguous runs of a codon's genomic bases, as [(start0, end)] in genome order."""
    bases = sorted(codon)
    runs = [[bases[0], bases[0] + 1]]
    for b in bases[1:]:
        if b == runs[-1][1]:
            runs[-1][1] = b + 1
        else:
            runs.append([b, b + 1])
    return [tuple(r) for r in runs]


def proteinTermText(pl):
    """p. term on the reference protein, e.g. p.Gly290Ala, p.Gly290=, p.Gly290del."""
    wt3 = ONE_TO_THREE[pl.wt]
    if pl.alt == '=':
        return 'p.%s%d=' % (wt3, pl.protPos)
    if pl.alt == '-':
        return 'p.%s%ddel' % (wt3, pl.protPos)
    return 'p.%s%d%s' % (wt3, pl.protPos, ONE_TO_THREE[pl.alt])


def loadCodonMaps(db, transcripts, table='ncbiRefSeqCurated'):
    """CodonMap for each transcript in one genePred table.

    A transcript can align to more than one place; a main-chromosome alignment wins, then
    the longest CDS, then the first chromosome by name (chrX over chrY for the
    pseudoautosomal genes, whatever the table's row order). Returns (maps, missing).
    """
    maps = {}
    wanted = set(transcripts)
    for tx in wanted:
        checkAccession(tx)
    if not wanted:
        return maps, []
    names = ','.join("'%s'" % tx for tx in sorted(wanted))
    rows = hgsql(db, 'select name, chrom, strand, cdsStart, cdsEnd, exonStarts, exonEnds '
                     'from %s where name in (%s)' % (table, names))
    for tx, chrom, strand, cdsStart, cdsEnd, exonStarts, exonEnds in rows:
        cdsStart, cdsEnd = int(cdsStart), int(cdsEnd)
        starts = [int(x) for x in exonStarts.rstrip(',').split(',')]
        ends = [int(x) for x in exonEnds.rstrip(',').split(',')]
        bases = []
        for s, e in zip(starts, ends):
            s, e = max(s, cdsStart), min(e, cdsEnd)
            if s < e:
                bases.extend(range(s, e))
        if not bases:
            continue
        if strand == '-':
            bases.reverse()
        cand = CodonMap(tx, chrom, strand, bases)
        best = maps.get(tx)
        if best is None \
                or (isMainChrom(cand.chrom) and not isMainChrom(best.chrom)) \
                or (isMainChrom(cand.chrom) == isMainChrom(best.chrom)
                    and (len(cand.cdsBases), best.chrom) > (len(best.cdsBases), cand.chrom)):
            maps[tx] = cand
    return maps, sorted(wanted - set(maps))


CODON_TABLE = {}
for _bases, _aa in (
        ('TTT TTC', 'F'), ('TTA TTG CTT CTC CTA CTG', 'L'), ('ATT ATC ATA', 'I'),
        ('ATG', 'M'), ('GTT GTC GTA GTG', 'V'), ('TCT TCC TCA TCG AGT AGC', 'S'),
        ('CCT CCC CCA CCG', 'P'), ('ACT ACC ACA ACG', 'T'), ('GCT GCC GCA GCG', 'A'),
        ('TAT TAC', 'Y'), ('TAA TAG TGA', '*'), ('CAT CAC', 'H'), ('CAA CAG', 'Q'),
        ('AAT AAC', 'N'), ('AAA AAG', 'K'), ('GAT GAC', 'D'), ('GAA GAG', 'E'),
        ('TGT TGC', 'C'), ('TGG', 'W'), ('CGT CGC CGA CGG AGA AGG', 'R'),
        ('GGT GGC GGA GGG', 'G')):
    for _b in _bases.split():
        CODON_TABLE[_b] = _aa

COMPLEMENT = str.maketrans('ACGTacgtNn', 'TGCAtgcaNn')


def fetchBases(twoBit, runs, workDir, tag='bases'):
    """Genome sequence of (chrom, start0, end) runs, as {(chrom, pos0): base}."""
    bedPath = os.path.join(workDir, tag + '.bed')
    faPath = os.path.join(workDir, tag + '.fa')
    runs = sorted(set(runs))
    with open(bedPath, 'w') as fh:
        for chrom, start, end in runs:
            fh.write('%s\t%d\t%d\t%s:%d-%d\n' % (chrom, start, end, chrom, start, end))
    subprocess.run(['twoBitToFa', '-bed=' + bedPath, '-bedPos', twoBit, faPath],
                   check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    seq = {}
    name, chunks = None, []
    with open(faPath) as fh:
        for line in fh:
            if line.startswith('>'):
                if name:
                    seq[name] = ''.join(chunks)
                name, chunks = line[1:].strip(), []
            else:
                chunks.append(line.strip())
    if name:
        seq[name] = ''.join(chunks)
    base = {}
    for chrom, start, end in runs:
        s = seq.get('%s:%d-%d' % (chrom, start, end))
        if s is None:
            raise RuntimeError('twoBitToFa returned no sequence for %s:%d-%d'
                               % (chrom, start, end))
        for i, ch in enumerate(s):
            base[(chrom, start + i)] = ch.upper()
    return base


def addProteinSequence(maps, twoBit, workDir):
    """Translate each transcript's CDS from the genome and hang it on its CodonMap."""
    runs = []
    for cm in maps.values():
        ordered = sorted(cm.cdsBases)
        start = prev = ordered[0]
        for pos in ordered[1:]:
            if pos != prev + 1:
                runs.append((cm.chrom, start, prev + 1))
                start = pos
            prev = pos
        runs.append((cm.chrom, start, prev + 1))
    base = fetchBases(twoBit, runs, workDir, 'cdsExons')
    for cm in maps.values():
        letters = []
        for pos in cm.cdsBases:
            ch = base.get((cm.chrom, pos), 'N')
            letters.append(ch.translate(COMPLEMENT) if cm.strand == '-' else ch)
        cds = ''.join(letters)
        cm.cds = cds             # genome bases of the CDS in transcript order
        cm.protein = ''.join(CODON_TABLE.get(cds[i:i + 3], 'X')
                             for i in range(0, len(cds) - 2, 3))


def loadMapsForProteins(ref, prots, twoBit, workDir):
    """CodonMaps with genome translation for proteins; returns (protToMap, unresolved)."""
    byTable = collections.defaultdict(set)
    txOf = {}
    unresolved = []
    for prot in prots:
        tx, table = ref.transcriptOf(prot)
        if tx is None:
            unresolved.append(prot)
            continue
        byTable[table].add(tx)
        txOf[prot] = (tx, table)
    maps = {}
    for table, txs in byTable.items():
        got, _ = loadCodonMaps(ref.db, txs, table)
        for tx, cm in got.items():
            maps[(tx, table)] = cm
    if maps:
        addProteinSequence(maps, twoBit, workDir)
    protToMap = {}
    for prot, key in txOf.items():
        if key in maps and isMainChrom(maps[key].chrom):
            protToMap[prot] = maps[key]
        else:
            unresolved.append(prot)
    return protToMap, unresolved


def checkGenome(placements, ref, twoBit, workDir, protToMap=None):
    """Validate protein placements against the genome; fill in codons. Returns stats.

    Per row: the codon must exist, and the genome must encode the stated wild-type
    residue there. An accession failing on more than MAX_GENOME_MISMATCH of its rows is
    dropped whole. A MaveDB genomic term on a placed row must fall inside the codon; the
    term is dropped otherwise and counted, and crossCheckGate() judges the run's total. Genomic-only rows must be on a
    main chromosome and, for substitutions, match the reference base.

    protToMap: CodonMaps from loadMapsForProteins(), when the caller loads them once for
    many score sets; otherwise they are loaded here.
    """
    stats = collections.Counter()
    if protToMap is None:
        prots = {p.prot for p in placements if p.status == 'placed' and p.prot}
        prots |= {p.mavedbProt for p in placements if p.mavedbProt}
        protToMap, unresolved = loadMapsForProteins(ref, prots, twoBit, workDir)
        stats['unresolvedProteins'] = len(unresolved)

    perAcc = collections.defaultdict(lambda: [0, 0])
    for p in placements:
        if p.status != 'placed' or not p.prot:
            continue
        cm = protToMap.get(p.prot)
        if cm is None:
            p.reject('noTranscript')
            continue
        codon = cm.codon(p.protPos)
        if codon is None:
            p.reject('outsideCds')
            continue
        perAcc[p.prot][0] += 1
        if cm.protein[p.protPos - 1:p.protPos] != p.wt:
            perAcc[p.prot][1] += 1
            p.reject('residueVsGenome')
            continue
        p.codon, p.chrom, p.strand = codon, cm.chrom, cm.strand
        if p.mavedbProt:
            other = protToMap.get(p.mavedbProt)
            otherCodon = other.codon(p.mavedbPos) if other else None
            if other is not None and other.chrom == cm.chrom and otherCodon is not None \
                    and sorted(otherCodon) == sorted(codon):
                stats['isoformSameCodon'] += 1
            else:
                stats['isoformTrueMove'] += 1
                p.label = PLACED_MAVEDB_OFFSET
                p.genomic = None
    dropped = {acc for acc, (n, bad) in perAcc.items() if n and bad / n > MAX_GENOME_MISMATCH}
    for p in placements:
        if p.status == 'placed' and p.prot in dropped:
            p.reject('accessionDropped')
    stats['accessionsDropped'] = len(dropped)

    snvRuns = []
    checked = bad = 0
    for p in placements:
        if p.status != 'placed' or p.genomic is None:
            continue
        g = p.genomic
        chrom = ref.ncToChrom.get(g.acc)
        if chrom is None or not isMainChrom(chrom):
            if p.prot:
                p.genomic = None
            else:
                p.reject('genomicNotMainChrom')
            continue
        if p.codon is not None:
            checked += 1
            span = set(range(g.start - 1, max(g.end, g.start)))
            if chrom != p.chrom or not span & set(p.codon):
                bad += 1
                p.genomic = None
                stats['crossCheckMismatch'] += 1
        elif g.snvRef():
            snvRuns.append((chrom, g.start - 1, g.start))
    stats['crossChecked'] = checked
    if snvRuns:
        base = fetchBases(twoBit, snvRuns, workDir, 'snvRef')
        for p in placements:
            if p.status != 'placed' or p.prot or p.genomic is None:
                continue
            ref1 = p.genomic.snvRef()
            if ref1 is None:
                continue
            chrom = ref.ncToChrom[p.genomic.acc]
            if base.get((chrom, p.genomic.start - 1)) != ref1:
                p.reject('genomicRefBase')
    return stats


def crossCheckGate(stats):
    """Raise if MaveDB genomic terms fall outside our codon too often over a whole run.

    stats is the sum of checkGenome() results for every score set of the run. A single
    score set can legitimately carry a few such rows; a systematic projection bug cannot
    hide in the total.
    """
    checked, bad = stats['crossChecked'], stats['crossCheckMismatch']
    if checked and bad / checked > MAX_CROSSCHECK_MISMATCH:
        raise RuntimeError('MaveDB genomic terms disagree with our codon for %d of %d rows '
                           '(%.2f%%), above %.1f%%: the projection is wrong somewhere'
                           % (bad, checked, 100.0 * bad / checked,
                              100 * MAX_CROSSCHECK_MISMATCH))


# ---------------------------------------------------------------- text helpers

DASHES = {0x2010: '-', 0x2011: '-', 0x2012: '-', 0x2013: '-', 0x2014: '-', 0x2015: '-',
          0x2212: '-', 0x00ad: '-'}

# Characters with no NFKD decomposition, which would otherwise vanish.
TRANSLITERATE = {
    0x00f8: 'o', 0x00d8: 'O', 0x00e6: 'ae', 0x00c6: 'AE', 0x00e5: 'a', 0x00c5: 'A',
    0x00df: 'ss', 0x0111: 'd', 0x0110: 'D', 0x0142: 'l', 0x0141: 'L',
    0x03b1: 'alpha', 0x03b2: 'beta', 0x03b3: 'gamma', 0x03b4: 'delta',
    0x03ba: 'kappa', 0x03bb: 'lambda', 0x03bc: 'u', 0x03c3: 'sigma', 0x03c9: 'omega',
    0x2018: "'", 0x2019: "'", 0x201c: '"', 0x201d: '"', 0x2026: '...', 0x00b7: '-',
}


def asciiText(value):
    """Flatten text to ASCII, for fields hgTracks draws as raster rather than HTML."""
    import unicodedata
    text = '' if value is None else str(value)
    out = []
    for ch in text:
        code = ord(ch)
        if code < 128:
            out.append(ch)
        elif code in DASHES:
            out.append('-')
        elif code in TRANSLITERATE:
            out.append(TRANSLITERATE[code])
        else:
            folded = unicodedata.normalize('NFKD', ch).encode('ascii', 'ignore').decode()
            out.append(folded if folded else '?')
    return ''.join(out)


def bedField(value):
    """One BED field: no tabs, no newlines, non-ASCII as numeric HTML entities."""
    text = '' if value is None else str(value)
    for ch in ('\t', '\n', '\r'):
        text = text.replace(ch, ' ')
    while '  ' in text:
        text = text.replace('  ', ' ')
    return ''.join(c if ord(c) < 128 else '&#%d;' % ord(c) for c in text.strip())


def fmtScore(value, places=4):
    """Format a functional score for display, trimming trailing zeros."""
    try:
        text = ('%.*f' % (places, float(value))).rstrip('0').rstrip('.')
    except (TypeError, ValueError):
        return ''
    return '0' if text in ('', '-0') else text

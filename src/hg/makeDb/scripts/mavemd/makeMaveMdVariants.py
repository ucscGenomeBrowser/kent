#!/usr/bin/env python3
"""Build the MaveMD per-variant bigBed from a fetchMaveMd.py download.

One item per variant per score set.  Placement takes three routes, in order of preference:

  1. MaveDB's own genomic HGVS (post_mapped_hgvs_g), about a third of the collection
  2. the codon of the protein term (post_mapped_hgvs_p), about two thirds
  3. hgvsToVcf on the submitter's original transcript term, for variants MaveDB's mapper
     did not resolve at all

Routes 1 and 3 both go through kent's hgvsToVcf rather than a hand-rolled HGVS parser, in
one batch invocation.  Route 2 is validated against route 1 on the ~154k variants that
carry both terms; a disagreement rate above a threshold stops the build, because it means
either RefSeq or MaveDB's mapper has moved under us.

Haplotype measurements (p.[Arg19Leu;Gly20Ser]) have no single position and are counted and
excluded, as they were from the earlier MaveDB track.

Usage: makeMaveMdVariants.py <downloadDir> <outBed> [--db hg38] [--raFragment out.ra]
"""

import argparse
import collections
import csv
import glob
import json
import os
import re
import subprocess
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import mavemdLib as lib

csv.field_size_limit(10 ** 7)

# MaveDB states protein terms against RefSeq for most score sets and Ensembl for a few.
PROTEIN_TERM = re.compile(r'^(?P<acc>(?:N[PM]_|ENSP)[\w.]+):p\.'
                          r'(?P<wt>[A-Z][a-z]{2})(?P<pos>\d+)'
                          r'(?P<var>[A-Z][a-z]{2}|Ter|=)$')
NA = ('NA', '', '-', None)

# Above this fraction of disagreements between our codon projection and MaveDB's own
# genomic mapping, stop rather than publish coordinates we no longer trust.
MAX_PROJECTION_MISMATCH = 0.005


def clean(value):
    """Normalise the CSV's several spellings of "no value" to an empty string."""
    return '' if value in NA else value.strip()


def basesToBlocks(bases, chromStart):
    """Contiguous runs of a codon's genomic bases, as BED block starts and sizes."""
    ordered = sorted(bases)
    starts, sizes = [ordered[0]], [1]
    for prev, cur in zip(ordered, ordered[1:]):
        if cur == prev + 1:
            sizes[-1] += 1
        else:
            starts.append(cur)
            sizes.append(1)
    return [s - chromStart for s in starts], sizes


def strengthScore(outcome, strength):
    """BED score 0-1000 from the ACMG evidence strength.

    The ladder is MaveDB's StrengthOfEvidenceProvided enum. Score carries strength only,
    not direction; pathogenic against benign is what the item color says.
    """
    if not outcome or outcome.endswith('_not_met'):
        return 0
    return {'VERY_STRONG': 1000, 'STRONG': 800, 'MODERATE_PLUS': 600,
            'MODERATE': 400, 'SUPPORTING': 200}.get((strength or '').upper(), 0)


def loadScoreSets(downloadDir):
    """Read the score set records, keyed by URN.

    Returns urn -> {title, gene, publication, calibrations}, where calibrations maps a
    calibration URN to its metadata (title, primary flag, research-use-only flag, and the
    OddsPath of each functional class).
    """
    scoreSets = {}
    for path in glob.glob(os.path.join(downloadDir, 'scoreSets', '*.json')):
        record = json.load(open(path))
        targets = record.get('targetGenes') or [{}]
        gene = targets[0].get('mappedHgncName') or targets[0].get('name') or ''
        publications = record.get('primaryPublicationIdentifiers') or []
        calibrations = {}
        primaryTitle = None
        for cal in record.get('scoreCalibrations') or []:
            if cal.get('primary'):
                primaryTitle = cal.get('title', '')
            oddsByClass = {}
            for fc in cal.get('functionalClassifications') or []:
                if fc.get('oddspathsRatio') is not None:
                    oddsByClass[fc.get('label', '')] = fc['oddspathsRatio']
                    oddsByClass[fc.get('functionalClassification', '')] = fc['oddspathsRatio']
            calibrations[cal['urn']] = {
                'title': cal.get('title', ''),
                'primary': bool(cal.get('primary')),
                'ruo': bool(cal.get('researchUseOnly')),
                'odds': oddsByClass,
            }
        # Assay metadata, from the experiment's controlled keywords. McEwen et al. argue
        # that a clinician's first question about a MAVE is what the assay actually
        # measured and in what system, and that the library route matters most of all: an
        # in vitro construct library cannot see a variant's effect on splicing or on
        # nonsense-mediated decay, so it can read falsely normal for one.
        keywords = {}
        for entry in (record.get('experiment') or {}).get('keywords') or []:
            kw = entry.get('keyword') or {}
            if kw.get('key') and kw.get('label'):
                keywords[kw['key']] = kw['label']

        # Not every primary publication is in PubMed; several score sets cite a bioRxiv or
        # medRxiv preprint, whose "identifier" is a DOI. The record carries a resolved url
        # either way, so link that rather than assuming a PMID.
        scoreSets[record['urn']] = {
            'assayMethod': keywords.get('Phenotypic Assay Method', ''),
            'assayModel': keywords.get('Phenotypic Assay Model System', ''),
            'assayMechanism': keywords.get('Molecular Mechanism Assessed', ''),
            'libraryMethod': keywords.get('Variant Library Creation Method', ''),
            'title': record.get('title', ''),
            'gene': gene,
            'publication': publications[0].get('referenceHtml', '') if publications else '',
            'publicationUrl': publications[0].get('url', '') if publications else '',
            'calibrations': calibrations,
            'primaryTitle': primaryTitle,
        }
    return scoreSets


def calibrationColumns(fieldnames):
    """Group the per-calibration CSV columns by calibration URN.

    Column names look like calibration.<urn>.acmg_criterion, so the URN is everything
    between the first and last dot-separated parts.
    """
    byUrn = collections.defaultdict(dict)
    for name in fieldnames:
        if not name.startswith('calibration.'):
            continue
        rest = name[len('calibration.'):]
        urn, _, field = rest.rpartition('.')
        byUrn[urn][field] = name
    return byUrn


def loadScoreSetTranscripts(downloadDir):
    """The transcript each score set's variants were submitted against, per score set.

    Some score sets state their variants as bare terms with no accession, for example
    c.34_36delinsCTT.  MaveDB's mapper leaves those unmapped, but the score set's other
    variants carry a post-mapped cDNA term naming the transcript, so the bare terms can be
    qualified from their own neighbours.  The most frequent accession wins; a score set
    whose rows disagree is not qualified at all rather than guessed at.
    """
    transcripts = {}
    for path in sorted(glob.glob(os.path.join(downloadDir, 'variants', '*.csv'))):
        counts = collections.Counter()
        urn = ''
        with open(path, newline='') as fh:
            for row in csv.DictReader(fh):
                urn = urn or clean(row.get('score_set.score_set_urn'))
                cdna = clean(row.get('mavedb.post_mapped_hgvs_c'))
                if cdna and ':' in cdna:
                    counts[cdna.split(':')[0]] += 1
        if urn and counts:
            top, topCount = counts.most_common(1)[0]
            if topCount >= 0.9 * sum(counts.values()):
                transcripts[urn] = top
    return transcripts


def qualifyTerm(term, transcript):
    """Attach a transcript accession to a bare HGVS term, or return it unchanged."""
    if term and transcript and ':' not in term and term[:2] in ('c.', 'n.', 'g.'):
        return '%s:%s' % (transcript, term)
    return term


def collectTerms(downloadDir, scoreSetTx):
    """First pass: every HGVS term that needs kent's parser, and the protein accessions.

    Returns (terms, protAccs).  terms holds MaveDB's genomic terms plus, for variants
    MaveDB failed to map at all, the submitter's original transcript term, qualified with
    the score set's transcript when it arrived bare.
    """
    terms = set()
    protAccs = set()
    for path in sorted(glob.glob(os.path.join(downloadDir, 'variants', '*.csv'))):
        with open(path, newline='') as fh:
            for row in csv.DictReader(fh):
                genomic = clean(row.get('mavedb.post_mapped_hgvs_g'))
                protein = clean(row.get('mavedb.post_mapped_hgvs_p'))
                if genomic:
                    terms.add(genomic)
                if protein:
                    match = PROTEIN_TERM.match(protein)
                    if match:
                        protAccs.add(match.group('acc'))
                if not genomic and not protein:
                    urn = clean(row.get('score_set.score_set_urn'))
                    original = qualifyTerm(clean(row.get('hgvs_nt')), scoreSetTx.get(urn))
                    if original and ':' in original:
                        terms.add(original)
    return terms, protAccs


def runHgvsToVcf(db, terms, workDir):
    """Convert HGVS terms to coordinates with kent's hgvsToVcf, in one batch.

    Returns (placements, failures) where placements maps the original term to
    (chrom, start0, end) with any VCF anchor base trimmed off.
    """
    if not terms:
        return {}, collections.Counter()
    inPath = os.path.join(workDir, 'terms.hgvs')
    outPath = os.path.join(workDir, 'terms.vcf')
    with open(inPath, 'w') as fh:
        for term in sorted(terms):
            fh.write(term + '\n')
    result = subprocess.run(['hgvsToVcf', db, inPath, outPath],
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                            universal_newlines=True)
    if result.returncode != 0:
        raise RuntimeError('hgvsToVcf failed: %s' % result.stderr)
    unparsed = len([l for l in result.stderr.split('\n') if 'Unable to parse' in l])

    placements = {}
    failures = collections.Counter()
    failures['unparsed'] = unparsed
    with open(outPath) as fh:
        for line in fh:
            if line.startswith('#'):
                continue
            f = line.rstrip('\n').split('\t')
            chrom, pos, term, ref, alt, _, flt = f[0], int(f[1]), f[2], f[3], f[4], f[5], f[6]
            if flt != 'PASS':
                failures[flt] += 1
                continue
            if alt == '.':
                failures['noAlt'] += 1
                continue
            start = pos - 1
            end = start + len(ref)
            # VCF anchors an indel on the preceding base; drop it so the item covers only
            # the bases that actually change.
            if len(ref) > 1 and len(alt) >= 1 and ref[0] == alt[0]:
                start += 1
            if end <= start:
                end = start + 1
            placements[term] = (chrom, start, end)
    return placements, failures


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('downloadDir')
    parser.add_argument('outBed')
    parser.add_argument('--db', default='hg38')
    parser.add_argument('--classPalette', default='purple',
                        choices=sorted(lib.CLASS_PALETTES),
                        help='palette for measurements with no ACMG code')
    parser.add_argument('--raFragment', help='write generated trackDb filterValues here')
    parser.add_argument('--workDir', help='scratch directory (default: alongside outBed)')
    args = parser.parse_args()
    lib.setClassPalette(args.classPalette)

    workDir = args.workDir or os.path.dirname(os.path.abspath(args.outBed)) or '.'
    os.makedirs(workDir, exist_ok=True)

    scoreSets = loadScoreSets(args.downloadDir)
    sys.stderr.write("Loaded %d score set records\n" % len(scoreSets))

    scoreSetTx = loadScoreSetTranscripts(args.downloadDir)
    sys.stderr.write("Resolved a submission transcript for %d of %d score sets\n"
                     % (len(scoreSetTx), len(scoreSets)))

    sys.stderr.write("Pass 1: collecting HGVS terms\n")
    terms, protAccs = collectTerms(args.downloadDir, scoreSetTx)
    sys.stderr.write("  %d distinct HGVS terms, %d protein accessions\n"
                     % (len(terms), len(protAccs)))

    sys.stderr.write("Resolving protein accessions to transcripts\n")
    protToTx, unresolvedProt = lib.loadProteinToTranscript(args.db, sorted(protAccs))
    if unresolvedProt:
        sys.stderr.write("  WARNING: no transcript for %s\n" % ', '.join(unresolvedProt))
    codonMaps, missingTx = lib.loadCodonMaps(args.db, sorted(set(protToTx.values())))
    if missingTx:
        sys.stderr.write("  WARNING: no ncbiRefSeqCurated entry for %s\n" % ', '.join(missingTx))
    for acc, tx in sorted(protToTx.items()):
        cm = codonMaps.get(tx)
        if cm:
            sys.stderr.write("  %-16s -> %-16s %s %s  %d codons\n"
                             % (acc, tx, cm.chrom, cm.strand, cm.protLen))

    sys.stderr.write("Running hgvsToVcf on %d terms\n" % len(terms))
    placements, hgvsFailures = runHgvsToVcf(args.db, terms, workDir)
    sys.stderr.write("  placed %d, failures %s\n" % (len(placements), dict(hgvsFailures)))

    stats = collections.Counter()
    filterValues = collections.defaultdict(set)
    projectionChecked = 0
    projectionMismatch = 0
    mismatchExamples = []
    rows = []

    sys.stderr.write("Pass 2: building items\n")
    for path in sorted(glob.glob(os.path.join(args.downloadDir, 'variants', '*.csv'))):
        with open(path, newline='') as fh:
            reader = csv.DictReader(fh)
            calCols = calibrationColumns(reader.fieldnames)
            clinvarRelease = ''
            for name in reader.fieldnames:
                if name.startswith('clinvar.') and name.endswith('.clinical_significance'):
                    clinvarRelease = name.split('.')[1]
            for row in reader:
                stats['rows'] += 1
                urn = clean(row.get('score_set.score_set_urn'))
                meta = scoreSets.get(urn, {})
                gene = meta.get('gene') or clean(row.get('score_set.target_gene'))

                genomic = clean(row.get('mavedb.post_mapped_hgvs_g'))
                protein = clean(row.get('mavedb.post_mapped_hgvs_p'))
                original = qualifyTerm(clean(row.get('hgvs_nt')), scoreSetTx.get(urn))
                protMatch = PROTEIN_TERM.match(protein) if protein else None

                # Codon projection, computed whenever there is a usable protein term so it
                # can be cross-checked even when a genomic term is also present. A codon
                # at an exon junction is split, so the item spans the intervening intron:
                # that is the real extent of the codon, and a protein-level measurement
                # pins down nothing narrower.
                projected = None
                codonBases = None
                strand = '.'
                if protMatch:
                    tx = protToTx.get(protMatch.group('acc'))
                    codonMap = codonMaps.get(tx) if tx else None
                    if codonMap:
                        codonBases = codonMap.codon(int(protMatch.group('pos')))
                        strand = codonMap.strand
                        if codonBases:
                            start, end = codonMap.codonSpan(int(protMatch.group('pos')))
                            projected = (codonMap.chrom, start, end)

                place = None
                method = ''
                if genomic and genomic in placements:
                    place = placements[genomic]
                    method = 'MaveDB genomic mapping'
                    if projected and codonBases:
                        # Compare against the codon's actual three bases, not its span:
                        # a split codon's span includes an intron that the genomic
                        # coordinate should never fall in.
                        projectionChecked += 1
                        overlap = (projected[0] == place[0] and
                                   any(place[1] <= b < place[2] for b in codonBases))
                        if not overlap:
                            projectionMismatch += 1
                            if len(mismatchExamples) < 10:
                                mismatchExamples.append(
                                    '%s %s g=%s:%d-%d codon bases=%s'
                                    % (urn, protein, place[0], place[1], place[2],
                                       ','.join(str(b + 1) for b in sorted(codonBases))))
                elif projected:
                    place = projected
                    method = 'codon projection'
                elif original and original in placements:
                    place = placements[original]
                    method = 'hgvsToVcf on submitted term'

                if place is None:
                    submittedProtein = clean(row.get('hgvs_pro'))
                    if submittedProtein.startswith('p.[') or ';' in submittedProtein:
                        # A haplotype is several substitutions measured as one unit, so it
                        # has no single position. The earlier MaveDB track excluded these
                        # for the same reason.
                        stats['skipHaplotype'] += 1
                    elif protein and not protMatch:
                        stats['skipProteinTermNotASubstitution'] += 1
                    elif genomic:
                        stats['skipGenomicTermRejected'] += 1
                    elif original:
                        stats['skipSubmittedTermRejected'] += 1
                    else:
                        stats['skipNoUsableTerm'] += 1
                    continue

                chrom, start, end = place
                # Draw the real bases as blocks. For a codon split across an exon junction
                # that is two blocks with a thin connector, rather than one solid bar across
                # the intron: the intron is not part of the codon, and a solid bar there
                # shows a measurement where none was made and pollutes any range query.
                if method == 'codon projection' and codonBases and end - start > 3:
                    blockStarts, blockSizes = basesToBlocks(codonBases, start)
                    stats['splitCodonItem'] += 1
                else:
                    blockStarts, blockSizes = [0], [end - start]

                # Gather every calibration's call for this variant.
                calls = []
                for calUrn, cols in calCols.items():
                    outcome = clean(row.get(cols.get('acmg_evidence_outcome_code', ''), ''))
                    funcClass = clean(row.get(cols.get('functional_classification', ''), ''))
                    if not outcome and not funcClass:
                        continue
                    calMeta = meta.get('calibrations', {}).get(calUrn, {})
                    title = calMeta.get('title') or clean(row.get(cols.get('title', ''), ''))
                    ruoText = clean(row.get(cols.get('research_use_only', ''), ''))
                    calls.append({
                        'urn': calUrn,
                        'title': title,
                        'primary': calMeta.get('primary', False),
                        'ruo': calMeta.get('ruo', ruoText.lower() == 'true'),
                        'funcClass': funcClass,
                        'acmgOutcome': outcome,
                        'acmgCriterion': clean(row.get(cols.get('acmg_criterion', ''), '')),
                        'acmgStrength': clean(row.get(cols.get('acmg_evidence_strength', ''), '')),
                        'odds': calMeta.get('odds', {}).get(funcClass, ''),
                    })

                chosen, source = lib.pickDisplayCall(calls, meta.get('primaryTitle'))
                outcome = chosen['acmgOutcome'] if chosen else ''
                funcClass = chosen['funcClass'] if chosen else ''
                color = lib.cellColor(outcome, funcClass)

                if protMatch:
                    wt = lib.THREE_TO_ONE.get(protMatch.group('wt'), protMatch.group('wt'))
                    varAa = protMatch.group('var')
                    varAa = '=' if varAa == '=' else lib.THREE_TO_ONE.get(varAa, varAa)
                    shortChange = '%s%s%s' % (wt, protMatch.group('pos'), varAa)
                else:
                    shortChange = (genomic or original).split(':')[-1]
                name = '%s:%s' % (gene, shortChange) if gene else shortChange

                allCals = '; '.join(
                    '%s%s: %s' % (c['title'],
                                  ' (research use only)' if c['ruo'] else '',
                                  c['acmgOutcome'] or c['funcClass'] or 'no call')
                    for c in calls) or 'none'

                variantUrn = clean(row.get('accession'))
                score = strengthScore(outcome, chosen['acmgStrength'] if chosen else '')
                clinvarSig = clean(row.get('clinvar.%s.clinical_significance' % clinvarRelease))
                clinvarReview = clean(row.get('clinvar.%s.clinical_review_status' % clinvarRelease))
                vepConsequence = clean(row.get('vep.vep_functional_consequence'))
                gnomadVersion = clean(row.get('gnomad.gnomad_version'))
                assayMethod = meta.get('assayMethod', '')
                assayModel = meta.get('assayModel', '')
                libraryMethod = meta.get('libraryMethod', '')

                item = [
                    chrom, start, end, name, score, strand, start, end,
                    lib.hexToBedRgb(color),
                    len(blockStarts),
                    ','.join(str(b) for b in blockSizes) + ',',
                    ','.join(str(b) for b in blockStarts) + ',',
                    gene,
                    protein,
                    outcome,
                    chosen['acmgStrength'] if chosen else '',
                    funcClass,
                    lib.fmtScore(clean(row.get('scores.score'))),
                    lib.fmtScore(chosen['odds'], 3) if chosen and chosen['odds'] != '' else '',
                    chosen['title'] if chosen else '',
                    source if chosen else '',
                    'yes' if (chosen and chosen['ruo']) else ('no' if chosen else ''),
                    allCals,
                    clinvarSig,
                    clinvarReview,
                    clinvarRelease.replace('_', '-') if clinvarSig else '',
                    clean(row.get('gnomad.gnomad_af')),
                    clean(row.get('gnomad.gnomad_ac')),
                    clean(row.get('gnomad.gnomad_an')),
                    clean(row.get('gnomad.gnomad_faf95_max')),
                    clean(row.get('gnomad.gnomad_faf95_max_ancestry')),
                    gnomadVersion,
                    vepConsequence,
                    urn,
                    meta.get('title', ''),
                    assayMethod,
                    assayModel,
                    meta.get('assayMechanism', ''),
                    libraryMethod,
                    meta.get('publication', ''),
                    meta.get('publicationUrl', ''),
                    genomic,
                    clean(row.get('mavedb.post_mapped_hgvs_c')),
                    clean(row.get('clingen.clingen_allele_id')),
                    variantUrn,
                    method,
                ]
                rows.append(item)
                stats['placed'] += 1
                stats['method:' + method] += 1
                if outcome:
                    filterValues['acmgOutcome'].add(outcome)
                if funcClass:
                    filterValues['funcClass'].add(funcClass)
                if gene:
                    filterValues['gene'].add(gene)
                # Harvest from the named values, never from positions in `item`: the row
                # layout has changed several times and positional indices drift silently
                # into the wrong column.
                for field, value in (('clinvarSig', clinvarSig),
                                     ('vepConsequence', vepConsequence),
                                     ('assayMethod', assayMethod),
                                     ('assayModel', assayModel),
                                     ('libraryMethod', libraryMethod)):
                    if value:
                        filterValues[field].add(value)

    if projectionChecked:
        rate = projectionMismatch / projectionChecked
        sys.stderr.write("Codon projection cross-check: %d of %d disagreed with MaveDB's "
                         "genomic mapping (%.4f%%)\n"
                         % (projectionMismatch, projectionChecked, 100 * rate))
        for example in mismatchExamples:
            sys.stderr.write("    %s\n" % example)
        if rate > MAX_PROJECTION_MISMATCH:
            sys.exit("ERROR: codon projection disagrees with MaveDB on %.2f%% of the "
                     "variants that carry both coordinate systems, above the %.2f%% "
                     "threshold. RefSeq or the MaveDB mapper has moved; investigate before "
                     "publishing." % (100 * rate, 100 * MAX_PROJECTION_MISMATCH))

    rows.sort(key=lambda r: (r[0], r[1], r[2]))
    with open(args.outBed, 'w') as out:
        for item in rows:
            out.write('\t'.join(lib.bedField(f) for f in item) + '\n')

    sys.stderr.write("\nBuild summary\n")
    for key in sorted(stats):
        sys.stderr.write("  %-32s %d\n" % (key, stats[key]))

    if args.raFragment:
        with open(args.raFragment, 'w') as fh:
            for field, label in [
                    ('acmgOutcome', 'ACMG functional evidence'),
                    ('funcClass', 'Measured functional class'),
                    ('gene', 'Gene'),
                    ('clinvarSig', 'ClinVar significance'),
                    ('vepConsequence', 'Variant consequence'),
                    ('assayMethod', 'Assay method'),
                    ('assayModel', 'Assay model system'),
                    ('libraryMethod', 'Variant library method')]:
                values = sorted(filterValues[field])
                if not values:
                    continue
                # filterValues is read with slNameListFromCommaEscaped, which takes a doubled
                # comma as a literal one, so ClinVar terms like "Pathogenic, low penetrance"
                # are escaped to build the menu correctly.
                #
                # No filterType is emitted on purpose. The list types (multipleListOr and
                # friends) run COMPARE_HASH_LIST_OR in hgTracks/bigBedTrack.c, which splits
                # the *field value* on commas before looking it up, so a ClinVar term with a
                # comma in it could never match its own menu entry. Every one of these fields
                # holds a single value, so the default FILTERBY_MULTIPLE is both correct and
                # still multi-select.
                escaped = [v.replace(',', ',,') for v in values]
                fh.write('filterValues.%s %s\n' % (field, ','.join(escaped)))
                fh.write('filterLabel.%s %s\n\n' % (field, label))
        sys.stderr.write("Wrote trackDb filter fragment to %s\n" % args.raFragment)


if __name__ == '__main__':
    main()

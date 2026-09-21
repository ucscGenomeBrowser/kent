#!/usr/bin/env python3
"""Build the MaveMD heatmap bigBed: one variant effect map per score set.

Layout follows the MaveDB and popEVE heatmap tracks: columns are amino acid positions at
their codon's genomic coordinates, rows are the 20 standard amino acids ordered by class,
with a final row for nonsense.  A synonymous measurement fills the wildtype row, which is
empty in the prediction-score heatmaps but is real measured data here.

Cells are colored by clinical call rather than by raw score.  Functional scores are on
each score set's own arbitrary scale, so a single gradient across score sets would put
unrelated numbers on one ramp; the ACMG evidence code and the functional class are the
things that mean the same thing everywhere.  The renderer accepts a literal #rrggbb in a
cell of the score array, which is how the discrete colors get in.

Usage: makeMaveMdHeatmap.py <downloadDir> <outBed> [--db hg38]
"""

import argparse
import collections
import csv
import glob
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import mavemdLib as lib
from makeMaveMdVariants import (PROTEIN_TERM, calibrationColumns, clean, loadScoreSets)

csv.field_size_limit(10 ** 7)

# The fallback spectrum. Every cell carries an explicit color, so this is never used for
# drawing, but the renderer requires at least two ascending thresholds.
FALLBACK_BOUNDS = '0,1'
FALLBACK_COLORS = '#f7f7f7,#b2182b'

def assayLine(meta):
    """One line naming what a score set measured, for the legend and the cell mouseovers.

    Two maps of the same gene routinely disagree because they measured different things:
    PTEN abundance against PTEN lipid phosphatase activity, GCK activity against GCK
    abundance, KCNE1 trafficking with and without KCNQ1. A reader cannot make sense of that
    without knowing which assay they are looking at, so the assay travels with the map
    rather than sitting a click away on the details page.

    Method and model system come first because they are short and always present; the score
    set title can be long and is the part that gets truncated.

    The separator is a plain hyphen, not a middot: the legend is drawn as raster text by
    hgTracks, so an HTML entity from bedField() would appear literally as "&#183;".
    """
    method = meta.get('assayMethod') or ''
    model = meta.get('assayModel') or ''
    title = meta.get('title') or ''
    head = '%s in %s' % (method, model) if method and model else (method or model)
    return ' - '.join(p for p in (head, title) if p)


def severityRank(cell):
    """Rank a cell's call so the strongest evidence wins a tie. Lower is stronger."""
    code = cell.get('outcome')
    if code in lib.ACMG_SEVERITY:
        return lib.ACMG_SEVERITY.index(code)
    order = {'abnormal': 0, 'normal': 1, 'indeterminate': 2}
    return len(lib.ACMG_SEVERITY) + order.get(cell.get('funcClass'), 3)


def direction(cell):
    """'path', 'benign' or '' for a cell's call, ignoring strength."""
    code = cell.get('outcome') or ''
    if code and not code.endswith('_not_met'):
        return 'path' if code.startswith('PS3') else 'benign'
    return {'abnormal': 'path', 'normal': 'benign'}.get(cell.get('funcClass'), '')


def conflictsInDirection(a, b):
    """True when two measurements of the same substitution point opposite ways."""
    da, db = direction(a), direction(b)
    return bool(da) and bool(db) and da != db


def recolorToCalibration(byPos, calUrn, stats):
    """Re-color every cell from one calibration, so a map is internally comparable.

    A cell keeps the calls of all its calibrations; this picks the named one. A cell that
    calibration says nothing about falls back to whatever it already had, which is the
    honest thing to draw and is counted.
    """
    for entry in byPos.values():
        for cell in entry['cells'].values():
            call = (cell.get('calls') or {}).get(calUrn)
            if call is None:
                stats['cellOutsideChosenCalibration'] += 1
                continue
            cell['outcome'] = call['acmgOutcome']
            cell['funcClass'] = call['funcClass']
            cell['color'] = lib.cellColor(call['acmgOutcome'], call['funcClass'])


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('downloadDir')
    parser.add_argument('outBed')
    parser.add_argument('--db', default='hg38')
    parser.add_argument('--classPalette', default='purple',
                        choices=sorted(lib.CLASS_PALETTES),
                        help='palette for measurements with no ACMG code')
    args = parser.parse_args()
    lib.setClassPalette(args.classPalette)

    scoreSets = loadScoreSets(args.downloadDir)

    # Resolve every protein accession once.
    protAccs = set()
    for path in sorted(glob.glob(os.path.join(args.downloadDir, 'variants', '*.csv'))):
        with open(path, newline='') as fh:
            for row in csv.DictReader(fh):
                match = PROTEIN_TERM.match(clean(row.get('mavedb.post_mapped_hgvs_p')) or '')
                if match:
                    protAccs.add(match.group('acc'))
    protToTx, unresolved = lib.loadProteinToTranscript(args.db, sorted(protAccs))
    codonMaps, missing = lib.loadCodonMaps(args.db, sorted(set(protToTx.values())))
    if unresolved:
        sys.stderr.write("  WARNING: no transcript for %s\n" % ', '.join(unresolved))
    if missing:
        sys.stderr.write("  WARNING: no genePred for %s\n" % ', '.join(missing))

    stats = collections.Counter()
    entries = []

    for path in sorted(glob.glob(os.path.join(args.downloadDir, 'variants', '*.csv'))):
        with open(path, newline='') as fh:
            reader = csv.DictReader(fh)
            calCols = calibrationColumns(reader.fieldnames)
            clinvarRelease = ''
            for name in reader.fieldnames:
                if name.startswith('clinvar.') and name.endswith('.clinical_significance'):
                    clinvarRelease = name.split('.')[1]

            # byPos[protPos] = {'wt': X, 'bases': [...], 'cells': {aa: cellDict}}
            byPos = {}
            urn = ''
            codonMap = None
            # A map has to be colored by one calibration throughout, or its cells are not
            # comparable to each other. Where a score set has no MaveDB primary, different
            # rows can fall to different calibrations, so the one that wins for the most
            # variants is named and the map is rebuilt against it below.
            calibrationVotes = collections.Counter()
            for row in reader:
                stats['rows'] += 1
                urn = urn or clean(row.get('score_set.score_set_urn'))
                protein = clean(row.get('mavedb.post_mapped_hgvs_p'))
                match = PROTEIN_TERM.match(protein) if protein else None
                if not match:
                    stats['skipNoProteinTerm'] += 1
                    continue
                tx = protToTx.get(match.group('acc'))
                thisMap = codonMaps.get(tx) if tx else None
                if thisMap is None:
                    stats['skipNoCodonMap'] += 1
                    continue
                if codonMap is None:
                    codonMap = thisMap
                elif thisMap.tx != codonMap.tx:
                    stats['skipOtherTranscript'] += 1
                    continue
                protPos = int(match.group('pos'))
                block = codonMap.codonBlock(protPos)
                if block is None:
                    stats['skipPositionPastCds'] += 1
                    continue
                # The column is drawn on the longest contiguous run of the codon's bases, so
                # a codon split across an exon junction keeps its block on coding sequence
                # instead of starting inside the intron.
                blockStart, blockLen = block
                bases = range(blockStart, blockStart + blockLen)

                wt = lib.THREE_TO_ONE.get(match.group('wt'), match.group('wt'))
                rawVar = match.group('var')
                aa = wt if rawVar == '=' else lib.THREE_TO_ONE.get(rawVar, rawVar)
                if aa not in lib.HEATMAP_ROWS:
                    stats['skipNonStandardResidue'] += 1
                    continue

                meta = scoreSets.get(urn, {})
                calls = []
                for calUrn, cols in calCols.items():
                    outcome = clean(row.get(cols.get('acmg_evidence_outcome_code', ''), ''))
                    funcClass = clean(row.get(cols.get('functional_classification', ''), ''))
                    if not outcome and not funcClass:
                        continue
                    calMeta = meta.get('calibrations', {}).get(calUrn, {})
                    calls.append({
                        'urn': calUrn,
                        'title': calMeta.get('title', ''),
                        'primary': calMeta.get('primary', False),
                        'ruo': calMeta.get('ruo', False),
                        'funcClass': funcClass,
                        'acmgOutcome': outcome,
                        'acmgCriterion': clean(row.get(cols.get('acmg_criterion', ''), '')),
                        'acmgStrength': clean(row.get(cols.get('acmg_evidence_strength', ''), '')),
                        'odds': calMeta.get('odds', {}).get(funcClass, ''),
                    })
                chosen, source = lib.pickDisplayCall(calls, meta.get('primaryTitle'))
                if chosen:
                    calibrationVotes[(chosen['urn'], chosen['title'], chosen['ruo'],
                                      source)] += 1

                outcome = chosen['acmgOutcome'] if chosen else ''
                funcClass = chosen['funcClass'] if chosen else ''
                entry = byPos.setdefault(protPos, {'wt': wt, 'bases': set(), 'cells': {}})
                entry['bases'].update(bases)
                cell = {
                    'color': lib.cellColor(outcome, funcClass),
                    'score': lib.fmtScore(clean(row.get('scores.score'))),
                    'outcome': outcome,
                    'funcClass': funcClass,
                    'clinvar': clean(row.get('clinvar.%s.clinical_significance' % clinvarRelease)),
                    'synonymous': rawVar == '=',
                    'calls': {c['urn']: c for c in calls},
                }
                existing = entry['cells'].get(aa)
                if existing is None:
                    entry['cells'][aa] = cell
                else:
                    # Several nucleotide changes encode the same amino acid substitution and
                    # were measured separately. Taking whichever came first in the CSV is not
                    # a defined rule and flips 611 cells between pathogenic and benign
                    # depending on row order, so the strongest evidence wins instead. Cells
                    # whose measurements disagree in direction are marked separately from
                    # cells that merely have more than one.
                    stats['cellMultipleMeasurements'] += 1
                    if conflictsInDirection(existing, cell):
                        stats['cellConflictingDirection'] += 1
                        existing['conflict'] = True
                        cell['conflict'] = True
                    cell['multi'] = True
                    existing['multi'] = True
                    if severityRank(cell) < severityRank(existing):
                        cell['conflict'] = existing.get('conflict', False)
                        entry['cells'][aa] = cell

            if not byPos or codonMap is None:
                stats['scoreSetSkipped'] += 1
                sys.stderr.write("  no protein-level map for %s (%s)\n"
                                 % (urn, os.path.basename(path)))
                continue

            meta = scoreSets.get(urn, {})
            gene = meta.get('gene') or ''
            if calibrationVotes:
                (winUrn, chosenTitle, chosenRuo, chosenSource), votes = \
                    calibrationVotes.most_common(1)[0]
                if len(calibrationVotes) > 1:
                    stats['scoreSetMixedCalibration'] += 1
                    sys.stderr.write("  %s: %d calibrations competed, using %s (%d of %d)\n"
                                     % (urn, len(calibrationVotes), chosenTitle, votes,
                                        sum(calibrationVotes.values())))
                recolorToCalibration(byPos, winUrn, stats)
            else:
                winUrn = None
                chosenTitle = chosenSource = ''
                chosenRuo = False
            entries.append(buildEntry(urn, gene, meta, byPos, codonMap,
                                      chosenTitle, chosenSource, chosenRuo, stats))
            stats['scoreSetsWritten'] += 1

    entries = [e for e in entries if e]
    entries.sort(key=lambda f: (f[0], int(f[1])))
    with open(args.outBed, 'w') as out:
        for fields in entries:
            out.write('\t'.join(lib.bedField(f) for f in fields) + '\n')

    sys.stderr.write("\nHeatmap summary\n")
    for key in sorted(stats):
        sys.stderr.write("  %-32s %d\n" % (key, stats[key]))


def buildEntry(urn, gene, meta, byPos, codonMap, calTitle, calSource, calRuo, stats):
    """Assemble one heatmap BED12+ line for a score set."""
    cols = sorted((min(entry['bases']), protPos) for protPos, entry in byPos.items())
    colStarts = [c[0] for c in cols]
    colPositions = [c[1] for c in cols]
    nCols = len(cols)
    chromStart = colStarts[0]

    # A codon split across an intron does not occupy three contiguous bases, and adjacent
    # codons can end up closer than three bases apart in the block layout. Clamp so blocks
    # cannot overlap, which bedToBigBed rejects outright.
    blockSizes = []
    for i in range(nCols):
        span = len(byPos[colPositions[i]]['bases'])
        if i < nCols - 1:
            blockSizes.append(max(1, min(span, colStarts[i + 1] - colStarts[i])))
        else:
            blockSizes.append(span)
    relStarts = [s - chromStart for s in colStarts]
    chromEnd = colStarts[-1] + blockSizes[-1]

    for i in range(1, nCols):
        if relStarts[i] < relStarts[i - 1] + blockSizes[i - 1]:
            sys.stderr.write("  ERROR: overlapping blocks in %s at column %d\n" % (urn, i))
            stats['overlap'] += 1
            return None

    assay = assayLine(meta)
    scoreParts = []
    labelParts = []
    measured = 0
    pathogenic = 0
    for aa in lib.HEATMAP_ROWS:
        for protPos in colPositions:
            entry = byPos[protPos]
            cell = entry['cells'].get(aa)
            if cell is None:
                scoreParts.append('')
                labelParts.append('')
                continue
            measured += 1
            if cell['outcome'].startswith('PS3') and not cell['outcome'].endswith('_not_met'):
                pathogenic += 1
            value = cell['color']
            if cell.get('conflict'):
                value += '|!'
            elif cell.get('multi'):
                value += '|+'
            scoreParts.append(value)
            change = ('%s%d= (synonymous)' % (entry['wt'], protPos) if cell['synonymous']
                      else '%s%d%s' % (entry['wt'], protPos, aa))
            # Same wording as the mouseOver on the MaveMD Variants track, so a reader
            # flipping between the two sees the same labels.
            bits = ['<b>%s</b>' % change]
            if cell['funcClass']:
                bits.append('<b>Effect:</b> %s' % cell['funcClass'])
            if cell['score'] != '':
                bits.append('<b>Assay score:</b> %s' % cell['score'])
            if cell['outcome']:
                bits.append('<b>ACMG evidence:</b> %s' % cell['outcome'])
            if cell['clinvar']:
                bits.append('<b>ClinVar:</b> %s' % cell['clinvar'])
            if cell.get('conflict'):
                bits.append('several nucleotide changes measured here; '
                            'they disagree in direction. Strongest shown')
            elif cell.get('multi'):
                bits.append('several nucleotide changes measured here; strongest shown')
            if assay:
                bits.append('<b>Assay:</b> %s' % assay)
            # The label field is comma-split by the renderer, so labels carry no commas.
            labelParts.append('<br>'.join(bits).replace(',', ';'))

    # The renderer splits the score array with chopCommas, which keeps a trailing empty
    # field, but the label array with chopByCharRespectDoubleQuotesKeepEmpty, which drops
    # one. If the very last cell is empty the two counts disagree and the track aborts.
    if labelParts[-1] == '':
        labelParts[-1] = '(not measured)'
        stats['trailingFix'] += 1

    bedScore = int(round(1000.0 * pathogenic / measured)) if measured else 0
    shortUrn = urn.replace('urn:mavedb:', '')
    name = '%s %s' % (gene, shortUrn) if gene else shortUrn

    return [
        codonMap.chrom, chromStart, chromEnd, name, bedScore, codonMap.strand,
        chromStart, chromEnd, 0,
        nCols,
        ','.join(str(s) for s in blockSizes) + ',',
        ','.join(str(s) for s in relStarts) + ',',
        len(lib.HEATMAP_ROWS), ','.join(lib.HEATMAP_ROWS),
        FALLBACK_BOUNDS, FALLBACK_COLORS,
        ','.join(scoreParts), ','.join(labelParts),
        assay,
        urn, meta.get('title', ''),
        meta.get('assayMethod', ''), meta.get('assayModel', ''),
        meta.get('assayMechanism', ''), meta.get('libraryMethod', ''),
        gene,
        calTitle, calSource, 'yes' if calRuo else 'no',
        str(measured),
        meta.get('publication', ''),
        'https://mavedb.org/score-sets/%s' % urn,
    ]


if __name__ == '__main__':
    main()

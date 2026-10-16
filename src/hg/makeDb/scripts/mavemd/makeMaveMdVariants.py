#!/usr/bin/env python3
"""Build the MaveMD per-variant bigBed from a fetchMaveMd.py download.

One item per variant per score set. Placement is not done here: it is mavedbPlace.py, in
utils/otto/mavedb, shared with the MaveDB track so a variant lands in the same place in
both. In outline it picks one offset per score set target, checked against the wild-type
residues the experiment itself states, and then checks every placed row against the
reference protein and the genome codon; mavedbPlace.py documents the rules.

That matters here because MaveDB's own mapped terms are wrong for three of MaveMD's score
sets and the error is invisible from the mapped terms alone, which are rewritten to agree
with the codon they chose. NDUFAF6 00000663-a-1 is mapped one codon downstream on 3,668
rows, TPK1 00001251-a-1 is 49 codons downstream on 2,789, and both TARDBP 00000060 score
sets number a prion-like domain construct 1-42 that MaveDB reads as full-protein positions.
Placing from the experiment's residues is what catches all three.

What this script does is the MaveMD part: choose which of a score set's calibrations to
display, color by its ACMG code, and carry the ClinVar, gnomAD and assay columns.

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
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import mavemdLib as lib
import mavedbPlace as mp

csv.field_size_limit(10 ** 7)

clean = mp.clean


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

    Returns urn -> {record, title, gene, publication, calibrations}, where calibrations
    maps a calibration URN to its metadata (title, primary flag, research-use-only flag,
    and the OddsPath of each functional class). record is the record as downloaded, which
    is what mavedbPlace.placeScoreSet() reads the targets and their sequences from.
    """
    scoreSets = {}
    for path in glob.glob(os.path.join(downloadDir, 'scoreSets', '*.json')):
        with open(path) as f:
            record = json.load(f)
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
            'record': record,
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


def readScoreSetCsv(path):
    """One score set's rows, its per-calibration columns and its ClinVar release."""
    with open(path, newline='') as fh:
        reader = csv.DictReader(fh)
        clinvarRelease = ''
        for name in reader.fieldnames:
            if name.startswith('clinvar.') and name.endswith('.clinical_significance'):
                clinvarRelease = name.split('.')[1]
        return list(reader), calibrationColumns(reader.fieldnames), clinvarRelease


def shortChangeOf(pl):
    """The amino acid change for the item name: R175H, R175*, R175=, R175del."""
    alt = 'del' if pl.alt == '-' else pl.alt
    return '%s%d%s' % (pl.wt, pl.protPos, alt)


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('downloadDir')
    parser.add_argument('outBed')
    parser.add_argument('--db', default='hg38')
    parser.add_argument('--twoBit', default='/hive/data/genomes/hg38/hg38.2bit',
                        help='genome sequence, for the wild-type residue check')
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

    csvPaths = {}
    for path in sorted(glob.glob(os.path.join(args.downloadDir, 'variants', '*.csv'))):
        urn = 'urn:mavedb:' + os.path.basename(path)[len('urn_mavedb_'):-len('.csv')]
        csvPaths[urn] = path
    urns = sorted(u for u in csvPaths if u in scoreSets)
    missing = sorted(set(csvPaths) - set(scoreSets))
    if missing:
        sys.stderr.write("  WARNING: no score set record for %s\n" % ', '.join(missing))

    ref = mp.Reference(args.db).load()

    # Pass 1 exists only to learn which proteins to load codon maps for, so they are read
    # once for the whole build rather than per score set. Placement itself is cheap and is
    # redone in pass 2; the codon maps and the genome translation are what cost.
    sys.stderr.write("Pass 1: choosing offsets\n")
    prots = set()
    for urn in urns:
        rows, _, _ = readScoreSetCsv(csvPaths[urn])
        pls, _, _ = mp.placeScoreSet(scoreSets[urn]['record'], rows, ref)
        prots.update(p.prot for p in pls if p.status == 'placed' and p.prot)
        prots.update(p.mavedbProt for p in pls if p.mavedbProt)
    protToMap, unresolvedProt = mp.loadMapsForProteins(ref, prots, args.twoBit, workDir)
    sys.stderr.write("  codon maps for %d proteins\n" % len(protToMap))
    if unresolvedProt:
        sys.stderr.write("  WARNING: no transcript or codon map for %s\n"
                         % ', '.join(sorted(unresolvedProt)))
    for prot in sorted(protToMap):
        cm = protToMap[prot]
        sys.stderr.write("  %-18s -> %-18s %s %s  %d codons\n"
                         % (prot, cm.tx, cm.chrom, cm.strand, cm.protLen))

    stats = collections.Counter()
    placeStats = collections.Counter()
    reasons = collections.Counter()
    offsets = []
    filterValues = collections.defaultdict(set)
    rows = []

    sys.stderr.write("Pass 2: building items\n")
    for urn in urns:
        meta = scoreSets[urn]
        csvRows, calCols, clinvarRelease = readScoreSetCsv(csvPaths[urn])
        pls, choices, _ = mp.placeScoreSet(meta['record'], csvRows, ref)
        placeStats.update('referenceTerm:' + k
                          for k in mp.placeReferenceTerms(pls, ref, workDir).elements())
        placeStats.update(mp.checkGenome(pls, ref, args.twoBit, workDir, protToMap))
        offsets.append((urn, choices))
        gene = meta.get('gene') or ''

        for pl in pls:
            stats['rows'] += 1
            row = pl.row
            if pl.status != 'placed':
                reasons[pl.reason or 'unplaced'] += 1
                continue

            # A protein placement covers its codon, which is two blocks when an intron
            # splits it: the intron is not part of the codon, and a solid bar across it
            # would show a measurement where none was made. A row placed only at genomic
            # level covers the bases its term names.
            if pl.codon is not None:
                chrom, strand = pl.chrom, pl.strand
                runs = mp.codonRuns(pl.codon)
            else:
                chrom, strand = ref.ncToChrom[pl.genomic.acc], '.'
                runs = [pl.genomic.bedRange()]
            start, end = runs[0][0], runs[-1][1]
            if len(runs) > 1:
                stats['splitCodonItem'] += 1
            blockSizes = [e - b for b, e in runs]
            blockStarts = [b - start for b, _ in runs]

            protein = '%s:%s' % (pl.prot, mp.proteinTermText(pl)) if pl.prot else ''
            mavedbTerms = pl.label == mp.PLACED_MAVEDB
            genomic = pl.genomic.term if pl.genomic else ''
            submittedNt = clean(row.get('hgvs_nt'))
            cdna = (clean(row.get('mavedb.post_mapped_hgvs_c')) if mavedbTerms
                    else (submittedNt if ':' in submittedNt else ''))

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

            shortChange = shortChangeOf(pl) if pl.prot else genomic.split(':')[-1]
            name = '%s:%s' % (gene, shortChange) if gene else shortChange

            allCals = '; '.join(
                '%s%s: %s' % (c['title'],
                              ' (research use only)' if c['ruo'] else '',
                              c['acmgOutcome'] or c['funcClass'] or 'no call')
                for c in calls) or 'none'

            variantUrn = clean(row.get('accession'))
            score = strengthScore(outcome, chosen['acmgStrength'] if chosen else '')
            # The mouseOver pattern substitutes fields unconditionally, so an empty
            # acmgOutcome leaves a dangling "ACMG evidence:" label on the 13% of items
            # whose score set classifies variants without assigning an ACMG code. Both
            # labels are set after color and score so neither is affected by them, and
            # both go into the filter menus as real categories.
            outcomeLabel = outcome or 'none assigned'
            funcClassLabel = funcClass or 'not classified'
            scoreLabel = mp.fmtScore(clean(row.get('scores.score'))) or 'not reported'
            clinvarSig = clean(row.get('clinvar.%s.clinical_significance' % clinvarRelease))
            clinvarReview = clean(row.get('clinvar.%s.clinical_review_status' % clinvarRelease))
            vepConsequence = clean(row.get('vep.vep_functional_consequence'))
            gnomadVersion = clean(row.get('gnomad.gnomad_version'))
            # These three are filter columns. An empty value is not offered as a
            # filter choice, so any selection silently drops the rows that have none
            # (22,394 of them, from score sets whose experiment record has no assay
            # keywords). Give them a value so they stay reachable.
            assayMethod = meta.get('assayMethod', '') or 'not recorded'
            assayModel = meta.get('assayModel', '') or 'not recorded'
            libraryMethod = meta.get('libraryMethod', '') or 'not recorded'

            item = [
                chrom, start, end, name, score, strand, start, end,
                lib.hexToBedRgb(color),
                len(blockStarts),
                ','.join(str(b) for b in blockSizes) + ',',
                ','.join(str(b) for b in blockStarts) + ',',
                gene,
                protein,
                outcomeLabel,
                chosen['acmgStrength'] if chosen else '',
                funcClassLabel,
                scoreLabel,
                mp.fmtScore(chosen['odds'], 3) if chosen and chosen['odds'] != '' else '',
                lib.calibrationLabel(chosen['title'] if chosen else '',
                                     chosen['ruo'] if chosen else False),
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
                cdna,
                clean(row.get('clingen.clingen_allele_id')),
                variantUrn,
                pl.label,
                mp.bedField(pl.construct or ''),
            ]
            rows.append(item)
            stats['placed'] += 1
            stats['placement:' + pl.label] += 1
            filterValues['acmgOutcome'].add(outcomeLabel)
            filterValues['funcClass'].add(funcClassLabel)
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

    # The per-accession residue check and the cross-check of MaveDB's genomic terms
    # against our codon both run inside mavedbPlace.checkGenome(). crossCheckGate() stops
    # the build when the terms disagree with our codon too often over the whole run,
    # which is what a drift in RefSeq or in MaveDB's mapper would look like.
    mp.crossCheckGate(placeStats)

    sys.stderr.write("\nOffset chosen per score set target\n")
    for urn, choices in offsets:
        for prefix, (choice, why) in sorted(choices.items(), key=lambda kv: str(kv[0])):
            sys.stderr.write("  %-26s %-16s %s\n"
                             % (urn.split(':')[-1], prefix or '-',
                                ('%s%+d %s via %s%s'
                                 % (choice.prot, choice.offset, choice.kind, choice.route,
                                    '; ' + choice.constructNote() if choice.constructNote()
                                    else ''))
                                if choice else 'no offset (%s)' % why))

    rows.sort(key=lambda r: (r[0], r[1], r[2]))
    with open(args.outBed, 'w') as out:
        for item in rows:
            out.write('\t'.join(mp.bedField(f) for f in item) + '\n')

    sys.stderr.write("\nBuild summary\n")
    for key in sorted(stats):
        sys.stderr.write("  %-36s %d\n" % (key, stats[key]))
    sys.stderr.write("Not placed\n")
    for key in sorted(reasons, key=lambda k: -reasons[k]):
        sys.stderr.write("  %-36s %d\n" % (key, reasons[key]))
    sys.stderr.write("Placement checks\n")
    for key in sorted(placeStats):
        sys.stderr.write("  %-36s %d\n" % (key, placeStats[key]))

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
                # One newline, not two: a blank line ends a trackDb stanza, so a
                # fragment with blank separators silently orphans every setting after
                # the first when it is pasted in.
                fh.write('filterLabel.%s %s\n' % (field, label))
        sys.stderr.write("Wrote trackDb filter fragment to %s\n" % args.raFragment)


if __name__ == '__main__':
    main()

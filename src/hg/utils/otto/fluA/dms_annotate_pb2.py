#!/usr/bin/env python3

"""
Read the Bloom Lab PB2 deep mutational scanning (DMS) summary CSV file and extract the mutdiffsel
score for each PB2 mutation; then scan nextclade output TSV for sequences for mutations with a
score; write TSV output relating each sequence to phenotype scores (to be folded in with
other sequence metadata, and ultimately used to decorate the tree in Nextstrain or taxonium).

NOTE: the positions in the sequential_site column of expected DMS input
https://raw.githubusercontent.com/jbloomlab/PB2-DMS/master/results/diffsel/summary_prefs_effects_diffsel.csv
are 1-based offsets in the PB2 protein of A/green-winged teal/Ohio/175/1986 (CY018884.1)
-- so the nextclade TSV should be generated with CY018884.1 as reference.

HT https://github.com/moncla-lab/annotate-dms/blob/main/annotate-dms.py
"""

import argparse, sys, re, csv, json
import gzip, lzma

refLen = 760   # reference amino acid length
cdsStart = 13  # 1-based nucleotide pos of CDS start in CY018884.1

# Colors taken from https://github.com/moncla-lab/annotate-dms/blob/main/annotate-dms.py
min_hex = '#DC2F24'
mid_hex = '#ABABAB'
max_hex = '#4041C7'

# For now there's only one column of interest, but just in case we want to add others:
phenoNames = [ 'mutdiffsel' ]

auspiceLabels = { 'mutdiffsel': 'Differential selection in human vs. avian cells' }
dmsCsvHeaders = { 'mutdiffsel': 'mutdiffsel' }

def parseCommandLine():
    parser = argparse.ArgumentParser()
    parser.add_argument('dmsCsvIn', help="Comma-separated DMS results summary, must include columns '" +
                        "', '".join(dmsCsvHeaders.values()) + "'")
    parser.add_argument('nextcladeTsvIn', help="Tab-separated output of nextclade, must include columns 'seqName', 'alignmentStart', 'alignmentEnd', 'aaSubstitutions'")
    parser.add_argument('metadataTsvOut', help="Tab-separated metadata output file, columns will be 'strain' followed by per-phenotype sums of mutation scores followed by per-phenotype mutation lists")
    parser.add_argument('auspiceColoringsOut', help="JSON-fragment output file containing coloring definitions for phenotype sums, scaled to min/max across all sequences")
    args = parser.parse_args()
    return args


# Copied from Chris's hashSeqs.py, libify:
def openMaybeCompressed(fname):
    """Get an open file handle on an optionally gzipped file."""
    if fname[-3:] == ".gz":
        return gzip.open(fname, "rt")
    elif fname[-3:] == ".xz":
        return lzma.open(fname, "rt")
    elif fname == "stdin":
        return sys.stdin
    else:
        return open(fname, "rt")


def floatOrNone(val):
    """If val is None or empty string, return None, otherwise return float val."""
    if val is None or val == '':
        return None
    else:
        return float(val)


def discardNegative(val):
    """If val is None or negative, return None, otherwise return val."""
    if val is None or val < 0:
        return None
    else:
        return val


def parseDmsCsv(dmsCsvIn):
    """Read in DMS .csv phenotype summary file, return dict of phenotype name to dict of amino acid
    mutation in PB2 amino acid coords to phenotype score.
    Discard negative mutdiffsel values because Jesse Bloom said they are noisy (Slack 9/18/24)."""
    phenoMutScore = { pheno: {} for pheno in phenoNames }
    with openMaybeCompressed(dmsCsvIn) as f:
        reader = csv.DictReader(f)
        for row in reader:
            mut = row['site'] + row['mutation']
            for pheno in phenoNames:
                score = floatOrNone(row[dmsCsvHeaders[pheno]])
                if pheno == 'mutdiffsel':
                    score = discardNegative(score)
                if score is not None:
                    phenoMutScore[pheno][mut] = score
    return phenoMutScore


def parseAaMuts(aaSubstitutions, posToBase):
    """Break down nextclade's comma-separated list of substitutions like PB2:I310V and store base
    in posToBase, an array mapping pos to alt allele (or if mapRef=True, to ref allele)."""
    if not aaSubstitutions:
        return
    substList = aaSubstitutions.split(',')
    for subst in substList:
        m = re.search('^PB2:[A-Z*-]([0-9]+)([A-Z*-])$', subst)
        if not m:
            print(f"Error: expected PB2 substitution like 'PB2:I310V' but got '{subst}'",
                  file=sys.stderr)
            exit(1)
        pos, alt = m.groups()
        pos = int(pos)
        posToBase[pos] = alt
    return len(substList)


def dmsAnnotate(dmsCsvIn, nextcladeTsvIn, metadataTsvOut, colorJsonOut):
    """Extract scores for phenotypes of interest from dmsCsvIn.  Scan through nextcladeTsvIn,
    looking up each sequence's mutations in the DMS phenotype scores.  For each sequence and each
    phenotype, build composite scores and lists of mutations and scores, and output those as TSV
    metadata for each sequence.  Keep track of min and max score for each phenotype to use for
    coloring in Nextstrain/Auspice.  Output coloring specs to be included in an Auspice header."""
    phenoMutScore = parseDmsCsv(dmsCsvIn)
    # Write metadata column header
    print('\t'.join([ 'strain' ] +
                    [ pheno for pheno in phenoNames ] +
                    [ pheno + "_mutations" for pheno in phenoNames ] +
                    [ 'aa_substitution_count' ]), file=metadataTsvOut)
    min_sums = { pheno: None for pheno in phenoNames }
    max_sums = { pheno: None for pheno in phenoNames }
    max_subst_count = 0
    with openMaybeCompressed(nextcladeTsvIn) as f:
        reader = csv.DictReader(f, delimiter='\t')
        for row in reader:
            if 'seqName' not in row:
                print("No seqName in row; printing out keys", file=sys.stderr)
                print(row.keys(), file=sys.stderr)
                exit(1)
            # Ignore rows that have empty string for all columns except index, seqName and errors
            # due to failure to align to reference.  For those rows, empty string in the
            # aaSubstitutions column does not mean they have no AA substs relative to the reference!
            # Test the alignmentStart column because it should have an integer:
            if row['alignmentStart'] == '':
                continue
            sums = { pheno: 0.0 for pheno in phenoNames }
            mutDescs = { pheno: '' for pheno in phenoNames }
            # Parse amino acid changes relative to reference
            # refLen + 1 for 1-based pos, + 1 for stop-loss substitition
            posToBase = [None] * (refLen + 2)
            aa_subst_count = parseAaMuts(row['aaSubstitutions'], posToBase)
            if aa_subst_count is not None and aa_subst_count > max_subst_count:
                max_subst_count = aa_subst_count
            # Sum up scores from all mutations for each phenotype:
            for pos, base in enumerate(posToBase):
                if base is not None:
                    mut = str(pos) + str(base)
                    for pheno in phenoNames:
                        if mut in phenoMutScore[pheno]:
                            score = phenoMutScore[pheno][mut]
                            if score != 0.0:
                                sums[pheno] += score
                                if mutDescs[pheno]:
                                    mutDescs[pheno] += ', '
                                roundedScore=round(score, 5)
                                mutDescs[pheno] += f"{pos}{base} [{roundedScore}]"
            print('\t'.join([ row['seqName'] ] +
                            [ str(round(sums[pheno], 5)) for pheno in phenoNames ] +
                            [ mutDescs[pheno] for pheno in phenoNames ] +
                            [ str(aa_subst_count) ]), file=metadataTsvOut)
            for pheno in phenoNames:
                if min_sums[pheno] is None or sums[pheno] < min_sums[pheno]:
                    min_sums[pheno] = sums[pheno]
                if max_sums[pheno] is None or sums[pheno] > max_sums[pheno]:
                    max_sums[pheno] = sums[pheno]
    # Write Auspice JSON coloring definitions for each phenotype
    colorings = []
    for pheno in phenoNames:
        # Scaling copied from https://github.com/moncla-lab/annotate-dms/blob/main/annotate-dms.py
        min_value = round(min_sums[pheno], 5)
        max_value = round(max_sums[pheno], 5)
        if min_value < 0:
            abs_min = min(min_value, -max_value)
            abs_max = -abs_min
            scale = [[abs_min, min_hex], [0, mid_hex], [abs_max, max_hex]]
        else:
            scale = [[0, mid_hex], [max_value, max_hex]]
        colorings.append({ 'key': pheno,
                           'title': auspiceLabels[pheno],
                           'type': 'continuous',
                           'scale': scale })
    # Scale for AA substitution count
    scale = [[0, mid_hex], [max_subst_count, max_hex]]
    colorings.append({ 'key': 'aa_substitution_count',
                       'title': 'Number of amino acid mutations relative to DMS reference A/green-winged teal/Ohio/175/1986',
                       'type': 'continuous',
                           'scale': scale })
    print(', '.join([json.dumps(c) for c in colorings]), file=colorJsonOut)


def main():
    args = parseCommandLine()
    with open(args.metadataTsvOut, 'w') as metadataTsvOut:
        with open(args.auspiceColoringsOut, 'w') as auspiceColoringsOut:
            dmsAnnotate(args.dmsCsvIn, args.nextcladeTsvIn, metadataTsvOut, auspiceColoringsOut)

if __name__ == "__main__":
    main()

#!/usr/bin/env python3

"""Read a Bloom lab H5N1 deep mutational scanning (DMS) summary CSV file to get several phenotype
scores for each HA mutation, then scan nextclade output TSV for sequences for mutations with a
phenotype score; write TSV output relating each sequence to phenotype scores (to be folded in with
other sequence metadata, and ultimately used to decorate the tree in Nextstrain or taxonium).

NOTE: the positions in the sequential_site column of expected DMS input
https://github.com/dms-vep/Flu_H5_American-Wigeon_South-Carolina_2021-H5N1_DMS/blob/main/results/summaries/all_sera_escape.csv
are 1-based offsets in the HA protein of A/American Wigeon/South Carolina/22-000345-001/2021 
https://www.ncbi.nlm.nih.gov/nuccore/OQ958044.1
-- so the nextclade TSV should be generated with OQ958044.1 as reference.

This is largely inspired by https://github.com/moncla-lab/annotate-dms/blob/main/annotate-dms.py;
the main difference is that it is written for Auspice pipeline files (JSON everything) and a
reference that is more similar to the DMS wigeon reference, while I'm working with nextclade output
and tab-separated metadata (and a bit of Auspice config JSON).
"""

import argparse, sys, re, csv, json
import gzip, lzma

refLen = 568   # reference amino acid length
cdsStart = 1   # 1-based nucleotide pos (OQ958044.1 is CDS-only)

# Colors taken from https://github.com/moncla-lab/annotate-dms/blob/main/annotate-dms.py
min_hex = '#DC2F24'
mid_hex = '#ABABAB'
max_hex = '#4041C7'

# phenotypes of interest
phenoNames = [ 'mouse_escape', 'ferret_escape', 'cell_entry', 'stability', 'sa26_increase' ]

# Values copied from https://github.com/moncla-lab/annotate-dms/blob/main/dms_config.tsv
auspiceLabels = { 'mouse_escape':  'Mouse sera escape relative to A/American-Wigeon/South-Carolina/2021',
                  'ferret_escape': 'Ferret sera escape relative to A/American-Wigeon/South-Carolina/2021',
                  'cell_entry':    'Cell entry',
                  'stability':     'HA stability',
                  'sa26_increase': 'Increase in a2,6 sialic acid usage' }
dmsCsvHeaders = { 'mouse_escape':  'mouse sera escape',
                  'ferret_escape': 'ferret sera escape',
                  'cell_entry':    'entry in 293T cells',
                  'stability':     'stability',
                  'sa26_increase': 'SA26 usage increase' }

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
    mutation in HA amino acid coords to phenotype score.
    If 'SA26 usage increase' or sera escape scores are negative, skip them because Jesse Bloom said
    they are less reliably measured and have unclear biological interpretation (email 5/28/2024)."""
    # Actually the 'SA26 usage increase' values seem to have already been clipped at 0, but it's
    # harmless to clip them here again.
    phenoMutScore = { pheno: {} for pheno in phenoNames }
    with openMaybeCompressed(dmsCsvIn) as f:
        reader = csv.DictReader(f)
        for row in reader:
            mut = row['sequential_site'] + row['mutant']
            for pheno in phenoNames:
                score = floatOrNone(row[dmsCsvHeaders[pheno]])
                if pheno in [ 'mouse_escape', 'ferret_escape', 'sa26_increase' ]:
                    score = discardNegative(score)
                if score is not None:
                    phenoMutScore[pheno][mut] = score
    return phenoMutScore


def parseAaMuts(aaSubstitutions, posToBase):
    """Break down nextclade's comma-separated list of substitutions like HA:I310V and store base
    in posToBase, an array mapping pos to alt allele (or if mapRef=True, to ref allele)."""
    if not aaSubstitutions:
        return
    substList = aaSubstitutions.split(',')
    for subst in substList:
        m = re.search('^HA:[A-Z*-]([0-9]+)([A-Z*-])$', subst)
        if not m:
            print(f"Error: expected HA substitution like 'HA:I310V' but got '{subst}'",
                  file=sys.stderr)
            exit(1)
        pos, alt = m.groups()
        pos = int(pos)
        posToBase[pos] = alt


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
                    [ pheno + "_mutations" for pheno in phenoNames ]), file=metadataTsvOut)
    min_sums = { pheno: None for pheno in phenoNames }
    max_sums = { pheno: None for pheno in phenoNames }
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
            parseAaMuts(row['aaSubstitutions'], posToBase)
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
                                mutDescs[pheno] += f"{pos}{base} [{score}]"
            print('\t'.join([ row['seqName'] ] +
                            [ str(round(sums[pheno], 5)) for pheno in phenoNames ] +
                            [ mutDescs[pheno] for pheno in phenoNames ]), file=metadataTsvOut)
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
    print(', '.join([json.dumps(c) for c in colorings]), file=colorJsonOut)


def main():
    args = parseCommandLine()
    with open(args.metadataTsvOut, 'w') as metadataTsvOut:
        with open(args.auspiceColoringsOut, 'w') as auspiceColoringsOut:
            dmsAnnotate(args.dmsCsvIn, args.nextcladeTsvIn, metadataTsvOut, auspiceColoringsOut)

if __name__ == "__main__":
    main()

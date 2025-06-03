#!/usr/bin/env python3

"""Merge two maple files with different mutations for the same sequences (for example in different position ranges, or Ns in one file and substitutions in the other file.  Print output to stdout.  The first file will be held in memory, so if one file is smaller then make that the first argument.  If an N from one file overlaps with a substitution in the other file, the N will take precedence; other conflicts will cause an error exit.  All records in the second file will be printed out; records in the first file that do not appear in the second file will be dropped."""


import sys, logging, argparse, gzip, lzma, re

def parseCommandLine():
    parser = argparse.ArgumentParser()
    parser.add_argument('a', help="First input Maple/'diff' file, parsed into memory")
    parser.add_argument('b', help="Second input Maple/'diff' file, streamed through")
    args = parser.parse_args()
    return args

def die(msg):
    logging.error(msg)
    sys.exit(255)

def openMaybeDecompress(filename):
    if filename.endswith('.gz'):
        return gzip.open(filename, 'rt')
    elif filename.endswith('.xz'):
        return lzma.open(filename, 'rt')
    else:
        return open(filename, 'r')

def isAmbig(base):
    # Return True if base is in the IUPAC ambiguous set
    return True if base in 'nryswkmbdhv' else False;

def printDiff(base, start, end):
    pos = start + 1
    if base == 'n' or base == '-':
        length = end - start
        print(f"{base}\t{pos}\t{length}")
    else:
        print(f"{base}\t{pos}")

def maple_generator(filename):
    sample_set = {}
    name = ''
    muts = []
    last_end = 0
    with openMaybeDecompress(filename) as f:
        for line in f:
            if line.startswith('>'):
                if name != '':
                    yield name, muts
                    muts = []
                    last_end = 0
                name=line[1:].strip();
                if name == '':
                    die(f"File {filename} has line with no name following '>' -- all records must have non-empty names.")
                if sample_set.get(name) is not None:
                    die(f"File {filename} has more than one record with name '{name}' -- records must have unique names within each file.")
            else:
                words = line.split('\t')
                words[-1] = words[-1].rstrip()
                # Convert pos and optional len to 0-based, half open range
                words[1] = int(words[1]) - 1
                if (words[1] < last_end):
                    die(f"pos {words[1]+1} <= last item's end {last_end} for name={name} -- diffs must be sorted by pos and non-overlapping")
                if len(words) > 2:
                    length = int(words[2])
                    words[2] = words[1] + length
                else:
                    words.append(words[1] + 1)
                last_end = words[2]
                muts.append(words)
    if name != '':
        yield name, muts


def mergeOne(name, a_muts, b_muts):
    print(f">{name}")
    ix_a = 0
    ix_b = 0
    count_a = len(a_muts)
    count_b = len(b_muts)
    while (ix_a < count_a and ix_b < count_b):
        [base_a, start_a, end_a] = a_muts[ix_a]
        [base_b, start_b, end_b] = b_muts[ix_b]
        if end_b < start_a:
            # b strictly precedes a
            printDiff(base_b, start_b, end_b)
            ix_b += 1
        elif end_a < start_b:
            # a strictly precedes b
            printDiff(base_a, start_a, end_a)
            ix_a += 1
        elif end_b == start_a:
            # adjacent -- merge b and a if multi-base (n or gap):
            if base_b == base_a and (base_b == 'n' or base_b == '-'):
                printDiff(base_b, start_b, end_a)
                ix_a += 1
                ix_b += 1
            else:
                printDiff(base_b, start_b, end_b)
                ix_b += 1
        elif end_a == start_b:
            # adjacent -- merge a and b if multi-base (n or gap):
            if base_a == base_b and (base_a == 'n' or base_a == '-'):
                printDiff(base_a, start_a, end_b)
                ix_a += 1
                ix_b += 1
            else:
                printDiff(base_a, start_a, end_a)
                ix_a += 1
        elif start_a == start_b and end_a == end_b:
            # identical range; check for conflicting base (but n/ambig wins)
            if base_a == 'n' or base_b == 'n':
                printDiff('n', start_a, end_a)
                ix_a += 1
                ix_b += 1
            elif base_a == base_b:
                printDiff(base_a, start_a, end_a)
                ix_a += 1
                ix_b += 1
            elif isAmbig(base_a):
                printDiff(base_a, start_a, end_a)
                ix_a += 1
                ix_b += 1
            elif isAmbig(base_b):
                printDiff(base_b, start_b, end_b)
                ix_a += 1
                ix_b += 1
            else:
                die(f"Conflict between [{base_a}, {start_a}, {end_a}] and [{base_b}, {start_b}, {end_b}] for name={name}")
        elif start_a == start_b and end_b > end_a:
            # Shared start, b extends to right; check for conflicting base (but n wins)
            if base_b == 'n':
                ix_a += 1
            elif base_a == 'n':
                printDiff(base_a, start_a, end_a)
                ix_a += 1
                b_muts[ix_b][1] = end_a
            elif base_a == base_b:
                ix_a += 1
            else:
                die(f"Conflict between [{base_a}, {start_a}, {end_a}] and [{base_b}, {start_b}, {end_b}] for name={name}")
        elif start_a == start_b and end_a > end_b:
            # Shared start, a extends to the right; check for conflicting base (but n wins)
            if base_a == 'n':
                ix_b += 1
            elif base_b == 'n':
                printDiff(base_b, start_b, end_b)
                ix_b += 1
                a_muts[ix_a][1] = end_b
            elif base_a == base_b:
                ix_b += 1
            else:
                die(f"Conflict between [{base_a}, {start_a}, {end_a}] and [{base_b}, {start_b}, {end_b}] for name={name}")
        elif start_b < start_a and end_b == end_a:
            # Shared end, b extends to left; check for conflicting base (but n wins)
            if base_b == 'n':
                ix_a += 1
            elif base_a == 'n':
                printDiff(base_b, start_b, start_a)
                ix_b += 1
            elif base_a == base_b:
                ix_a += 1
            else:
                die(f"Conflict between [{base_a}, {start_a}, {end_a}] and [{base_b}, {start_b}, {end_b}] for name={name}")
        elif start_a < start_b and end_a == end_b:
            # Shared end, a extends to left; check for conflicting base (but n wins)
            if base_a == 'n':
                ix_b += 1
            elif base_b == 'n':
                printDiff(base_a, start_a, start_b)
                ix_a += 1
            elif base_a == base_b:
                ix_b += 1
            else:
                die(f"Conflict between [{base_a}, {start_a}, {end_a}] and [{base_b}, {start_b}, {end_b}] for name={name}")
        elif start_b < start_a and end_b > end_a:
            # b contains a; check for conflicting base (but n wins)
            if base_b == 'n':
                ix_a += 1
            elif base_a == 'n':
                printDiff(base_b, start_b, start_a)
                printDiff(base_a, start_a, end_a)
                ix_a += 1
                b_muts[ix_b][1] = end_a
            elif base_a == base_b:
                ix_a += 1
            else:
                die(f"Conflict between [{base_a}, {start_a}, {end_a}] and [{base_b}, {start_b}, {end_b}] for name={name}")
        elif start_a < start_b and end_a > end_b:
            # a contains b; check for conflicting base (but n wins)
            if base_a == 'n':
                ix_b += 1
            elif base_b == 'n':
                printDiff(base_a, start_a, start_b)
                printDiff(base_b, start_b, end_b)
                ix_b += 1
                a_muts[ix_a][1] = end_b
            elif base_a == base_b:
                ix_b += 1
            else:
                die(f"Conflict between [{base_a}, {start_a}, {end_a}] and [{base_b}, {start_b}, {end_b}] for name={name}")
        else:
            die(f"Unexpected coord permutation: a is [{base_a}, {start_a}, {end_a}], b is [{base_b}, {start_b}, {end_b}] for name={name}")
    while ix_a < count_a:
        [base_a, start_a, end_a] = a_muts[ix_a]
        printDiff(base_a, start_a, end_a)
        ix_a += 1
    while ix_b < count_b:
        [base_b, start_b, end_b] = b_muts[ix_b]
        printDiff(base_b, start_b, end_b)
        ix_b += 1


def mapleMerge(a, b):
    a_sample_muts = {}
    gen_a = maple_generator(a)
    for name, muts in gen_a:
        a_sample_muts[name] = muts
    gen_b = maple_generator(b)
    for name, muts in gen_b:
        a_muts = a_sample_muts.get(name, [])
        mergeOne(name, a_muts, muts)


def main():
    args = parseCommandLine()
    mapleMerge(args.a, args.b)

if __name__ == "__main__":
    main()

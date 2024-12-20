#!/usr/bin/env python

# adapted from https://github.com/nextstrain/avian-flu/blob/master/scripts/join-segments.py

from Bio import SeqIO
from collections import defaultdict
import argparse, sys, re

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--segments', type = str, required = True, nargs='+',
                        help = "per-segment aligned fasta")
    parser.add_argument('--output', type = str, required = True,
                        help = "output whole genome aligned fasta")
    args = parser.parse_args()

    records = {}
    strain_counts = defaultdict(int)
    insdcRe = re.compile('^[A-Z]{2}[0-9]{6}\\.[0-9]+$')
    for fname in args.segments:
        records[fname] = {}
        for record in SeqIO.parse(fname, 'fasta'):
            # Remove GenBank accession, but not SRA, from record.name
            nameParts = record.name.split("|")
            if len(nameParts) == 3 and insdcRe.search(nameParts[1]):
                name = "|".join([nameParts[0], nameParts[2]])
            else:
                name = record.name
            records[fname][name] = str(record.seq)
        for key in records[fname]: strain_counts[key]+=1
        print(f"{fname}: parsed {len(records[fname].keys())} sequences")

    with open(args.output, 'w') as fh:
        print("writing concatenated segments to ", args.output)
        for name,count in strain_counts.items():
            if count!=len(args.segments):
                print(f"Excluding {name} as it only appears in {count} segments")
                continue
            genome = "".join([records[seg][name] for seg in args.segments])
            print(f">{name}\n{genome}", file=fh)

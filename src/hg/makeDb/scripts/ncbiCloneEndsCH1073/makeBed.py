#!/usr/bin/env python3
"""Convert an NCBI clone-reports GFF (clone_insert records only) to a
BED 6+7 file matching cloneEnds.as.  Maps RefSeq contig names to UCSC
chrom names via refSeq.ucscName.tab and clamps to chrom.sizes.

Usage:
  makeBed.py <refSeqMap.tab> <chrom.sizes> <input.gff> > out.bed
"""

import sys

OVERSIZE_THRESHOLD = 500_000


def load_map(path):
    m = {}
    with open(path) as fh:
        for line in fh:
            line = line.rstrip("\n")
            if not line:
                continue
            k, v = line.split("\t", 1)
            m[k] = v
    return m


def load_sizes(path):
    s = {}
    with open(path) as fh:
        for line in fh:
            line = line.rstrip("\n")
            if not line:
                continue
            name, size = line.split("\t")
            s[name] = int(size)
    return s


def parse_attrs(field):
    out = {}
    for kv in field.split(";"):
        if "=" in kv:
            k, v = kv.split("=", 1)
            out[k] = v
    return out


def main(map_path, sizes_path, gff_path):
    refseq_to_ucsc = load_map(map_path)
    sizes = load_sizes(sizes_path)

    n_in = n_out = n_unmapped = n_oversize = n_nosize = 0
    unmapped_contigs = {}

    with open(gff_path) as fh:
        for line in fh:
            if line.startswith("#"):
                continue
            parts = line.rstrip("\n").split("\t")
            if len(parts) < 9:
                continue
            if parts[2] != "clone_insert":
                continue
            n_in += 1

            refseq = parts[0]
            ucsc = refseq_to_ucsc.get(refseq)
            if not ucsc or ucsc not in sizes:
                n_unmapped += 1
                unmapped_contigs[refseq] = unmapped_contigs.get(refseq, 0) + 1
                continue

            start = int(parts[3]) - 1
            end = int(parts[4])
            chrom_size = sizes[ucsc]
            if start < 0:
                start = 0
            if end > chrom_size:
                sys.stderr.write(
                    f"# warn: clamping end {end} -> {chrom_size} on {ucsc}\n")
                end = chrom_size
            if start >= end:
                n_nosize += 1
                continue

            attrs = parse_attrs(parts[8])
            name = attrs.get("Name", "?")
            ncbi_id = attrs.get("ID", "?")
            concordant = attrs.get("concordant", "?")
            unique = attrs.get("unique", "?")
            method = attrs.get("placement-method", "?")
            assm_unit = attrs.get("assm_unit_name", "?").replace(" ", "_")

            insert_size = end - start
            oversize = "TRUE" if insert_size > OVERSIZE_THRESHOLD else "FALSE"
            if oversize == "TRUE":
                n_oversize += 1

            row = [
                ucsc, str(start), str(end), name, "0", "+",
                str(insert_size), concordant, unique, assm_unit,
                oversize, ncbi_id, method,
            ]
            print("\t".join(row))
            n_out += 1

    sys.stderr.write(f"# input clone_insert rows:  {n_in}\n")
    sys.stderr.write(f"# output bed rows:          {n_out}\n")
    sys.stderr.write(f"# unmapped contig rows:     {n_unmapped}\n")
    sys.stderr.write(f"# zero-length rows skipped: {n_nosize}\n")
    sys.stderr.write(f"# oversize (>500kb) flagged:{n_oversize}\n")
    if unmapped_contigs:
        sys.stderr.write("# unmapped contigs (top 10 by count):\n")
        top = sorted(unmapped_contigs.items(),
                     key=lambda kv: -kv[1])[:10]
        for k, v in top:
            sys.stderr.write(f"#   {k}\t{v}\n")


if __name__ == "__main__":
    if len(sys.argv) != 4:
        sys.stderr.write(
            "usage: makeBed.py <refSeqMap.tab> <chrom.sizes> <input.gff>\n")
        sys.exit(1)
    main(sys.argv[1], sys.argv[2], sys.argv[3])

#!/usr/bin/env python3
"""Lift a miRBase <species>.gff3 to another assembly, one precursor locus at a time.

  mirBaseLift.py in.gff3 over.chain.gz out.gff3

Each precursor (miRNA_primary_transcript) is lifted together with the matures (miRNA)
that lie inside it, as mirBaseToBed.py assigns them. A precursor and its matures are
kept only if all of them lift, to the chromosome and strand of the lifted precursor,
with their lengths unchanged, so the mature positions in the precursor sequence stay
the same. The other precursors are left out with their matures and listed on stderr.
Comment lines are copied, with a line added that says the file was lifted.
"""
import subprocess
import sys
import tempfile

def main(inGff, chain, outGff):
    header, feats = [], []
    for line in open(inGff):
        if line.startswith("#"):
            header.append(line)
        else:
            feats.append(line.rstrip("\n").split("\t"))
    pres = [i for i, c in enumerate(feats) if c[2] == "miRNA_primary_transcript"]
    # each mature belongs to the precursor locus with its accession that contains it
    attrs = [dict(a.split("=", 1) for a in c[8].split(";") if "=" in a) for c in feats]
    owner = {}
    for i, c in enumerate(feats):
        if c[2] != "miRNA":
            continue
        cand = [p for p in pres if attrs[p]["Alias"] == attrs[i]["Derives_from"] and
                feats[p][0] == c[0] and feats[p][6] == c[6] and
                int(feats[p][3]) <= int(c[3]) and int(c[4]) <= int(feats[p][4])]
        if len(cand) != 1:
            sys.exit("%d precursor loci contain mature %s" % (len(cand), attrs[i]["ID"]))
        owner[i] = cand[0]

    with tempfile.TemporaryDirectory() as tmp:
        with open(tmp + "/in.bed", "w") as f:
            for i, c in enumerate(feats):
                f.write("%s\t%d\t%s\t%d\t0\t%s\n" % (c[0], int(c[3]) - 1, c[4], i, c[6]))
        subprocess.run(["liftOver", tmp + "/in.bed", chain, tmp + "/out.bed", tmp + "/unmapped"],
                       check=True, capture_output=True)
        lifted = {}
        for l in open(tmp + "/out.bed"):
            c = l.rstrip("\n").split("\t")
            lifted[int(c[3])] = (c[0], int(c[1]), int(c[2]), c[5])

    members = {p: [p] for p in pres}
    for i, p in owner.items():
        members[p].append(i)
    keep, dropped = set(), {}
    for p, idx in members.items():
        reason = None
        for i in idx:
            src, dst = feats[i], lifted.get(i)
            if dst is None:
                reason = "%s not lifted" % attrs[i]["ID"]
            elif dst[2] - dst[1] != int(src[4]) - int(src[3]) + 1:
                reason = "%s changes length" % attrs[i]["ID"]
            elif p in lifted and (dst[0], dst[3]) != (lifted[p][0], lifted[p][3]):
                reason = "%s on another chromosome or strand" % attrs[i]["ID"]
            if reason:
                break
        if reason:
            dropped[attrs[p]["ID"]] = (attrs[p]["Name"], reason)
        else:
            keep.update(idx)
    with open(outGff, "w") as out:
        out.write(header[0])
        out.write("# lifted with mirBaseLift.py and %s\n" % chain.split("/")[-1])
        out.writelines(header[1:])
        for i, c in enumerate(feats):
            if i in keep:
                chrom, start, end, strand = lifted[i]
                out.write("\t".join([chrom] + c[1:3] + [str(start + 1), str(end)] + c[5:6] +
                                    [strand] + c[7:]) + "\n")
    print("%d precursor loci, %d lifted, %d left out" % (len(pres), len(pres) - len(dropped),
          len(dropped)), file=sys.stderr)
    for pid in sorted(dropped):
        print("left out: %s %s, %s" % (pid, dropped[pid][0], dropped[pid][1]), file=sys.stderr)

if __name__ == "__main__":
    if len(sys.argv) != 4:
        sys.exit(__doc__)
    main(*sys.argv[1:])

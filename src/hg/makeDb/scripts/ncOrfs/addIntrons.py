#!/usr/bin/env python3
"""
Inject exon/intron block structure into utrAnnotUorfs (bed9+1) using a
same-strand host transcript as the donor.

Primary donor pool is MANE Select / MANE Plus Clinical (--mane). An
optional fallback pool (--fallback, e.g. a full GENCODE bigBed) is consulted
only when every MANE candidate is rejected, so MANE keeps priority whenever
it works.

For each uORF (chrom, chromStart, chromEnd, strand) the donor's coordinates
need only overlap the uORF range, not contain it. The host transcript's
exons are clipped to the uORF range; any host intron inside the overlap is
preserved as an intron of the output bed12. A uORF that extends past either
end of the host transcript keeps the host introns inside the overlap and
gets a single bridging block for the orphan portion. A uORF endpoint that
falls inside a host intron disqualifies that candidate (we will not invent
exon structure for intronic bases). If every MANE candidate is disqualified
the fallback pool is tried next; if every fallback candidate is also
disqualified the uORF stays single-block.

When multiple candidates qualify within the chosen pool, pick the one whose
exons split the uORF range into the most blocks; ties broken by transcript
name for determinism.

Output is bed12 + uorfType + intronsSource (14 fields). intronsSource holds
the chosen donor transcript ID (e.g. ENST00000342878.3), or 'none' if no
host was found in either pool or the chosen host had no introns inside the
uORF range.
"""

import argparse
import subprocess
import sys
from collections import defaultdict


def load_donors(bb_path, tag):
    """Return dict {(chrom, strand): [(tx_start, tx_end, name, blocks)]}
    sorted by tx_start within each group. Works for any bigBed/bigGenePred
    whose first 12 fields are a standard bed12."""
    p = subprocess.Popen(["bigBedToBed", bb_path, "stdout"],
                         stdout=subprocess.PIPE, text=True)
    by_key = defaultdict(list)
    n = 0
    for line in p.stdout:
        fields = line.rstrip("\n").split("\t")
        if len(fields) < 12:
            continue
        chrom = fields[0]
        tx_start = int(fields[1])
        tx_end = int(fields[2])
        name = fields[3]
        strand = fields[5]
        sizes = [int(x) for x in fields[10].rstrip(",").split(",") if x]
        starts = [int(x) for x in fields[11].rstrip(",").split(",") if x]
        blocks = [(tx_start + s, tx_start + s + sz)
                  for s, sz in zip(starts, sizes)]
        by_key[(chrom, strand)].append((tx_start, tx_end, name, blocks))
        n += 1
    rc = p.wait()
    if rc != 0:
        sys.exit(f"[addIntrons] bigBedToBed failed (exit {rc}) for {bb_path}")
    for k in by_key:
        by_key[k].sort(key=lambda t: (t[0], t[1], t[2]))
    sys.stderr.write(f"[addIntrons] loaded {n} {tag} transcripts from {bb_path}\n")
    return by_key


def find_overlapping(by_key, chrom, strand, u_s, u_e):
    """Return list of (tx_start, tx_end, name, blocks) where the transcript
    range overlaps [u_s, u_e] on the given strand."""
    out = []
    for entry in by_key.get((chrom, strand), []):
        tx_s, tx_e, _, _ = entry
        if tx_s >= u_e:
            break  # sorted by start; further entries start at/after u_e
        if tx_e > u_s:
            out.append(entry)
    return out


def project(tx_s, tx_e, blocks, u_s, u_e):
    """Project a uORF onto a MANE transcript's exon blocks.

    Returns (out_blocks, reason). out_blocks is a list of (cs, ce) pairs
    spanning [u_s, u_e], or None if the projection is rejected. reason is
    one of: 'ok', 'no_overlap_exons', 'start_in_intron', 'end_in_intron'.

    If the uORF extends past tx_s or tx_e the orphan portion is filled in
    with a single bridging block at that end; that block is merged into the
    neighbouring clipped MANE exon when they are contiguous, so a uORF
    that runs past MANE's last exon simply gets a longer terminal block.
    """
    proj = []
    for (bs, be) in blocks:
        if be <= u_s or bs >= u_e:
            continue
        cs = max(bs, u_s)
        ce = min(be, u_e)
        if ce > cs:
            proj.append((cs, ce))

    if not proj:
        return None, "no_overlap_exons"

    # Handle the uORF start. If the first clipped block starts after u_s
    # there is either (a) an "orphan" stretch before MANE (u_s < tx_s) which
    # we fill with a bridging block, or (b) u_s sits inside a MANE intron
    # which we cannot bridge without faking exonic bases — reject.
    first_cs = proj[0][0]
    if first_cs > u_s:
        if u_s < tx_s:
            proj.insert(0, (u_s, first_cs))
        else:
            return None, "start_in_intron"

    # Symmetric handling for the uORF end.
    last_ce = proj[-1][1]
    if last_ce < u_e:
        if u_e > tx_e:
            proj.append((last_ce, u_e))
        else:
            return None, "end_in_intron"

    # Merge any contiguous blocks created by the bridging steps so the
    # output has no zero-length introns.
    merged = [proj[0]]
    for (cs, ce) in proj[1:]:
        if cs == merged[-1][1]:
            merged[-1] = (merged[-1][0], ce)
        else:
            merged.append((cs, ce))
    return merged, "ok"


def best_projection(candidates, u_s, u_e):
    """Project a uORF against a list of candidate donor transcripts.

    Returns (best_blocks, best_name, saw_endpoint_in_intron).
    best_blocks is None if no candidate yielded a valid projection.
    """
    best_blocks = None
    best_name = ""
    saw_endpoint_in_intron = False
    for (tx_s, tx_e, tx_name, tx_blocks) in candidates:
        proj, reason = project(tx_s, tx_e, tx_blocks, u_s, u_e)
        if proj is None:
            if reason in ("start_in_intron", "end_in_intron"):
                saw_endpoint_in_intron = True
            continue
        if best_blocks is None:
            best_blocks = proj
            best_name = tx_name
            continue
        # Prefer more output blocks (= more host introns inside the uORF
        # range). Tie-break by transcript name for determinism.
        if (len(proj) > len(best_blocks)
                or (len(proj) == len(best_blocks) and tx_name < best_name)):
            best_blocks = proj
            best_name = tx_name
    return best_blocks, best_name, saw_endpoint_in_intron


def main():
    ap = argparse.ArgumentParser(description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--in", dest="inBed", required=True,
                    help="input utrAnnotUorfs BED (bed9+1)")
    ap.add_argument("--out", dest="outBed", required=True,
                    help="output BED12 + uorfType + intronsSource")
    ap.add_argument("--mane", required=True,
                    help="MANE bigBed (e.g. /gbdb/hg38/mane/mane.bb)")
    ap.add_argument("--fallback",
                    help="optional fallback bigBed (e.g. a full GENCODE bb) "
                         "consulted only when every MANE candidate is "
                         "rejected")
    ap.add_argument("--report", help="optional path to write a summary TSV")
    args = ap.parse_args()

    mane_by_key = load_donors(args.mane, "MANE")
    fb_by_key = load_donors(args.fallback, "fallback") if args.fallback else {}

    n_mane_introns = 0        # picked a MANE donor, projection has >=2 blocks
    n_mane_noIntron = 0       # picked a MANE donor, projection has 1 block
    n_fb_introns = 0          # MANE rejected, fallback gave >=2 blocks
    n_fb_noIntron = 0         # MANE rejected, fallback gave 1 block
    n_endpoint_in_intron = 0  # both pools rejected with an endpoint reason
    n_unmatched = 0           # no candidate in either pool overlaps the uORF
    by_tx_mane = defaultdict(int)
    by_tx_fb = defaultdict(int)

    with open(args.inBed) as fh, open(args.outBed, "w") as out:
        for line in fh:
            fields = line.rstrip("\n").split("\t")
            if len(fields) < 10:
                continue
            chrom = fields[0]
            u_s = int(fields[1])
            u_e = int(fields[2])
            name = fields[3]
            score = fields[4]
            strand = fields[5]
            thick_start = fields[6]
            thick_end = fields[7]
            item_rgb = fields[8]
            uorf_type = fields[9]

            mane_cands = find_overlapping(mane_by_key, chrom, strand, u_s, u_e)
            best_proj, best_name, mane_endpoint = best_projection(
                mane_cands, u_s, u_e)
            used_pool = "mane"
            fb_endpoint = False

            if best_proj is None and fb_by_key:
                fb_cands = find_overlapping(fb_by_key, chrom, strand, u_s, u_e)
                best_proj, best_name, fb_endpoint = best_projection(
                    fb_cands, u_s, u_e)
                used_pool = "fallback"
            else:
                fb_cands = None  # not consulted

            if best_proj is None:
                size = u_e - u_s
                block_count = 1
                block_sizes = f"{size},"
                chrom_starts = "0,"
                sources = "none"
                if not mane_cands and not fb_by_key:
                    n_unmatched += 1
                elif not mane_cands and fb_by_key and not fb_cands:
                    n_unmatched += 1
                elif mane_endpoint or fb_endpoint:
                    n_endpoint_in_intron += 1
                else:
                    # candidates existed but all collapsed to nothing
                    n_unmatched += 1
            elif len(best_proj) <= 1:
                size = u_e - u_s
                block_count = 1
                block_sizes = f"{size},"
                chrom_starts = "0,"
                sources = "none"
                if used_pool == "mane":
                    n_mane_noIntron += 1
                else:
                    n_fb_noIntron += 1
            else:
                block_count = len(best_proj)
                block_sizes = ",".join(str(ce - cs) for (cs, ce) in best_proj) + ","
                chrom_starts = ",".join(str(cs - u_s) for (cs, ce) in best_proj) + ","
                sources = best_name
                if used_pool == "mane":
                    n_mane_introns += 1
                    by_tx_mane[best_name] += 1
                else:
                    n_fb_introns += 1
                    by_tx_fb[best_name] += 1

            bed12 = [chrom, str(u_s), str(u_e), name, score, strand,
                     thick_start, thick_end, item_rgb,
                     str(block_count), block_sizes, chrom_starts]
            out.write("\t".join(bed12 + [uorf_type, sources]) + "\n")

    sys.stderr.write(
        f"[addIntrons] mane_introns={n_mane_introns} "
        f"mane_noIntron={n_mane_noIntron} "
        f"fallback_introns={n_fb_introns} "
        f"fallback_noIntron={n_fb_noIntron} "
        f"endpoint_in_intron={n_endpoint_in_intron} "
        f"unmatched={n_unmatched}\n")
    sys.stderr.write(
        f"[addIntrons] distinct donors used: "
        f"mane={len(by_tx_mane)} fallback={len(by_tx_fb)}\n")

    if args.report:
        with open(args.report, "w") as r:
            r.write(f"mane_introns\t{n_mane_introns}\n")
            r.write(f"mane_noIntron\t{n_mane_noIntron}\n")
            r.write(f"fallback_introns\t{n_fb_introns}\n")
            r.write(f"fallback_noIntron\t{n_fb_noIntron}\n")
            r.write(f"endpoint_in_intron\t{n_endpoint_in_intron}\n")
            r.write(f"unmatched\t{n_unmatched}\n")
            r.write(f"distinct_mane_donors\t{len(by_tx_mane)}\n")
            r.write(f"distinct_fallback_donors\t{len(by_tx_fb)}\n")
            for tx, cnt in sorted(by_tx_mane.items(), key=lambda x: -x[1]):
                r.write(f"mane_donor_{tx}\t{cnt}\n")
            for tx, cnt in sorted(by_tx_fb.items(), key=lambda x: -x[1]):
                r.write(f"fallback_donor_{tx}\t{cnt}\n")


if __name__ == "__main__":
    main()

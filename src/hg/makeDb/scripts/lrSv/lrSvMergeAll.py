#!/usr/bin/env python3
"""
Merge all lrSv subtrack bigBeds into a single combined bigBed (lrSvAll).

Variants are merged on EXACT key (chrom, chromStart, chromEnd, svType, svLen,
insLen). For each merged variant we record which databases contained it and
the per-database AC (or "unknown" / sample count for placeholder datasets).
The Kim PD Brain dataset is split into affected (PD+ILBD) and healthy (HC)
allele counts.

Usage:
  python3 lrSvMergeAll.py
  python3 lrSvMergeAll.py --region chr22       # quick test on one chromosome
"""

import argparse
import os
import re
import subprocess
import sys
from collections import OrderedDict, defaultdict
from multiprocessing import Pool

SCRIPTS_DIR = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, SCRIPTS_DIR)
from lrSvCommon import svColor

ALL_CHROMOSOMES = [f"chr{i}" for i in range(1, 23)] + ["chrX", "chrY", "chrM"]

# ---------------------------------------------------------------------------
# Config + autoSql parsing
# ---------------------------------------------------------------------------

def sanitize_id(s):
    """Convert an arbitrary key to a valid autoSql / C identifier.
    Non-alphanumeric chars become underscores; leading digit gets a 'd' prefix.
    """
    out = re.sub(r'[^A-Za-z0-9_]', '_', s)
    if out and out[0].isdigit():
        out = 'd' + out
    return out


def load_config(path):
    """Parse databases.tsv. Returns ordered list of dicts.
    Each entry gets:
      key      - original key (used in `sources` field and as filter value)
      idSafe   - sanitized version usable as autoSql field-name prefix
    """
    dbs = []
    with open(path) as f:
        for line in f:
            line = line.rstrip("\n")
            if not line or line.startswith("#"):
                continue
            cols = line.split("\t") + [""] * 8
            key = cols[0]
            dbs.append({
                "key":        key,
                "idSafe":     sanitize_id(key),
                "label":      cols[1],
                "bbPath":     cols[2],
                "valueField": cols[3],
                "valueLabel": cols[4] or "AC",
                "affField":   cols[5],
                "healField":  cols[6],
                "afField":    cols[7],
            })
    return dbs


def parse_autosql_fields(bb_path):
    """Return ordered list of field names from a bigBed's embedded autoSql."""
    out = subprocess.run(["bigBedInfo", "-as", bb_path],
                         capture_output=True, text=True, check=True).stdout
    fields = []
    in_block = False
    for ln in out.splitlines():
        s = ln.strip()
        if s == "(":
            in_block = True
            continue
        if s == ")":
            break
        if not in_block or not s:
            continue
        # Lines look like: 'string chrom;       "Chromosome"'
        # Skip comments and empty.
        if s.startswith("("):
            continue
        # Drop the comment after the semicolon
        before_semi = s.split(";", 1)[0].strip()
        # before_semi = 'string chrom' or 'char[1] strand'
        toks = before_semi.split()
        if len(toks) < 2:
            continue
        fname = toks[-1]
        # strip array notation if any: e.g. "name[3]" -> "name"
        if "[" in fname:
            fname = fname.split("[", 1)[0]
        fields.append(fname)
    return fields


# ---------------------------------------------------------------------------
# Phase 1: extract per-db, per-chrom records
# ---------------------------------------------------------------------------

def eval_expr(expr, row, names):
    """Evaluate 'a' or 'a+b' (only +) over named row values; ints, "" if missing."""
    if not expr:
        return ""
    total = 0
    any_val = False
    for term in expr.split("+"):
        term = term.strip()
        if term not in names:
            return ""
        v = row[names[term]]
        if v in ("", ".", "-1"):
            continue
        try:
            total += int(v)
            any_val = True
        except ValueError:
            return ""
    return str(total) if any_val else ""


def extract_one(args):
    """Run bigBedToBed on one subtrack, write per-chrom TSVs.
    Output TSV columns: start \t end \t svType \t svLen \t insLen \t value \t af
    Where:
      - value: for SPLIT dbs, "<aff>|<heal>"; otherwise the per-db value string
      - af:    max AF across configured AF fields, or "" if none
    """
    db, region, extract_dir = args
    bb = db["bbPath"]
    if not os.path.exists(bb):
        print(f"  {db['key']}: missing {bb}", file=sys.stderr)
        return db["key"], 0

    fields = parse_autosql_fields(bb)
    name_to_idx = {fn: i for i, fn in enumerate(fields)}

    required = ["chrom", "chromStart", "chromEnd", "svType", "svLen"]
    for r in required:
        if r not in name_to_idx:
            print(f"  {db['key']}: missing required field {r} in autoSql, "
                  f"skipping", file=sys.stderr)
            return db["key"], 0
    has_inslen = "insLen" in name_to_idx

    af_fields = [f for f in db["afField"].split(",") if f]

    cmd = ["bigBedToBed"]
    if region:
        cmd.extend(["-chrom=" + region.split(":")[0]])
    cmd.extend([bb, "/dev/stdout"])

    out_dir = os.path.join(extract_dir, db["key"])
    os.makedirs(out_dir, exist_ok=True)
    chrom_files = {}
    n = 0

    proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, text=True)
    try:
        for line in proc.stdout:
            row = line.rstrip("\n").split("\t")
            if len(row) < len(fields):
                # Some bigBed rows may have trailing empty fields stripped
                row += [""] * (len(fields) - len(row))

            chrom = row[name_to_idx["chrom"]]
            start = row[name_to_idx["chromStart"]]
            end   = row[name_to_idx["chromEnd"]]
            svType = row[name_to_idx["svType"]]
            svLen = row[name_to_idx["svLen"]]
            insLen = row[name_to_idx["insLen"]] if has_inslen else "0"
            if not svLen or svLen in (".", ""):
                svLen = "0"
            if not insLen or insLen in (".", ""):
                insLen = "0"

            # Per-db value
            vf = db["valueField"]
            if vf == "UNKNOWN":
                value = "unknown"
            elif vf == "SPLIT":
                aff = eval_expr(db["affField"], row, name_to_idx)
                heal = eval_expr(db["healField"], row, name_to_idx)
                value = f"{aff}|{heal}"
            else:
                idx = name_to_idx.get(vf)
                if idx is None:
                    value = ""
                else:
                    v = row[idx]
                    value = "" if v in ("", ".", "-1") else v

            # Max AF across configured AF fields
            max_af = ""
            for fn in af_fields:
                idx = name_to_idx.get(fn)
                if idx is None:
                    continue
                v = row[idx]
                if v in ("", ".", "-1"):
                    continue
                try:
                    f = float(v)
                except ValueError:
                    continue
                if f < 0:
                    continue
                if max_af == "" or f > float(max_af):
                    max_af = f"{f:.6g}"

            if chrom not in chrom_files:
                chrom_files[chrom] = open(os.path.join(out_dir, f"{chrom}.tsv"), "w")
            chrom_files[chrom].write(
                f"{start}\t{end}\t{svType}\t{svLen}\t{insLen}\t{value}\t{max_af}\n"
            )
            n += 1
    finally:
        for fh in chrom_files.values():
            fh.close()
        proc.wait()

    print(f"  {db['key']}: {n:,} variants extracted", file=sys.stderr)
    return db["key"], n


# ---------------------------------------------------------------------------
# Phase 2: per-chromosome merge
# ---------------------------------------------------------------------------

def _add_int(a, b):
    """Add two AC values represented as strings; "" if neither numeric."""
    ai = int(a) if a and a.lstrip("-").isdigit() else None
    bi = int(b) if b and b.lstrip("-").isdigit() else None
    if ai is None and bi is None:
        return a or b
    if ai is None:
        return str(bi)
    if bi is None:
        return str(ai)
    return str(ai + bi)


def _max_str_float(a, b):
    """Max of two AF values represented as strings; "" if neither numeric."""
    if not a:
        return b
    if not b:
        return a
    try:
        m = max(float(a), float(b))
        return f"{m:.6g}"
    except ValueError:
        return a


def _combine(db, prev, new):
    """Merge two within-db hits at the same variant key.
    For numeric AC we sum; for SPLIT we sum each component; for UNKNOWN
    or sampleCount we keep the existing value (sample count is per-site).
    AF takes the max."""
    pval, paf, pins = prev
    nval, naf, nins = new
    vf = db["valueField"]
    af = _max_str_float(paf, naf)
    if vf == "SPLIT":
        pa, ph = pval.split("|", 1)
        na, nh = nval.split("|", 1)
        return (f"{_add_int(pa, na)}|{_add_int(ph, nh)}", af, pins)
    if vf == "UNKNOWN":
        return ("unknown", af, pins)
    if vf == "sampleCount":
        # Take max - same site shouldn't have varying sample counts but be safe
        try:
            v = max(int(pval) if pval.isdigit() else 0,
                    int(nval) if nval.isdigit() else 0)
            return (str(v), af, pins)
        except ValueError:
            return (pval or nval, af, pins)
    # Numeric AC: sum
    return (_add_int(pval, nval), af, pins)


def merge_chrom(args):
    chrom, dbs, extract_dir, out_dir = args
    # variant_key (start, end, svType, svLen, insLen) -> {db_key: (value, af, insLen)}
    variants = defaultdict(dict)
    # (key, db_key) -> set of (value, af) already incorporated, so we only sum
    # AC across genuinely distinct records and never across byte-identical
    # duplicate rows (which would inflate the per-database and total AC).
    seen_combo = defaultdict(set)
    n_in = 0
    n_dup = 0

    db_by_key = {db["key"]: db for db in dbs}

    for db in dbs:
        path = os.path.join(extract_dir, db["key"], f"{chrom}.tsv")
        if not os.path.exists(path):
            continue
        with open(path) as f:
            for line in f:
                parts = line.rstrip("\n").split("\t")
                if len(parts) < 7:
                    continue
                start, end, svType, svLen, insLen, value, af = parts
                # Use insLen=0 for non-INS to avoid spurious key splits
                if svType not in ("INS",):
                    insLen_key = "0"
                else:
                    insLen_key = insLen
                try:
                    key = (int(start), int(end), svType,
                           int(svLen), int(insLen_key))
                except ValueError:
                    continue
                n_in += 1
                combo = (value, af)
                if db["key"] in variants[key]:
                    if combo in seen_combo[(key, db["key"])]:
                        # identical record already counted for this db; skip
                        n_dup += 1
                        continue
                    variants[key][db["key"]] = _combine(
                        db, variants[key][db["key"]], (value, af, insLen))
                else:
                    variants[key][db["key"]] = (value, af, insLen)
                seen_combo[(key, db["key"])].add(combo)

    out_path = os.path.join(out_dir, f"{chrom}.bed")
    n_out = 0
    n_split_aff = 0  # variants with at least one Kim PD affected AC

    with open(out_path, "w") as out:
        for key in sorted(variants.keys()):
            start, end, svType, svLen, _insLen_key, = key
            per_db = variants[key]

            sources = sorted(per_db.keys())
            source_count = len(sources)

            # Compute totalAC: sum over numeric values from regular dbs
            total_ac = 0
            for db in dbs:
                if db["key"] not in per_db:
                    continue
                vf = db["valueField"]
                value, af, _ins = per_db[db["key"]]
                if vf == "SPLIT":
                    aff, heal = value.split("|", 1)
                    for v in (aff, heal):
                        if v.isdigit():
                            total_ac += int(v)
                elif vf in ("UNKNOWN", "sampleCount"):
                    # Don't sum sampleCount or "unknown" placeholders into AC
                    pass
                else:
                    if value and value.lstrip("-").isdigit():
                        try:
                            iv = int(value)
                            if iv > 0:
                                total_ac += iv
                        except ValueError:
                            pass

            # Compute min/max AF across DBs that contributed an AF value
            max_af = 0.0
            min_af = None
            for db_key, (_v, af, _i) in per_db.items():
                if not af:
                    continue
                try:
                    f_af = float(af)
                except ValueError:
                    continue
                if f_af > max_af:
                    max_af = f_af
                if f_af > 0 and (min_af is None or f_af < min_af):
                    min_af = f_af

            score = min(1000, int(max_af * 1000)) if max_af > 0 else 0

            # Use the original insLen from any contributing INS record
            ins_len_out = "0"
            for db in dbs:
                if db["key"] in per_db:
                    _v, _af, ins = per_db[db["key"]]
                    ins_len_out = ins
                    break

            short = svType[:6]
            name = f"{short}-{svLen}:{source_count}"
            color = svColor(svType)

            fields = [
                chrom, str(start), str(end), name, str(score), ".",
                str(start), str(end), color,
                svType, str(svLen), ins_len_out, svType,
                f"{max_af:.6g}" if max_af > 0 else "0",
                f"{min_af:.6g}" if min_af is not None else "0",
                str(total_ac),
                str(source_count),
                ",".join(sources),
            ]

            # Per-database value columns (SPLIT contributes 2 columns)
            for db in dbs:
                if db["valueField"] == "SPLIT":
                    if db["key"] in per_db:
                        value, _af, _i = per_db[db["key"]]
                        aff, heal = value.split("|", 1)
                        if aff:
                            n_split_aff += 1
                    else:
                        aff, heal = "", ""
                    fields.extend([aff, heal])
                else:
                    value = per_db.get(db["key"], ("", "", ""))[0]
                    fields.append(value)

            out.write("\t".join(fields) + "\n")
            n_out += 1

    print(f"  {chrom}: {n_in:,} input rows -> {n_out:,} unique variants "
          f"({n_in - n_out:,} merged; {n_dup:,} identical dup rows skipped)",
          file=sys.stderr)
    return out_path, n_out


# ---------------------------------------------------------------------------
# AutoSql + trackDb fragment generation
# ---------------------------------------------------------------------------

def write_autosql(out_path, dbs):
    with open(out_path, "w") as f:
        f.write('table lrSvAll\n')
        f.write('"Combined long-read structural variants from all lrSv subtracks"\n')
        f.write('(\n')
        # BED9
        f.write('    string  chrom;          "Chromosome"\n')
        f.write('    uint    chromStart;     "Start position (0-based)"\n')
        f.write('    uint    chromEnd;       "End position"\n')
        f.write('    string  name;           "Variant ID (TYPE-svLen:sourceCount)"\n')
        f.write('    uint    score;          "Score (maxAF * 1000)"\n')
        f.write('    char[1] strand;         "Strand"\n')
        f.write('    uint    thickStart;     "Thick start"\n')
        f.write('    uint    thickEnd;       "Thick end"\n')
        f.write('    uint    itemRgb;        "Color by SV type"\n')
        # SV info
        f.write('    string  svType;         "SV Type|DEL/INS/DUP/INV/CPX/etc."\n')
        f.write('    int     svLen;          "SV Length|Length on the reference in bp"\n')
        f.write('    int     insLen;         "Insertion Length|Length of inserted sequence (0 for DEL/INV/DUP)"\n')
        f.write('    string  varType;        "Variant Type|Alias of svType"\n')
        # Aggregate fields
        f.write('    float   maxAF;          "Max Allele Frequency|Maximum alleleFreq across contributing databases"\n')
        f.write('    float   minAF;          "Min Allele Frequency|Minimum non-zero alleleFreq across contributing databases"\n')
        f.write('    int     AC;             "AC|Sum of allele counts across contributing databases"\n')
        f.write('    int     sourceCount;    "Source Count|Number of databases reporting this variant"\n')
        f.write('    string  sources;        "Source Databases|Comma-separated keys of databases reporting this variant"\n')
        # Per-database value fields. Field names use the sanitized id; the
        # description still shows the original label, including any commas.
        for db in dbs:
            sid = db["idSafe"]
            if db["valueField"] == "SPLIT":
                f.write(f'    string  {sid}AC_affected;'
                        f'  "{db["label"]} AC affected|Allele count in affected samples"\n')
                f.write(f'    string  {sid}AC_healthy;'
                        f'   "{db["label"]} AC healthy|Allele count in healthy samples"\n')
            elif db["valueField"] == "sampleCount":
                f.write(f'    string  {sid}Samples;'
                        f'      "{db["label"]} Samples|Number of samples carrying this variant"\n')
            else:
                # AC or UNKNOWN
                f.write(f'    string  {sid}AC;'
                        f'           "{db["label"]} AC|Allele count (or \'unknown\' for site-only datasets)"\n')
        f.write(')\n')


def write_trackdb_stanza(out_path, dbs):
    """Auto-generated full trackDb stanza for the lrSvAll track.

    Written directly into the trackDb dir and pulled in via
    `include lrSvAll.ra` from lrSv.ra.
    """
    # Commas are the separator in filterValues, so strip them from labels.
    # The original label (with commas) still appears on the detail page via
    # the autoSql field descriptions.
    src_parts = [f"{db['key']}|{db['label'].replace(',', '')}" for db in dbs]
    with open(out_path, "w") as f:
        f.write("# AUTO-GENERATED by ~/kent/src/hg/makeDb/scripts/lrSv/lrSvMergeAll.py\n")
        f.write("# Do not edit by hand - re-run the merge script and re-commit.\n\n")
        f.write("    track lrSvAll\n")
        f.write("    parent longReadVariants
")
        f.write("    bigDataUrl /gbdb/$D/lrSv/lrSvAll.bb\n")
        f.write("    shortLabel All LR SVs merged\n")
        f.write("    longLabel All long-read SVs merged across subtracks "
                "by exact position, with per-database AC\n")
        f.write("    type bigBed 9 +\n")
        f.write("    itemRgb on\n")
        f.write("    visibility pack\n")
        f.write("    mouseOver <b>Var</b>: $name ($svType)<br><b>SV len</b>: $svLen"
                "<br><b>Ins len</b>: $insLen<br><b>Sources</b>: $sources"
                "<br><b>AF range</b>: $minAF-$maxAF<br><b>AC</b>: $AC\n")
        # Source filter
        f.write("    filterValues.sources " + ",".join(src_parts) + "\n")
        f.write("    filterType.sources multipleListOr\n")
        f.write("    filterLabel.sources Source Database\n")
        # SV type
        f.write("    filterValues.svType DEL,INS,DUP,INV,CPX,MIXED,INSDEL,"
                "TRA\n")
        f.write("    filterType.svType multipleListOr\n")
        f.write("    filterLabel.svType SV Type\n")
        # Range filters
        f.write("    filter.svLen 0:30000000\n")
        f.write("    filterByRange.svLen on\n")
        f.write("    filterLabel.svLen SV Length (bp)\n")
        f.write("    filter.insLen 0:600000\n")
        f.write("    filterByRange.insLen on\n")
        f.write("    filterLabel.insLen Insertion Length (bp)\n")
        f.write("    filter.maxAF 0:1\n")
        f.write("    filterByRange.maxAF on\n")
        f.write("    filterLimits.maxAF 0:1\n")
        f.write("    filterLabel.maxAF Max Allele Frequency (across DBs)\n")
        f.write("    filter.minAF 0:1\n")
        f.write("    filterByRange.minAF on\n")
        f.write("    filterLimits.minAF 0:1\n")
        f.write("    filterLabel.minAF Min Allele Frequency (across DBs)\n")
        f.write("    filter.AC 0:30000\n")
        f.write("    filterByRange.AC on\n")
        f.write("    filterLabel.AC Total AC (across DBs)\n")
        f.write(f"    filter.sourceCount 1:{len(dbs)}\n")
        f.write("    filterByRange.sourceCount on\n")
        f.write("    filterLabel.sourceCount Number of Source Databases\n")
        f.write("    skipEmptyFields on\n")
        f.write("    priority 0\n")


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--config", default=os.path.join(SCRIPTS_DIR, "databases.tsv"))
    parser.add_argument("--work-dir", default="/hive/data/genomes/hg38/bed/lrSv/all")
    parser.add_argument("--chrom-sizes", default="/hive/data/genomes/hg38/chrom.sizes")
    parser.add_argument("--output-prefix", default="lrSvAll")
    parser.add_argument("--region", default=None)
    parser.add_argument("--threads", type=int, default=8)
    parser.add_argument("--keep-temp", action="store_true")
    parser.add_argument("--trackdb-ra",
                        default="~/kent/src/hg/makeDb/trackDb/human/lrSvAll.ra",
                        help="Path to write the trackDb stanza file. Default "
                        "writes directly into the kent source tree so it is "
                        "picked up by `include lrSvAll.ra`.")
    args = parser.parse_args()

    os.makedirs(args.work_dir, exist_ok=True)

    dbs = load_config(args.config)
    print(f"Loaded {len(dbs)} databases:", file=sys.stderr)
    for db in dbs:
        ok = "OK" if os.path.exists(db["bbPath"]) else "MISSING"
        print(f"  {db['key']}: {db['label']}  [{ok}] {db['bbPath']}",
              file=sys.stderr)

    # Output paths
    as_path = os.path.join(args.work_dir, f"{args.output_prefix}.as")
    bb_path = os.path.join(args.work_dir, f"{args.output_prefix}.bb")
    bed_path = os.path.join(args.work_dir, f"{args.output_prefix}.bed")
    extract_dir = os.path.join(args.work_dir, "extracted")
    bed_dir = os.path.join(args.work_dir, "beds")
    os.makedirs(extract_dir, exist_ok=True)
    os.makedirs(bed_dir, exist_ok=True)

    # The trackDb stanza is written directly into the kent source tree so it
    # can be `include`d from human/lrSv.ra without manual copy-paste.
    ra_path = os.path.expanduser(args.trackdb_ra) if args.trackdb_ra else \
        os.path.join(args.work_dir, f"{args.output_prefix}.ra")

    write_autosql(as_path, dbs)
    write_trackdb_stanza(ra_path, dbs)
    print(f"Wrote {as_path}", file=sys.stderr)
    print(f"Wrote {ra_path}", file=sys.stderr)

    # Phase 1: extract
    print(f"\n=== Phase 1: Extracting ({args.threads} parallel) ===",
          file=sys.stderr)
    tasks = [(db, args.region, extract_dir) for db in dbs]
    with Pool(min(args.threads, len(tasks))) as pool:
        ext_results = pool.map(extract_one, tasks)
    total_in = sum(n for _, n in ext_results)
    print(f"Phase 1 done: {total_in:,} input rows", file=sys.stderr)

    # Phase 2: merge per chromosome
    chroms = [args.region.split(":")[0]] if args.region else ALL_CHROMOSOMES
    print(f"\n=== Phase 2: Merging {len(chroms)} chromosomes "
          f"({args.threads} parallel) ===", file=sys.stderr)
    tasks = [(c, dbs, extract_dir, bed_dir) for c in chroms]
    with Pool(min(args.threads, len(chroms))) as pool:
        results = pool.map(merge_chrom, tasks)

    total_out = sum(n for _, n in results)
    print(f"Phase 2 done: {total_out:,} merged rows", file=sys.stderr)

    # Concatenate + sort
    print(f"\n=== Concatenating + sorting ===", file=sys.stderr)
    chrom_beds = [p for p, _ in results if os.path.exists(p)]
    with open(bed_path, "w") as out:
        for cb in chrom_beds:
            with open(cb) as f:
                # already sorted by start within chrom from sorted(variants.keys())
                out.write(f.read())

    # bedToBigBed
    print(f"\n=== Running bedToBigBed ===", file=sys.stderr)
    cmd = ["bedToBigBed", "-type=bed9+", f"-as={as_path}", "-tab",
           bed_path, args.chrom_sizes, bb_path]
    print("  " + " ".join(cmd), file=sys.stderr)
    subprocess.run(cmd, check=True)

    bb_size_mb = os.path.getsize(bb_path) / (1024 ** 2)
    print(f"\nDone. {bb_path} ({bb_size_mb:.1f} MB)", file=sys.stderr)
    print(f"Input variants:  {total_in:,}", file=sys.stderr)
    print(f"Output variants: {total_out:,}", file=sys.stderr)
    if total_in > 0:
        print(f"Dedup rate:      {(1 - total_out/total_in)*100:.1f}%",
              file=sys.stderr)

    if not args.keep_temp:
        # Keep extract_dir + bed_dir for now in case we want to re-merge with
        # different rules. Caller can rm -rf if needed.
        pass


if __name__ == "__main__":
    main()

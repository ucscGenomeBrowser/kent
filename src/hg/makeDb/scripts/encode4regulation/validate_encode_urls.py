#!/usr/bin/env python3
"""
Validate local ENCODE4 regulation files against the ENCODE portal API.

For each ENCFF file used in the wgEncodeReg4 composites:
1. Query the ENCODE REST API for file metadata
2. Extract the S3 public URL, md5sum, and file_size
3. Compare md5sum against local file
4. Output a TSV mapping local paths to S3 URLs with validation status

Usage:
    python3 validate_encode_urls.py [--sample N] [--threads N]

Output files (in working directory):
    encode4_url_mapping.tsv    - Full mapping with validation results
    encode4_validation.log     - Detailed log of any mismatches or errors
"""

import argparse
import hashlib
import json
import os
import sys
import time
import urllib.request
from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path

TRACKDB_DIR = "/cluster/home/lrnassar/kent/src/hg/makeDb/trackDb/human/hg38"
COMPOSITE_FILES = [
    "wgEncodeReg4Epigenetics.ra",
    "wgEncodeReg4RnaSeq.ra",
    "wgEncodeReg4TfChip.ra",
]
ENCODE_API = "https://www.encodeproject.org/files/{accession}/?format=json"
LOCAL_DATA = "/hive/data/outside/encode4/ccre/ENCODE_V4_Regulation"

OUTPUT_TSV = "encode4_url_mapping.tsv"
OUTPUT_LOG = "encode4_validation.log"


def parse_encff_from_ra_files():
    """Extract all ENCFF accessions and their gbdb paths from the .ra files."""
    entries = {}  # accession -> gbdb_path
    for ra_file in COMPOSITE_FILES:
        path = os.path.join(TRACKDB_DIR, ra_file)
        with open(path) as f:
            for line in f:
                line = line.strip()
                if line.startswith("bigDataUrl") and "ENCFF" in line:
                    gbdb_path = line.split(None, 1)[1]
                    basename = os.path.basename(gbdb_path)
                    accession = basename.split(".")[0]
                    entries[accession] = gbdb_path
    return entries


def query_encode_api(accession):
    """Query the ENCODE portal for a file's metadata. Returns dict or error string."""
    url = ENCODE_API.format(accession=accession)
    headers = {"Accept": "application/json"}
    req = urllib.request.Request(url, headers=headers)
    try:
        with urllib.request.urlopen(req, timeout=30) as resp:
            data = json.loads(resp.read().decode())
        md5 = data.get("md5sum", "")
        file_size = data.get("file_size", 0)
        status = data.get("status", "")
        # Extract S3 HTTPS URL from cloud_metadata
        s3_url = ""
        cloud = data.get("cloud_metadata", {})
        if isinstance(cloud, dict):
            s3_url = cloud.get("url", "")
        # Sometimes it's nested differently
        if not s3_url:
            for key in ("cloud_metadata",):
                obj = data.get(key, {})
                if isinstance(obj, dict):
                    for sub_key, sub_val in obj.items():
                        if isinstance(sub_val, str) and "s3.amazonaws.com" in sub_val:
                            s3_url = sub_val
                            break
        return {
            "accession": accession,
            "md5sum": md5,
            "file_size": file_size,
            "s3_url": s3_url,
            "status": status,
            "error": None,
        }
    except Exception as e:
        return {
            "accession": accession,
            "md5sum": "",
            "file_size": 0,
            "s3_url": "",
            "status": "",
            "error": str(e),
        }


def md5_file(filepath, chunk_size=8192*1024):
    """Compute md5 of a local file."""
    h = hashlib.md5()
    with open(filepath, "rb") as f:
        while True:
            chunk = f.read(chunk_size)
            if not chunk:
                break
            h.update(chunk)
    return h.hexdigest()


def resolve_local_path(accession):
    """Resolve ENCFF accession to local file path."""
    for ext in (".bigWig", ".bigBed"):
        path = os.path.join(LOCAL_DATA, accession + ext)
        if os.path.exists(path):
            return path
    return None


def validate_one(accession, gbdb_path, do_md5=True):
    """Query API and optionally validate md5 for one file."""
    result = query_encode_api(accession)
    result["gbdb_path"] = gbdb_path

    if result["error"]:
        result["md5_match"] = "API_ERROR"
        result["local_md5"] = ""
        return result

    local_path = resolve_local_path(accession)
    if not local_path:
        result["md5_match"] = "LOCAL_MISSING"
        result["local_md5"] = ""
        return result

    if do_md5:
        local_md5 = md5_file(local_path)
        result["local_md5"] = local_md5
        result["md5_match"] = "MATCH" if local_md5 == result["md5sum"] else "MISMATCH"
    else:
        # Size-only check
        local_size = os.path.getsize(local_path)
        result["local_md5"] = ""
        result["md5_match"] = "SIZE_MATCH" if local_size == result["file_size"] else "SIZE_MISMATCH"

    return result


def main():
    parser = argparse.ArgumentParser(description="Validate ENCODE4 files against portal API")
    parser.add_argument("--sample", type=int, default=0,
                        help="Only process N random files (0 = all)")
    parser.add_argument("--threads", type=int, default=8,
                        help="Number of parallel API query threads (default: 8)")
    parser.add_argument("--skip-md5", action="store_true",
                        help="Skip md5 validation (size-only check)")
    args = parser.parse_args()

    print("Parsing .ra files for ENCFF accessions...")
    entries = parse_encff_from_ra_files()
    print(f"Found {len(entries)} unique ENCFF files")

    accessions = list(entries.keys())
    if args.sample > 0:
        import random
        random.seed(42)
        accessions = random.sample(accessions, min(args.sample, len(accessions)))
        print(f"Sampling {len(accessions)} files")

    results = []
    errors = []
    mismatches = []
    api_errors = []

    do_md5 = not args.skip_md5
    mode = "md5" if do_md5 else "size-only"
    print(f"Querying ENCODE API and validating ({mode})...")
    print(f"Using {args.threads} threads")

    completed = 0
    total = len(accessions)
    start_time = time.time()

    with ThreadPoolExecutor(max_workers=args.threads) as executor:
        futures = {
            executor.submit(validate_one, acc, entries[acc], do_md5): acc
            for acc in accessions
        }
        for future in as_completed(futures):
            result = future.result()
            results.append(result)
            completed += 1

            if result["md5_match"] == "MISMATCH" or result["md5_match"] == "SIZE_MISMATCH":
                mismatches.append(result)
            if result["error"]:
                api_errors.append(result)

            if completed % 100 == 0 or completed == total:
                elapsed = time.time() - start_time
                rate = completed / elapsed if elapsed > 0 else 0
                eta = (total - completed) / rate if rate > 0 else 0
                print(f"  {completed}/{total} ({rate:.1f}/sec, ETA {eta:.0f}s) "
                      f"- {len(api_errors)} errors, {len(mismatches)} mismatches")

    # Sort by accession for stable output
    results.sort(key=lambda r: r["accession"])

    # Write TSV
    with open(OUTPUT_TSV, "w") as f:
        f.write("accession\tgbdb_path\ts3_url\tportal_md5\tlocal_md5\tvalidation\tstatus\terror\n")
        for r in results:
            f.write(f"{r['accession']}\t{r['gbdb_path']}\t{r['s3_url']}\t"
                    f"{r['md5sum']}\t{r.get('local_md5','')}\t{r['md5_match']}\t"
                    f"{r['status']}\t{r.get('error','')}\n")

    # Write log
    with open(OUTPUT_LOG, "w") as f:
        f.write(f"ENCODE4 Regulation URL Validation\n")
        f.write(f"Date: {time.strftime('%Y-%m-%d %H:%M:%S')}\n")
        f.write(f"Total files: {total}\n")
        f.write(f"Validation mode: {mode}\n")
        f.write(f"Elapsed: {time.time() - start_time:.0f}s\n\n")

        matches = sum(1 for r in results if r["md5_match"] in ("MATCH", "SIZE_MATCH"))
        f.write(f"Results:\n")
        f.write(f"  Matches: {matches}\n")
        f.write(f"  Mismatches: {len(mismatches)}\n")
        f.write(f"  API errors: {len(api_errors)}\n")
        missing_url = sum(1 for r in results if not r["s3_url"] and not r["error"])
        f.write(f"  Missing S3 URL: {missing_url}\n\n")

        if mismatches:
            f.write("MISMATCHES:\n")
            for r in mismatches:
                f.write(f"  {r['accession']}: portal={r['md5sum']} local={r.get('local_md5','')} "
                        f"size={r['file_size']}\n")
            f.write("\n")

        if api_errors:
            f.write("API ERRORS:\n")
            for r in api_errors:
                f.write(f"  {r['accession']}: {r['error']}\n")
            f.write("\n")

        if missing_url:
            f.write("MISSING S3 URLs:\n")
            for r in results:
                if not r["s3_url"] and not r["error"]:
                    f.write(f"  {r['accession']}\n")

    # Summary
    print(f"\nDone! Elapsed: {time.time() - start_time:.0f}s")
    matches = sum(1 for r in results if r["md5_match"] in ("MATCH", "SIZE_MATCH"))
    missing_url = sum(1 for r in results if not r["s3_url"] and not r["error"])
    print(f"  Matches:      {matches}/{total}")
    print(f"  Mismatches:   {len(mismatches)}/{total}")
    print(f"  API errors:   {len(api_errors)}/{total}")
    print(f"  Missing URLs: {missing_url}/{total}")
    print(f"\nOutput: {OUTPUT_TSV}")
    print(f"Log:    {OUTPUT_LOG}")


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""Download the GPN-Star sequence-logo and signed LLR bigWigs from Hugging Face into
/hive/data/genomes/<db>/bed/gpnStar/download, pinned to one dataset revision.

The browser never reads the download directory: gpnStarRebuild.sh re-encodes each file from
here into /hive/data/genomes/<db>/bed/gpnStar, so re-running this script cannot overwrite the
files that /gbdb serves.

Each file is downloaded to <file>.part (resumable), its size checked against the server and
its SHA-256 against the Hugging Face LFS oid, and only then moved into place.

usage: gpnStarMirror.py [scoreSet ...]     (default: all score sets)
"""
import fcntl, hashlib, json, os, subprocess, sys, urllib.request

REPO = "songlab/gpn-star-scores"
REV = "47e7f051113abab49f04f43f9107cae2cbbfd34d"
FILES = ["A.bw", "C.bw", "G.bw", "T.bw", "llr_A.bw", "llr_C.bw", "llr_G.bw", "llr_T.bw"]

# Hugging Face score set -> local download directory
SCORESETS = {
    "gpn-star-hg38-v100-200m": "/hive/data/genomes/hg38/bed/gpnStar/download/v100",
    "gpn-star-hg38-m447-200m": "/hive/data/genomes/hg38/bed/gpnStar/download/m447",
    "gpn-star-hg38-p243-200m": "/hive/data/genomes/hg38/bed/gpnStar/download/p243",
    "mm39": "/hive/data/genomes/mm39/bed/gpnStar/download",
    "gg6": "/hive/data/genomes/galGal6/bed/gpnStar/download",
    "dm6": "/hive/data/genomes/dm6/bed/gpnStar/download",
    "ce11": "/hive/data/genomes/ce11/bed/gpnStar/download",
}

def remoteFiles(scoreSet):
    " return dict fileName -> (size, sha256) from the HF tree API "
    url = "https://huggingface.co/api/datasets/%s/tree/%s/bigwig/%s" % (REPO, REV, scoreSet)
    res = {}
    for f in json.load(urllib.request.urlopen(url)):
        lfs = f.get("lfs", {})
        res[f["path"].split("/")[-1]] = (lfs.get("size", f.get("size")), lfs.get("oid"))
    return res

def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as fh:
        for chunk in iter(lambda: fh.read(16 * 1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()

def mirrorOne(scoreSet, outDir, fname, size, oid):
    final = os.path.join(outDir, fname)
    if os.path.exists(final) and os.path.getsize(final) == size:
        print("ok     %s/%s" % (scoreSet, fname), flush=True)
        return
    part = final + ".part"
    # refuse to run if another process holds the lock; curl inherits it, so a leftover
    # curl from a killed run also blocks a restart
    lock = open(part + ".lock", "w")
    try:
        fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
    except BlockingIOError:
        sys.exit("another download is already writing %s" % part)
    if os.path.exists(part) and os.path.getsize(part) > size:
        print("removing oversized %s" % part, flush=True)
        os.remove(part)
    url = ("https://huggingface.co/datasets/%s/resolve/%s/bigwig/%s/%s"
           % (REPO, REV, scoreSet, fname))
    for attempt in range(10):
        if os.path.exists(part) and os.path.getsize(part) >= size:
            break
        subprocess.call(["curl", "-sS", "-L", "--fail", "--retry", "5", "-C", "-", "-o", part, url],
                        pass_fds=(lock.fileno(),))
    if not os.path.exists(part):
        sys.exit("download failed, no data received for %s" % url)
    got = os.path.getsize(part)
    if got != size:
        sys.exit("size mismatch %s: got %d expected %d" % (part, got, size))
    digest = sha256(part)
    if digest != oid:
        os.rename(part, part + ".bad")
        sys.exit("sha256 mismatch %s: got %s expected %s" % (part, digest, oid))
    os.rename(part, final)
    os.remove(part + ".lock")
    print("done   %s/%s %d bytes" % (scoreSet, fname, size), flush=True)

def main():
    sets = sys.argv[1:] or list(SCORESETS)
    unknown = [x for x in sets if x not in SCORESETS]
    if unknown:
        sys.exit("unknown score set(s): %s\nknown: %s" % (" ".join(unknown), " ".join(SCORESETS)))
    for scoreSet in sets:
        outDir = SCORESETS[scoreSet]
        os.makedirs(outDir, exist_ok=True)
        remote = remoteFiles(scoreSet)
        for fname in FILES:
            size, oid = remote[fname]
            mirrorOne(scoreSet, outDir, fname, size, oid)

if __name__ == "__main__":
    main()

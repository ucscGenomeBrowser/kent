#!/usr/bin/env python3
"""
strchiveOtto.py -- keep the STRchive track in sync with STRchive's GitHub releases.

STRchive (https://strchive.org, https://github.com/dashnowlab/STRchive) curates
disease-associated tandem repeat loci and cuts a GitHub release one to two times a
month.  Since v2.26.1 every release ships ready-to-use bigBed files built for us,
named STRchive-disease-loci-<tag>.<assembly>.ucsc.bb, so there is nothing left to
convert: we download the file and repoint the /gbdb symlink at it.  See STRchive
issue #333 for the correspondence that set this up.

Each release lands in its own releases/<tag>/ subdirectory and the /gbdb symlinks
are moved to it, so the live release is visible from `ls -l /gbdb/hg38/strVar/` and
a rollback is just re-running with --release on the older tag.

Like every otto cron this is silent when there is nothing to do: no new release
means no output and therefore no mail.  Anything unexpected is loud and exits
non-zero.
"""

import argparse
import json
import os
import re
import subprocess
import sys
import time
import urllib.error
import urllib.request

# Where the cron runs.  Not derived from __file__: the tree copy of this script must
# never write to the tree, it is the installed copy under /hive that owns the data.
ottoDir = "/hive/data/outside/otto/strchive"

repo = "dashnowlab/STRchive"
apiUrl = "https://api.github.com/repos/%s/releases/latest" % repo

# UCSC db -> the assembly name STRchive uses in its release asset names.  STRchive
# builds all three of these for every release.  trackDb/human/strVar.ra is shared by
# the human assemblies and keys off whether /gbdb/<db>/strVar/strchive.bb exists, so
# adding a db here is all it takes to put the track on another assembly.
dbToAsm = {
    "hg19": "hg19",
    "hg38": "hg38",
    "hs1": "T2T-chm13",
}

# A STRchive release adds or drops a handful of loci at a time, so a swing this
# large means something went wrong upstream rather than a normal curation update.
maxItemCountChange = 0.25

# GitHub occasionally 502s; a weekly cron should not mail over a blip.
httpRetries = 5
httpRetrySecs = 30


def errAbort(msg):
    "print an error and exit non-zero, so cron mails the otto group"
    print("strchive otto: ERROR: %s" % msg, file=sys.stderr)
    sys.exit(1)


def fetchUrl(url, isJson=False):
    "GET a url, retrying transient failures, return bytes (or the parsed json)"
    lastErr = None
    for attempt in range(httpRetries):
        try:
            req = urllib.request.Request(url, headers={"User-Agent": "UCSC-otto-strchive"})
            with urllib.request.urlopen(req, timeout=120) as resp:
                data = resp.read()
            if isJson:
                return json.loads(data.decode("utf8"))
            return data
        except (urllib.error.URLError, OSError, ValueError) as ex:
            lastErr = ex
            if attempt < httpRetries - 1:
                time.sleep(httpRetrySecs)
    errAbort("could not fetch %s after %d attempts: %s" % (url, httpRetries, lastErr))


def getRelease(tag):
    "return the GitHub release json, latest if tag is None"
    if tag:
        url = "https://api.github.com/repos/%s/releases/tags/%s" % (repo, tag)
    else:
        url = apiUrl
    rel = fetchUrl(url, isJson=True)
    if "tag_name" not in rel:
        errAbort("no tag_name in the release json from %s (API changed or rate limited?)" % url)
    return rel


def assetUrl(rel, asm):
    """find the bigBed asset for one assembly.  Matched by suffix rather than built
    from the tag, so a change in how STRchive versions the file name does not
    silently pick up the wrong file."""
    suffix = ".%s.ucsc.bb" % asm
    hits = [a for a in rel.get("assets", []) if a["name"].endswith(suffix)]
    if len(hits) != 1:
        errAbort("expected exactly one *%s asset in release %s, found %d: %s"
                 % (suffix, rel["tag_name"], len(hits),
                    ", ".join(a["name"] for a in rel.get("assets", [])) or "none"))
    return hits[0]["browser_download_url"], hits[0]["name"]


def bigBedItemCount(fname):
    "item count of a bigBed, or None if the file does not exist"
    if not os.path.exists(fname):
        return None
    try:
        info = subprocess.run(["bigBedInfo", fname], check=True,
                              stdout=subprocess.PIPE, universal_newlines=True).stdout
    except (subprocess.CalledProcessError, OSError) as ex:
        errAbort("bigBedInfo failed on %s: %s" % (fname, ex))
    match = re.search(r"^itemCount:\s+([0-9,]+)", info, re.M)
    if not match:
        errAbort("no itemCount in bigBedInfo output for %s -- corrupt download?" % fname)
    return int(match.group(1).replace(",", ""))


def readFile(fname):
    "strip()ed contents of a file, or None if it is not there"
    if not os.path.isfile(fname):
        return None
    with open(fname) as fh:
        return fh.read().strip()


def writeFile(fname, text):
    with open(fname, "w") as fh:
        fh.write(text + "\n")


def relinkGbdb(linkPath, target):
    """point linkPath at target.  Written as symlink-then-rename so hgTracks never
    sees a moment with no link at all."""
    tmpPath = linkPath + ".otto.tmp"
    if os.path.lexists(tmpPath):
        os.remove(tmpPath)
    os.symlink(target, tmpPath)
    os.rename(tmpPath, linkPath)


def liveTarget(linkPath):
    "what a /gbdb symlink currently points at, or None"
    if not os.path.islink(linkPath):
        return None
    return os.readlink(linkPath)


def main():
    parser = argparse.ArgumentParser(description=__doc__,
            formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("-f", "--force", action="store_true",
            help="rebuild even when the release has already been handled")
    parser.add_argument("-r", "--release", metavar="TAG",
            help="use this release tag instead of the latest one, e.g. to roll back")
    parser.add_argument("-n", "--dry-run", action="store_true",
            help="say what would happen, download and check nothing")
    args = parser.parse_args()

    if not os.path.isdir(ottoDir):
        errAbort("%s does not exist -- run 'make install' from the kent tree first" % ottoDir)
    os.chdir(ottoDir)

    rel = getRelease(args.release)
    tag = rel["tag_name"]
    relDate = rel.get("published_at", "")[:10]

    handledFname = os.path.join(ottoDir, "lastRelease.txt")
    handled = readFile(handledFname)
    if handled == tag and not args.force:
        # nothing to do: stay quiet so cron sends no mail
        return

    if args.dry_run:
        print("strchive otto: would update from %s to %s (%s)" % (handled or "none", tag, relDate))
        return

    relDir = os.path.join(ottoDir, "releases", tag)
    os.makedirs(relDir, exist_ok=True)

    # download and check everything before touching a single /gbdb symlink, so a bad
    # release cannot leave some assemblies updated and others not
    staged = []
    for db, asm in sorted(dbToAsm.items()):
        url, assetName = assetUrl(rel, asm)
        newPath = os.path.join(relDir, "strchive.%s.bb" % db)
        tmpPath = newPath + ".tmp"
        with open(tmpPath, "wb") as fh:
            fh.write(fetchUrl(url))
        if os.path.getsize(tmpPath) == 0:
            errAbort("downloaded an empty file from %s" % url)
        os.rename(tmpPath, newPath)

        newCount = bigBedItemCount(newPath)
        if newCount == 0:
            errAbort("%s has no items" % assetName)

        gbdbLink = "/gbdb/%s/strVar/strchive.bb" % db
        oldCount = bigBedItemCount(gbdbLink)
        if oldCount:
            change = abs(newCount - oldCount) / float(oldCount)
            if change > maxItemCountChange:
                errAbort("%s item count went %d -> %d (%.0f%%), more than the %.0f%% we "
                         "allow without a human looking.  The new file is in %s; re-run "
                         "with --force once it has been checked."
                         % (db, oldCount, newCount, 100 * change, 100 * maxItemCountChange, relDir))
        staged.append((db, gbdbLink, newPath, oldCount, newCount, assetName))

    versionPath = os.path.join(relDir, "version.txt")
    writeFile(versionPath, "%s (%s)" % (tag.lstrip("v"), relDate))

    # every download passed, now move the track over to the new release
    for db, gbdbLink, newPath, oldCount, newCount, assetName in staged:
        gbdbDir = os.path.dirname(gbdbLink)
        if not os.path.isdir(gbdbDir):
            errAbort("%s does not exist -- create it before adding %s to dbToAsm" % (gbdbDir, db))
        relinkGbdb(gbdbLink, newPath)
        relinkGbdb(os.path.join(gbdbDir, "strchive.version.txt"), versionPath)

    writeFile(handledFname, tag)

    # an update really happened, so say so: this is the one case where the cron mails
    print("STRchive track updated: %s -> %s, released %s" % (handled or "(first run)", tag, relDate))
    print("Release notes: %s" % rel.get("html_url", ""))
    for db, gbdbLink, newPath, oldCount, newCount, assetName in staged:
        print("  %s: %d -> %d loci (%s)" % (db, oldCount or 0, newCount, assetName))
        print("      %s -> %s" % (gbdbLink, newPath))
    print("hgTrackUi shows this version via the dataVersion setting in "
          "trackDb/human/strVar.ra.")


main()

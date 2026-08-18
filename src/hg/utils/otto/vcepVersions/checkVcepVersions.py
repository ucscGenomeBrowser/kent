#!/usr/bin/env python3
"""checkVcepVersions.py -- monthly notifier (RM #37795).

Compares the CSpec specification version shown on each of our VCEP hub
description pages against the current released version in the ClinGen CSpec
registry. Emails the otto MAILTO ONLY when they disagree, or when the check
itself fails. Silent (no output, so cron sends no mail) when everything matches.

Our side is read from the live hgdownload description page rather than the hive
source, so this also catches a hub that was updated on hive but never pushed.

The ClinGen side comes from the CSpec affiliation page, which embeds its
specification list as an inline "svisData = [...]" JSON assignment in the raw
HTML. No JavaScript needed, but it is their UI's internal format, so a parse
failure is reported rather than ignored.

Two things this deliberately does NOT do:
  - Compare against unreleased specs. Pilot/in-prep entries are skipped.
  - Compare against genes our hub does not cover. An affiliation can hold specs
    for more genes than we display (InSiGHT 50099 also carries APC and MUTYH),
    and those move on their own schedule.

To silence after updating a hub: update the version string on that hub's
description page and push it to hgdownload.
"""

import json
import re
import sys
import time
import urllib.error
import urllib.request

# One entry per VCEP hub in the recommended track sets
# (kent/src/hg/htdocs/data/recTrackSets/recTrackSets.hg*.tab).
# hubRegex must capture the dotted version from that hub's description page.
vcepConfig = {
    "ENIGMA BRCA1/BRCA2 VCEP": {
        "hubUrl": "https://hgdownload.soe.ucsc.edu/hubs/enigma/enigma.html",
        "hubRegex": r"Guidelines for BRCA1/BRCA2\s+Version\s+(\d+(?:\.\d+)+)",
        "affiliation": "50087",
        "genes": ["BRCA1", "BRCA2"],
    },
    "InSiGHT Lynch Syndrome VCEP": {
        "hubUrl": "https://hgdownload.soe.ucsc.edu/hubs/insight/insight.html",
        "hubRegex": r"<h1>InSiGHT specs\s+(\d+(?:\.\d+)+)</h1>",
        "affiliation": "50099",
        "genes": ["MLH1", "MSH2", "MSH6", "PMS2"],
    },
}

cspecUrl = "https://cspec.genome.network/cspec/ui/svi/affiliation/"


def fetchUrl(url):
    """Fetch url and return its text. Retries a few times so a transient network
    blip does not turn into a false alarm, then raises."""
    attempts = 5
    lastErr = None
    for attempt in range(attempts):
        try:
            with urllib.request.urlopen(url, timeout=120) as resp:
                return resp.read().decode("utf-8", "replace")
        except (urllib.error.URLError, OSError) as e:
            lastErr = e
            if attempt < attempts - 1:
                time.sleep(30)
    raise RuntimeError("could not fetch " + url + " after " + str(attempts) +
                       " attempts: " + str(lastErr))


def normalizeVersion(version):
    """Turn a dotted version into a tuple for comparison, dropping trailing
    zeroes so ClinGen's '2.0' matches our page's '2.0.0'. ClinGen's version is a
    free-form string, so an unparseable one is reported rather than raising past
    the per-VCEP error handling and killing the rest of the run."""
    try:
        parts = [int(p) for p in version.split(".")]
    except ValueError:
        raise RuntimeError("could not parse version string '" + version + "'")
    while len(parts) > 1 and parts[-1] == 0:
        parts.pop()
    return tuple(parts)


def getOurVersion(config):
    """Scrape the version we publish from a hub description page."""
    html = fetchUrl(config["hubUrl"])
    match = re.search(config["hubRegex"], html)
    if match is None:
        raise RuntimeError("no version found on " + config["hubUrl"] +
                           " (page wording changed? regex: " + config["hubRegex"] + ")")
    return match.group(1)


def getClinGenVersions(config):
    """Return {gene: version} for the released CSpec specs covering our genes."""
    url = cspecUrl + config["affiliation"]
    html = fetchUrl(url)
    match = re.search(r"svisData\s*=\s*(\[.*?\])\s*;", html, re.S)
    if match is None:
        raise RuntimeError("no svisData block found at " + url + " (registry page format changed?)")
    try:
        svis = json.loads(match.group(1))
    except ValueError as e:
        raise RuntimeError("could not parse svisData JSON at " + url + ": " + str(e))

    versions = {}
    for svi in svis:
        if not svi.get("isReleased"):
            continue
        for gene in svi.get("genes", []):
            if gene.get("label") in config["genes"]:
                versions[gene["label"]] = svi["version"]

    missing = [g for g in config["genes"] if g not in versions]
    if missing:
        raise RuntimeError("no released CSpec spec for " + ", ".join(missing) + " at " + url)
    return versions


def checkVcep(name, config, report):
    """Compare one VCEP and append any mismatch to report. Returns True on
    success, False if the check itself could not be completed."""
    try:
        ourVersion = getOurVersion(config)
        clinGenVersions = getClinGenVersions(config)
        stale = {g: v for g, v in clinGenVersions.items()
                 if normalizeVersion(v) != normalizeVersion(ourVersion)}
    except RuntimeError as e:
        report.append(name + ": check failed: " + str(e))
        return False

    if stale:
        geneList = ", ".join(g + "=" + stale[g] for g in sorted(stale))
        report.append(name + " is out of date.")
        report.append("  our hub page: " + ourVersion + "  (" + config["hubUrl"] + ")")
        report.append("  ClinGen CSpec: " + geneList +
                      "  (" + cspecUrl + config["affiliation"] + ")")
    return True


def main():
    report = []
    ok = True
    for name in sorted(vcepConfig):
        if not checkVcep(name, vcepConfig[name], report):
            ok = False

    if report:
        print("VCEP specification versions need attention. refs #37795")
        print("")
        for line in report:
            print(line)
        print("")
        print("To silence: update the version on the hub description page and push it")
        print("to hgdownload, or fix the check in")
        print("kent/src/hg/utils/otto/vcepVersions/checkVcepVersions.py")

    sys.exit(0 if ok else 1)


main()

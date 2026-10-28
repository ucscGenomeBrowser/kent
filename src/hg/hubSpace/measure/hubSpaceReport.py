#!/usr/bin/env python3

"""Turn the dated measureHubSpace reports into one HTML page of charts.

The page loads hubSpaceReport.css and hubSpaceReport.js from its own directory,
so those two files must be installed next to it.
"""

import argparse
import datetime
import json
import os
import re
import socket
import sys

# section titles exactly as the measure scripts print them
SEC_UNREADABLE = "paths that could not be read"
SEC_UNSEARCHABLE = "directories not searchable by other (contents cannot be stat'ed)"
SEC_WHITESPACE = "names with leading or trailing whitespace"
SEC_FILES = "total number of uploaded files"
SEC_DISK = "total disk space consumed"
SEC_TYPES = "file type counts"
SEC_CATEGORIES = "file type counts grouped by category"
SEC_HUB_TEXT = "hub text file names seen more than once"
SEC_TXT_OTHER = "text files whose name suggests another format"
SEC_UNKNOWN = "files with no extension or an unrecognized extension"
SEC_USERS = "userNames of uploaders"
SECTIONS = {SEC_UNREADABLE, SEC_UNSEARCHABLE, SEC_WHITESPACE, SEC_FILES, SEC_DISK, SEC_TYPES,
            SEC_CATEGORIES, SEC_HUB_TEXT, SEC_TXT_OTHER, SEC_UNKNOWN, SEC_USERS}

REPORT_NAME_RE = re.compile(r"^(\d{4}-\d{2}-\d{2})T(\d{2})\.txt$")
COUNT_RE = re.compile(r"^# count: (\d+)")
TALLY_RE = re.compile(r"^\s*(\d+)\s+(\S+)\s+(.+?)\s*$")
OLD_TALLY_RE = re.compile(r"^\s*(\d+)\s+(.+?)\s*$")
SIZE_RE = re.compile(r"^([\d.]+)([BKMGTP]?)$")

# file types merged into one format for the charts, keyed on the lower cased type
FORMAT_NAMES = {
    "bw": "bigWig", "bigwig": "bigWig",
    "bb": "bigBed", "bigbed": "bigBed",
    "bam": "BAM", "cram": "CRAM",
    "vcf": "VCF", "vcf.gz": "VCF",
    "bai": "index files", "crai": "index files", "tbi": "index files", "csi": "index files",
    "fa": "FASTA", "fa.gz": "FASTA", "fasta": "FASTA", "fasta.gz": "FASTA",
    "txt": "hub text files",
}

CATEGORY_NAMES = {
    "trackData": "Track data",
    "index": "Index files",
    "hubText": "Hub text files",
    "auxiliary": "Other recognized files",
    "tusdMeta": "Upload metadata",
    "noExtension": "No extension",
    "unrecognized": "Unknown type",
    "internal": "Lock files",
}

# the page itself is drawn by hubSpaceReport.js, this only carries the data
PAGE_TEMPLATE = """<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Hub Space usage</title>
<link rel="stylesheet" href="hubSpaceReport.css">
</head>
<body>
<main id="report"></main>
<div id="tip"></div>
<script>const DATA = __DATA__;</script>
<script src="hubSpaceReport.js"></script>
</body>
</html>
"""


def parseSize(text):
    """Convert a du -h or report size such as 11.8G back to bytes, or None."""
    m = SIZE_RE.match(text)
    if m is None:
        return None
    return float(m.group(1)) * 1024 ** "BKMGTP".index(m.group(2) or "B")


def splitSections(path):
    """Return a dict of section title to the list of lines under it."""
    sections = {}
    current = None
    with open(path, errors="replace") as f:
        for line in f:
            line = line.rstrip("\n")
            if line.startswith("# ") and line[2:] in SECTIONS:
                current = line[2:]
                sections[current] = []
            elif current is not None:
                sections[current].append(line)
    return sections


def sectionCount(lines):
    """Return N from the '# count: N' line of a section, or None."""
    for line in lines or []:
        m = COUNT_RE.match(line)
        if m:
            return int(m.group(1))
    return None


def parseTally(lines):
    """Return a list of [name, count, bytes] from a count table, bytes None in old reports."""
    lines = lines or []
    hasBytes = any(line.startswith("#") and "bytes" in line for line in lines)
    rows = []
    for line in lines:
        if line.startswith("#") or not line.strip():
            continue
        if hasBytes:
            m = TALLY_RE.match(line)
            if m:
                rows.append([m.group(3), int(m.group(1)), parseSize(m.group(2))])
        else:
            m = OLD_TALLY_RE.match(line)
            if m:
                rows.append([m.group(2), int(m.group(1)), None])
    return rows


def parseReport(path, dateStr, hour):
    """Read one report, return a dict for the page or None if it is incomplete."""
    sections = splitSections(path)
    fileLines = [l for l in sections.get(SEC_FILES, []) if l.strip()]
    if not fileLines or not fileLines[0].strip().isdigit():
        print("skipping %s: no file count, report is incomplete" % path, file=sys.stderr)
        return None
    diskBytes = None
    for line in sections.get(SEC_DISK, []):
        fields = line.split("\t")
        if len(fields) == 2 and fields[1] == "total":
            diskBytes = parseSize(fields[0])
    types = parseTally(sections.get(SEC_TYPES))
    formats = None
    if types and all(t[2] is not None for t in types):
        formats = {}
        for name, count, size in types:
            if name.startswith("."):
                continue
            fmt = FORMAT_NAMES.get(name.lower(), name)
            prev = formats.get(fmt, [0, 0])
            formats[fmt] = [prev[0] + count, prev[1] + size]
    categories = None
    catRows = parseTally(sections.get(SEC_CATEGORIES))
    if catRows:
        categories = [[CATEGORY_NAMES.get(c[0], c[0]), c[1], c[2]] for c in catRows]
    users = sorted(set(l.strip() for l in sections.get(SEC_USERS, []) if l.strip()))
    return {
        "date": "%sT%s:00:00" % (dateStr, hour),
        "files": int(fileLines[0].strip()),
        "diskBytes": diskBytes,
        "formats": formats,
        "categories": categories,
        "users": users,
        "unreadable": sectionCount(sections.get(SEC_UNREADABLE)),
        "unsearchable": sectionCount(sections.get(SEC_UNSEARCHABLE)),
        "whitespace": sectionCount(sections.get(SEC_WHITESPACE)),
        "unknownType": sectionCount(sections.get(SEC_UNKNOWN)),
    }


def readReports(logDir):
    """Find every YYYY-MM-DDTHH.txt report under logDir, return them oldest first."""
    reports = []
    for dirPath, dirNames, fileNames in os.walk(logDir):
        for name in fileNames:
            m = REPORT_NAME_RE.match(name)
            if m is None:
                continue
            report = parseReport(os.path.join(dirPath, name), m.group(1), m.group(2))
            if report is not None:
                reports.append(report)
    reports.sort(key=lambda r: r["date"])
    # user names stay out of the page, only the counts go in
    prevUsers = None
    for report in reports:
        users = set(report.pop("users"))
        report["userCount"] = len(users)
        report["newUsers"] = None if prevUsers is None else len(users - prevUsers)
        prevUsers = users
    return reports


def writePage(reports, label, outFile):
    """Write the HTML page, replacing outFile in one step so it is never half written."""
    data = {
        "label": label,
        "made": datetime.datetime.now().strftime("%Y-%m-%d %H:%M"),
        "reports": reports,
    }
    # escape < so nothing in a file name can close the script element
    dataJson = json.dumps(data, separators=(",", ":")).replace("<", "\\u003c")
    page = PAGE_TEMPLATE.replace("__DATA__", dataJson)
    tmpFile = outFile + ".tmp"
    with open(tmpFile, "w") as f:
        f.write(page)
    os.chmod(tmpFile, 0o664)
    os.replace(tmpFile, outFile)


def main():
    """Parse the command line, read the reports and write the page."""
    parser = argparse.ArgumentParser(
        description="Make an HTML page of charts from the measureHubSpace reports. "
                    "hubSpaceReport.css and hubSpaceReport.js must be in the same "
                    "directory as the page.",
        epilog="example: hubSpaceReport.py ~/hubSpace ~/public_html/hubSpace/hgwdev.html")
    parser.add_argument("logDir", help="directory holding the YYYY/MM/YYYY-MM-DDTHH.txt reports")
    parser.add_argument("outFile", help="HTML file to write")
    parser.add_argument("-l", "--label", default=socket.gethostname().split(".")[0],
                        help="machine name shown on the page (default: this host)")
    args = parser.parse_args()
    reports = readReports(args.logDir)
    if not reports:
        sys.exit("no reports found under %s" % args.logDir)
    writePage(reports, args.label, args.outFile)


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
# Rebuild the hs1 sgdpCopyNumber track as a faceted composite.  refs #29344
#
# The track is 319 per-sample copy-number bigBeds supplied by the Eichler lab in 2022.
# As a classic composite that is 319 checkboxes on one hgTrackUi page (735 KB of HTML),
# which is what this rewrite is for.  Nothing about the underlying data changes: the same
# bigBeds are pointed at by the same /gbdb paths.  What is new is a metadata table, so the
# samples can be picked by region, country, sex and DNA source instead of by scrolling.
#
# Reads:
#   sgdpCopyNumberSamples.tsv        - the frozen sample list, beside this script
#   metaSrc/SGDP_metadata...txt      - SGDP sample table (fetched by sgdpCopyNumberFetchMeta.sh)
#   metaSrc/ena.ftp.pointers.txt     - library id to BioSample accession
#
# Writes (into the track data dir, then symlinked into /gbdb/hs1/sgdpCopyNumber):
#   sgdpCopyNumber_metadata.tsv
#   sgdpCopyNumber_colors.json
#   sgdpCopyNumber.trackDb.ra        - copied by hand into the trackDb tree after review

import json, os, sys, collections

DATADIR = "/hive/data/genomes/hs1/bed/sgdpCopyNumber"
SRCDIR  = os.path.join(DATADIR, "metaSrc")
GBDB    = "/gbdb/hs1/sgdpCopyNumber"
SAMPLES = os.path.join(os.path.dirname(os.path.abspath(__file__)), "sgdpCopyNumberSamples.tsv")

TSV     = os.path.join(DATADIR, "sgdpCopyNumber_metadata.tsv")
COLORS  = os.path.join(DATADIR, "sgdpCopyNumber_colors.json")
RA      = os.path.join(DATADIR, "sgdpCopyNumber.trackDb.ra")

TRACK   = "sgdpCopyNumber"

# The SGDP table's region codes, spelled out.  The two-to-four letter prefixes on the old
# subtrack names use a different vocabulary (EUR for WestEurasia, SIB for CentralAsiaSiberia);
# that mapping is only needed for the handful of samples missing from the SGDP table.
REGION_NAME = {
    "Africa":             "Africa",
    "America":            "America",
    "CentralAsiaSiberia": "Central Asia and Siberia",
    "EastAsia":           "East Asia",
    "Oceania":            "Oceania",
    "SouthAsia":          "South Asia",
    "WestEurasia":        "West Eurasia",
}
PREFIX_REGION = {
    "AFR": "Africa",              "AMR": "America",
    "SIB": "Central Asia and Siberia", "EAS": "East Asia",
    "OCN": "Oceania",             "SAS": "South Asia",
    "EUR": "West Eurasia",
}
REFERENCE = "Reference"   # the two reference-assembly baseline tracks

# Okabe-Ito, which stays distinguishable under all three kinds of color blindness.  These
# color the swatch beside each Region checkbox on the configuration page only.  The track
# image itself is itemRgb, where color means copy number, not region.
REGION_COLOR = {
    "Africa":                   "#E69F00",
    "America":                  "#56B4E9",
    "Central Asia and Siberia": "#009E73",
    "East Asia":                "#F0E442",
    "Oceania":                  "#0072B2",
    "South Asia":               "#D55E00",
    "West Eurasia":             "#CC79A7",
    REFERENCE:                  "#000000",
}

SEX_NAME = {"M": "Male", "F": "Female", "U": "Unknown"}

# The source file spells the saliva samples two ways, which would otherwise show up as two
# separate facet values for the same thing.
DNA_SOURCE_NAME = {
    "Genomic_from_blood":      "Blood",
    "Genomic_from_cell_lines": "Cell line",
    "Genomic_from_saliva":     "Saliva",
    "Genomic from saliva":     "Saliva",
}

# The SGDP table spells one country two ways, two rows each, which would otherwise be two
# separate facet values for the same place.  Exact match, not a general tidy-up of the
# country vocabulary, which is inconsistent throughout and is not ours to rewrite.
COUNTRY_FIX = {
    "CentralAfricanRepublic": "Central African Republic",
}

# Two mojibake sequences in the SGDP table, in fields we emit.  Listed explicitly rather
# than guessed at, so a third one shows up as a warning instead of being silently mangled.
TEXT_FIX = {
    "North OssetiaÐAlania": "North Ossetia-Alania",
}

NA = "NA"


def readLatin1Tsv(path):
    """Read a tab-separated file that is byte-for-byte latin-1, not utf-8.  Returns the
    header list and a list of field lists.  Strips the quotes the SGDP table wraps some
    values in, and applies TEXT_FIX."""
    rows = []
    with open(path, encoding="latin-1") as fh:
        for line in fh:
            line = line.rstrip("\r\n")
            if not line:
                continue
            fields = []
            for v in line.split("\t"):
                v = v.strip()
                if len(v) > 1 and v[0] == '"' and v[-1] == '"':
                    v = v[1:-1]
                for bad, good in TEXT_FIX.items():
                    v = v.replace(bad, good)
                fields.append(NA if v == "?" else v)   # the SGDP table's "unknown" marker
            rows.append(fields)
    return rows[0], rows[1:]


def loadSgdpSamples():
    """library id -> dict of the SGDP sample attributes we use."""
    path = os.path.join(SRCDIR, "SGDP_metadata.279public.21signedLetter.44Fan.samples.txt")
    header, rows = readLatin1Tsv(path)
    col = {name: i for i, name in enumerate(header)}
    out = {}
    for f in rows:
        libId = f[col["Illumina_ID"]]
        out[libId] = dict(
            sgdpId     = f[col["SGDP_ID"]],
            population = f[col["Population_ID"]],
            region     = REGION_NAME.get(f[col["Region"]], f[col["Region"]]),
            country    = COUNTRY_FIX.get(f[col["Country"]], f[col["Country"]]),
            town       = f[col["Town"]],
            sex        = SEX_NAME.get(f[col["Gender"]], f[col["Gender"]]),
            latitude   = f[col["Latitude"]],
            longitude  = f[col["Longitude"]],
            dnaSource  = DNA_SOURCE_NAME.get(f[col["DNA_Source"]], f[col["DNA_Source"]]),
        )
    return out


def loadEnaSamples():
    """library id -> the same attributes, from the ENA pointer file, which also carries a
    BioSample accession.  It covers fewer samples than the SGDP table but not the same ones:
    S_Naxi-2 is listed here as fully public and is missing from the SGDP table altogether.
    Used for the BioSample accession, and as a fallback for the rest."""
    path = os.path.join(SRCDIR, "ena.ftp.pointers.txt")
    out = {}
    with open(path, encoding="latin-1") as fh:
        for line in fh:
            if line.startswith("#"):
                continue
            f = [NA if v.strip() == "?" else v.strip()
                 for v in line.rstrip("\r\n").split("\t")]
            if len(f) < 17 or not f[2]:
                continue
            out[f[2]] = dict(
                sgdpId     = f[3],
                population = f[7],
                region     = REGION_NAME.get(f[9], f[9]),
                country    = COUNTRY_FIX.get(f[10], f[10]),
                town       = f[11],
                sex        = SEX_NAME.get(f[6], f[6]),
                latitude   = f[12],
                longitude  = f[13],
                dnaSource  = DNA_SOURCE_NAME.get(f[14], f[14]),
                bioSample  = f[15],
            )
    return out


def loadSampleList():
    """The frozen sample list: (libId, legacyTrackName, bigDataUrl) in file order.

    This deliberately does not read the trackDb .ra.  The generated stanzas now are
    sgdpCopyNumber.trackDb.ra, and their names no longer carry the region and population
    prefix that regionPopFromLegacyName() needs, so a script that read its own output
    would quietly lose those two fields for any sample the SGDP table misses."""
    rows = []
    for line in open(SAMPLES, encoding="utf-8"):
        if line.startswith("#") or not line.strip():
            continue
        libId, legacyName, url = line.rstrip("\n").split("\t")
        rows.append((libId, legacyName, url))
    return rows


# Only needed for samples the SGDP table does not cover: recover region and population from
# the legacy subtrack name, which is <regionPrefix>_<population>_<library id>_wssd.  Two
# samples depend on this, and one of them has no prefix at all, so it stays blank.
def regionPopFromLegacyName(legacyName, libId):
    stem = legacyName[:-len("_wssd")] if legacyName.endswith("_wssd") else legacyName
    if not stem.endswith(libId):
        return NA, NA
    stem = stem[:-len(libId)].rstrip("_")
    if not stem:
        return NA, NA
    prefix, _, population = stem.partition("_")
    return PREFIX_REGION.get(prefix, NA), (population or NA)


def main():
    sgdp = loadSgdpSamples()
    ena = loadEnaSamples()
    sampleList = loadSampleList()
    print("samples in %s: %d" % (os.path.basename(SAMPLES), len(sampleList)))

    samples = []
    unmatched = []
    for libId, legacyName, url in sampleList:
        gbdbPath = os.path.join(GBDB, os.path.basename(url))
        if not os.path.exists(gbdbPath):
            sys.exit("ERROR: %s is in the sample list but not present" % gbdbPath)

        if libId.endswith("_kmer"):
            # CHM13_kmer and GRCh38_kmer are the copy-number baseline computed on the
            # reference assemblies themselves, not SGDP donors.
            rec = dict(sgdpId=NA, population=NA, region=REFERENCE, country=NA, town=NA,
                       sex=NA, latitude=NA, longitude=NA, dnaSource=NA)
        elif libId in sgdp:
            rec = dict(sgdp[libId])
        elif libId in ena:
            rec = {k: v for k, v in ena[libId].items() if k != "bioSample"}
        else:
            # Present in the track but not in the public SGDP sample table.  Keep whatever
            # region and population the legacy subtrack name already published, and leave
            # everything else blank rather than inventing it.
            region, population = regionPopFromLegacyName(legacyName, libId)
            rec = dict(sgdpId=NA, population=population, region=region, country=NA,
                       town=NA, sex=NA, latitude=NA, longitude=NA, dnaSource=NA)
            unmatched.append((legacyName, libId, region, population))

        rec["libId"] = libId
        rec["legacyName"] = legacyName
        rec["url"] = url
        rec["bioSample"] = ena.get(libId, {}).get("bioSample", "")
        samples.append(rec)

    if unmatched:
        print("WARNING: %d samples are not in the SGDP sample table.  Region and "
              "population come from the legacy subtrack name where it carried them, and "
              "every other column is left blank:" % len(unmatched))
        for legacyName, libId, region, population in unmatched:
            print("    %-40s region=%-26s population=%s"
                  % (legacyName, region, population))

    dupes = [k for k, n in collections.Counter(s["libId"] for s in samples).items() if n > 1]
    if dupes:
        sys.exit("ERROR: duplicate library ids, which would collide as subtrack names: %s"
                 % ", ".join(dupes))

    writeMetadata(samples)
    writeColors(samples)
    writeTrackDb(samples)

    byRegion = collections.Counter(s["region"] for s in samples)
    print("\nwrote %d samples" % len(samples))
    for region, n in sorted(byRegion.items()):
        print("    %-26s %3d" % (region, n))
    print("  with a BioSample accession: %d" % sum(1 for s in samples if s["bioSample"] not in (NA, "", None)))


# A leading underscore on a field name keeps it out of the facet checkboxes but leaves it
# searchable in the table; two underscores keep it out of both.  Population and SGDP id are
# single underscore because 120 of the 157 populations have exactly two samples, which makes
# for a useless checkbox list but a genuinely useful search box.
# (column name, record key, column description or None)
# A name with no underscore prefix becomes a checkbox facet as well as a table column, so
# a missing value there has to be the literal NA that the faceted UI knows to leave out of
# the checkbox list.  Everywhere else a missing value is genuinely empty, which keeps "NA"
# out of the table and stops subtrackUrls building a link to an accession named NA.
COLUMNS = [
    ("Sample",      "libId",      "Sequencing library identifier, which is also the name of the data file"),
    ("Region",      "region",     None),
    ("Country",     "country",    None),
    ("Sex",         "sex",        None),
    ("DNA_source",  "dnaSource",  "Material the DNA was extracted from"),
    ("_Population", "population", None),
    ("_SGDP_ID",    "sgdpId",     "Sample identifier used in the SGDP publications"),
    ("_Town",       "town",       None),
    ("_BioSample",  "bioSample",  None),
    ("__Latitude",  "latitude",   None),
    ("__Longitude", "longitude",  None),
]


def cellValue(sample, column, key):
    """NA for a missing facet value, empty for a missing plain column.  Written out as an
    explicit 'is it missing' test rather than 'or NA', so a legitimate '0' latitude does
    not read as missing."""
    v = sample.get(key)
    if v is None or v == "" or v == NA:
        return NA if not column.startswith("_") else ""
    return v


def writeMetadata(samples):
    lines = []
    header = []
    for name, _, description in COLUMNS:
        header.append("%s|%s" % (name, description) if description else name)
    lines.append("\t".join(header))
    for s in sorted(samples, key=lambda s: (s["region"], s["country"], s["libId"])):
        lines.append("\t".join(cellValue(s, column, key) for column, key, _ in COLUMNS))

    # The SGDP table is latin-1 and carries a few characters that were mangled upstream.
    # TEXT_FIX handles the ones we know about; anything else left non-ASCII here is a new
    # one, and should be looked at rather than passed through to the browser.
    offenders = sorted({c for line in lines for c in line if ord(c) > 127})
    if offenders:
        sys.exit("ERROR: non-ASCII characters in the metadata table: %s\n"
                 "Check the source file and add to TEXT_FIX if it is mangled text."
                 % " ".join("%r" % c for c in offenders))

    with open(TSV, "w", encoding="utf-8") as fh:
        fh.write("\n".join(lines) + "\n")
    print("wrote %s" % TSV)


def writeColors(samples):
    present = {s["region"] for s in samples}
    colors = {"Region": {r: c for r, c in REGION_COLOR.items() if r in present}}
    noRegion = sum(1 for s in samples if s["region"] in (NA, "", None))
    if noRegion:
        # These get no swatch because the faceted UI leaves NA out of the checkbox list
        # entirely, so there is nothing to put a swatch beside.  Said out loud because a
        # growing count here means the region lookup is silently failing.
        print("note: %d sample(s) have no region and so no Region facet entry" % noRegion)
    missing = present - set(REGION_COLOR) - {NA}
    if missing:
        sys.exit("ERROR: no color assigned for region(s): %s" % ", ".join(sorted(missing)))
    with open(COLORS, "w", encoding="utf-8") as fh:
        json.dump(colors, fh, indent=4, sort_keys=True)
        fh.write("\n")
    print("wrote %s" % COLORS)


# One sample per region plus the two reference baselines are checked on by default, so the
# track shows something recognizable the first time it is turned on.  Everything else is off;
# turning on all 319 at once is what made the old version unusable.  The pick is the
# alphabetically first SGDP id in each region, purely so the choice is reproducible.
def defaultOnSamples(samples):
    best = {}
    for s in samples:
        if s["region"] == REFERENCE:
            best.setdefault(s["libId"], s)
            continue
        if s["sgdpId"] == NA:
            continue
        cur = best.get(s["region"])
        if cur is None or s["sgdpId"] < cur["sgdpId"]:
            best[s["region"]] = s
    return {s["libId"] for s in best.values()}


PARENT_STANZA = """track %(track)s
compositeTrack faceted
shortLabel SGDP Copy Number
longLabel SGDP copy number estimates
type bigBed 9 +
group varRep
priority 101
visibility hide
itemRgb on
noScoreFilter on
filter.name 0
filterLabel.name Minimum copy number
maxItems 100000
metaDataUrl %(gbdb)s/sgdpCopyNumber_metadata.tsv
colorSettingsUrl %(gbdb)s/sgdpCopyNumber_colors.json
primaryKey Sample
defaultSortField Region
maxCheckboxes 200
subtrackUrls BioSample=https://www.ebi.ac.uk/biosamples/samples/$$
noInherit on
html html/sgdpCopyNumber.html
"""


def writeTrackDb(samples):
    defaultOn = defaultOnSamples(samples)
    with open(RA, "w", encoding="utf-8") as fh:
        fh.write("# Generated by hg/makeDb/scripts/sgdpCopyNumber/sgdpCopyNumberBuild.py.\n")
        fh.write("# Do not edit by hand, edit the script and regenerate.\n\n")
        fh.write(PARENT_STANZA % dict(track=TRACK, gbdb=GBDB))
        for i, s in enumerate(sorted(samples, key=lambda s: (s["region"], s["libId"])), 1):
            label = s["sgdpId"] if s["sgdpId"] != NA else s["libId"]
            if s["region"] == REFERENCE:
                longLabel = "%s copy number estimates on the reference assembly" % \
                            s["libId"].replace("_kmer", "")
            elif s["population"] != NA:
                longLabel = "%s (%s, %s) copy number estimates" % \
                            (label, s["population"], s["region"])
            else:
                longLabel = "%s copy number estimates" % label
            # No visibility or onlyVisibility here on purpose.  A faceted composite honors a
            # child's own display mode, so pinning these to dense would take away the
            # per-item click that the copy-number value is read from.  Left unset, each child
            # draws at whatever mode the container is set to.
            fh.write("\n    track %s_%s\n" % (TRACK, s["libId"]))
            fh.write("    parent %s %s\n" % (TRACK, "on" if s["libId"] in defaultOn else "off"))
            fh.write("    bigDataUrl %s\n" % s["url"])
            fh.write("    shortLabel %s CN\n" % label)
            fh.write("    longLabel %s\n" % longLabel)
            fh.write("    type bigBed 9 +\n")
            fh.write("    itemRgb on\n")
            fh.write("    priority %d\n" % i)
    print("wrote %s (%d on by default)" % (RA, len(defaultOn)))


if __name__ == "__main__":
    main()

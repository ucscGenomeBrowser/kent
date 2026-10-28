#!/usr/bin/env python3
# Query the ENCODE API for the released ENCODE-rE2G annotations on GRCh38 and
# write, in the current directory:
#   manifest.tsv   one row per file to download: the thresholded bigInteract,
#                  thresholded bed and thresholded bedpe of every annotation
#   samples.tsv    one row per annotation, the metadata for the track
#   annotations.json, donors.json   raw API output, kept for reference
#
# Left out on purpose:
#   - the full-predictions bed (output_type "element gene links", 466 GB)
#   - the ABC model files that sit in the same annotations: they are the
#     intermediate step of the pipeline, not the rE2G predictions
#   - the two "ENCODE-rE2G (extended)" annotations, K562 and GM12878, which
#     use H3K27ac and Hi-C on top of DNase and have a different bed format
#
# e2g.tsv (the portal report the track was scoped from) is only used to check
# that the API returns the same set of annotations.
import json, sys, urllib.request, csv

BASE = "https://www.encodeproject.org"
SOFTWARE = "distal-regulation-encode_re2g"
FILEQ = ("/search/?type=File&status=released&assembly=GRCh38"
         "&analysis_step_version.software_versions.software.name=" + SOFTWARE +
         "&output_type=thresholded+links&output_type=thresholded+element+gene+links"
         "&field=accession&field=dataset&field=file_format&field=output_type"
         "&field=md5sum&field=file_size&field=href"
         "&limit=all&format=json")
ANNOTQ = ("/search/?type=Annotation&status=released"
          "&annotation_type=element+gene+regulatory+interaction+predictions"
          "&software_used.software.name=" + SOFTWARE +
          "&field=accession&field=description&field=biosample_ontology"
          "&field=treatments&field=donor&field=experimental_input"
          "&limit=all&format=json")
DONORQ = ("/search/?type=HumanDonor&field=accession&field=sex&field=life_stage"
          "&field=age&field=age_units&limit=all&format=json")
EXT = {"bigInteract": "bigInteract", "bed": "bed.gz", "bedpe": "bedpe.gz"}


def get(path):
    req = urllib.request.Request(BASE + path, headers={"Accept": "application/json"})
    with urllib.request.urlopen(req, timeout=600) as r:
        return json.load(r)["@graph"]


def clean(s):
    " metadata cells must not contain tabs or newlines "
    return " ".join(str(s).split())


def treatmentText(a):
    out = []
    for t in a.get("treatments", []):
        s = t["treatment_term_name"]
        if "amount" in t:
            s += " %g %s" % (t["amount"], t.get("amount_units", ""))
        if "duration" in t:
            s += " %g %s" % (t["duration"], t.get("duration_units", ""))
        out.append(s.strip())
    return "; ".join(sorted(out))


def ageText(d):
    age = d.get("age")
    if not age or age == "unknown":
        return "unknown"
    unit = d.get("age_units", "")
    if unit and age != "1" and not age.endswith("above"):
        unit += "s"
    return ("%s %s" % (age, unit)).strip()


def main():
    files = get(FILEQ)
    annots = get(ANNOTQ)
    donors = {d["@id"]: d for d in get(DONORQ)}
    json.dump(annots, open("annotations.json", "w"), indent=1)
    json.dump(list(donors.values()), open("donors.json", "w"), indent=1)

    reportIds = set()
    for row in csv.reader(open("e2g.tsv"), delimiter="\t"):
        if row and row[0].startswith("/annotations/"):
            reportIds.add(row[0])
    apiIds = {a["@id"] for a in annots}
    if apiIds != reportIds:
        print("WARNING: annotations differ from e2g.tsv: %d only API, %d only report"
              % (len(apiIds - reportIds), len(reportIds - apiIds)))

    extended = {a["@id"] for a in annots if "(extended)" in a["description"]}
    print("excluded extended-model annotations:",
          " ".join(sorted(a.split("/")[2] for a in extended)))
    annots = [a for a in annots if a["@id"] not in extended]
    byId = {a["@id"]: a for a in annots}

    filesByAnnot = {}
    out = csv.writer(open("manifest.tsv", "w"), delimiter="\t", lineterminator="\n")
    out.writerow(["accession", "dataset", "file_format", "output_type", "md5sum",
                  "file_size", "url", "localPath"])
    nFiles = nBytes = 0
    for f in sorted(files, key=lambda f: (f["dataset"], f["file_format"])):
        if f["output_type"] == "element gene links":
            sys.exit("full predictions file in manifest: %s" % f["accession"])
        if f["dataset"] in extended:
            continue
        if f["dataset"] not in byId:
            sys.exit("file %s has no annotation %s" % (f["accession"], f["dataset"]))
        fmt = f["file_format"]
        if fmt in filesByAnnot.setdefault(f["dataset"], {}):
            sys.exit("two %s files in %s" % (fmt, f["dataset"]))
        local = "files/%s/%s.%s" % (fmt, f["accession"], EXT[fmt])
        filesByAnnot[f["dataset"]][fmt] = f["accession"]
        out.writerow([f["accession"], f["dataset"], fmt, f["output_type"],
                      f["md5sum"], f["file_size"], BASE + f["href"], local])
        nFiles += 1
        nBytes += f["file_size"]

    # samples.tsv: everything the trackDb script needs, one row per annotation
    out = csv.writer(open("samples.tsv", "w"), delimiter="\t", lineterminator="\n")
    out.writerow(["accession", "biosample", "classification", "system", "organ",
                  "treatment", "donor", "sex", "lifeStage", "age", "inputExp",
                  "description", "bigInteract", "bed", "bedpe"])
    for a in sorted(annots, key=lambda a: a["accession"]):
        bo = a["biosample_ontology"]
        d = donors[a["donor"]]
        fa = filesByAnnot.get(a["@id"], {})
        if sorted(fa) != sorted(EXT):
            sys.exit("%s does not have one file of each format: %s" % (a["accession"], fa))
        out.writerow([clean(x) for x in [
            a["accession"], bo["term_name"], bo["classification"],
            (bo.get("system_slims") or ["unknown"])[0],
            (bo.get("organ_slims") or ["unknown"])[0],
            treatmentText(a), d["accession"], d.get("sex") or "unknown",
            d.get("life_stage") or "unknown", ageText(d),
            ",".join(e.split("/")[2] for e in a.get("experimental_input", [])),
            a["description"], fa["bigInteract"], fa["bed"], fa["bedpe"]]])

    print("annotations: %d  files: %d  GB: %.1f" % (len(annots), nFiles, nBytes / 1e9))


main()

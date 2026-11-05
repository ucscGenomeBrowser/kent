#!/usr/bin/env python3
"""Map TCGA patient IDs to cBioPortal study IDs, for links to the cBioPortal patient pages.

Gets the patients of all TCGA PanCanAtlas studies (*_tcga_pan_can_atlas_2018) from the
cBioPortal API, then those of the other TCGA studies (*_tcga, *_tcga_pub...) for patients
that are not in PanCanAtlas. Writes a tab-separated file patientId, studyId.

usage: fusionGdbCbioportal.py outTsv
"""
import sys, json, time, urllib.request

api = "https://www.cbioportal.org/api"

def getJson(url):
    for attempt in range(5):
        try:
            # the API returns 403 for Python's default user agent
            req = urllib.request.Request(url, headers={"User-Agent": "curl/7.76.1", "Accept": "application/json"})
            with urllib.request.urlopen(req, timeout=120) as resp:
                return json.load(resp)
        except Exception as e:
            print("retrying %s: %s" % (url, e), file=sys.stderr)
            time.sleep(10)
    raise Exception("could not get %s" % url)

def main():
    outFname = sys.argv[1]
    studies = [s["studyId"] for s in getJson(api + "/studies?projection=SUMMARY")]
    tcga = [s for s in studies if "_tcga" in s and "msk" not in s and not s.endswith("_gdc")]
    # PanCanAtlas first, it has all TCGA projects with uniform processing
    tcga.sort(key=lambda s: (not s.endswith("_tcga_pan_can_atlas_2018"), s))
    patToStudy = {}
    for studyId in tcga:
        pats = getJson("%s/studies/%s/patients?projection=ID&pageSize=100000" % (api, studyId))
        newCount = 0
        for p in pats:
            if p["patientId"] not in patToStudy:
                patToStudy[p["patientId"]] = studyId
                newCount += 1
        print("%s: %d patients, %d new" % (studyId, len(pats), newCount), file=sys.stderr)
    with open(outFname, "w") as ofh:
        for pat, studyId in sorted(patToStudy.items()):
            ofh.write("%s\t%s\n" % (pat, studyId))
    print("%d patients written to %s" % (len(patToStudy), outFname), file=sys.stderr)

main()

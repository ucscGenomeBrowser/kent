#!/usr/bin/env python3
"""Read the HPRC release2 sample metadata CSV (which has quoted fields containing
commas) and print "sample_id<TAB>superpopulation" for each sample.
Redmine #35415
Usage: hprc2SampleSuperpop.py <hprc_release2_sample_metadata.csv>
"""
import csv, sys

# 1000 Genomes population_abbreviation -> superpopulation
SUPERPOP = {
    "YRI": "AFR", "LWK": "AFR", "GWD": "AFR", "MSL": "AFR", "ESN": "AFR",
    "ASW": "AFR", "ACB": "AFR", "MKK": "AFR",
    "MXL": "AMR", "PUR": "AMR", "CLM": "AMR", "PEL": "AMR",
    "CHB": "EAS", "JPT": "EAS", "CHS": "EAS", "CDX": "EAS", "KHV": "EAS",
    "CEU": "EUR", "TSI": "EUR", "FIN": "EUR", "GBR": "EUR", "IBS": "EUR",
    "GIH": "SAS", "PJL": "SAS", "BEB": "SAS", "STU": "SAS", "ITU": "SAS",
}

with open(sys.argv[1], newline="") as fh:
    for row in csv.DictReader(fh):
        sid = (row.get("sample_id") or "").strip()
        if not sid:
            continue
        abbr = (row.get("population_abbreviation") or "").strip().upper()
        print("%s\t%s" % (sid, SUPERPOP.get(abbr, "OTH")))

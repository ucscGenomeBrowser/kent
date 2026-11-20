#!/usr/bin/env python3
"""
Convert the Autism Gene Browser (AGB, agb.broadinstitute.org) variant results
VCF into a VCF for the varFreqs container and the varFreqs merge.

The AGB VCF ships allele counts only (no AN, no AF), split by study design:
  de_novo_ac_proband, de_novo_ac_sibling    family-based, de novo
  transmitted_ac_proband                    inherited, seen in the proband
  untransmitted_ac_proband                  inherited, parent allele not passed on
  ac_case, ac_ctrl                          case-control cohorts

This script:
  - fills in the empty INFO Description strings
  - replaces spaces in "consequence" with "_" (VCF INFO must not contain spaces)
  - adds AC_AUT    = de novo proband + transmitted proband + case
         AC_NONAUT = de novo sibling + control
         AC_UNTRANS= untransmitted parental alleles (parent phenotype unknown)
         AC        = AC_AUT + AC_NONAUT
  - collapses records with identical CHROM/POS/REF/ALT into one, summing all
    counts. The 2026-10-12 release has 20 of these; each pair is one copy as
    class PTV (is_other_splice) with the de novo count and one as Mis0 with the
    inherited counts. The merged record keeps the first copy's annotation and
    lists both classes in variant_class, e.g. "PTV|Mis0".

Usage: agbToVcf.py AGB_variant_results.vcf.bgz out.vcf
"""

import gzip
import sys

DESC = {
    "hgvsp": "HGVS protein change",
    "hgvsc": "HGVS coding change",
    "consequence": "Most severe consequence on the canonical transcript",
    "gene_id": "Ensembl gene ID",
    "mpc": "MPC missense deleteriousness score",
    "alpha_missense": "AlphaMissense pathogenicity score",
    "is_other_splice": "Variant affects splicing outside canonical splice sites",
    "gnomad_af": "gnomAD allele frequency",
    "transcript_id": "Ensembl transcript ID",
    "variant_class": "Variant class: PTV, Mis2, Mis1, Mis0 (by MPC/AlphaMissense) or synonymous",
    "groups": "Analysis group",
    "de_novo_ac_proband": "De novo allele count in autistic probands",
    "de_novo_ac_sibling": "De novo allele count in non-autistic siblings",
    "transmitted_ac_proband": "Allele count transmitted from parents to autistic probands",
    "untransmitted_ac_proband": "Parental allele count not transmitted to autistic probands",
    "ac_case": "Allele count in autism cases (case-control cohorts)",
    "ac_ctrl": "Allele count in controls (case-control cohorts)",
}

NEWHDR = [
    '##INFO=<ID=AC,Number=A,Type=Integer,Description="Allele count, autistic plus non-autistic individuals (AC_AUT + AC_NONAUT)">',
    '##INFO=<ID=AC_AUT,Number=1,Type=Integer,Description="Allele count in autistic individuals (de novo + transmitted in probands, plus cases)">',
    '##INFO=<ID=AC_NONAUT,Number=1,Type=Integer,Description="Allele count in non-autistic individuals (de novo in siblings, plus controls)">',
    '##INFO=<ID=AC_UNTRANS,Number=1,Type=Integer,Description="Parental allele count not transmitted to autistic probands">',
]

COUNTS = ["de_novo_ac_proband", "de_novo_ac_sibling", "transmitted_ac_proband",
          "untransmitted_ac_proband", "ac_case", "ac_ctrl"]

def parseInfo(s):
    keys, d = [], {}
    for it in s.split(";"):
        k, _, v = it.partition("=")
        keys.append(k)
        d[k] = v if _ else True
    return keys, d

def fmtInfo(keys, d):
    out = []
    for k in keys:
        v = d[k]
        out.append(k if v is True else f"{k}={v}")
    return ";".join(out)

def finish(rec):
    parts, keys, d = rec
    c = {k: int(d.get(k, 0)) for k in COUNTS}
    acAut = c["de_novo_ac_proband"] + c["transmitted_ac_proband"] + c["ac_case"]
    acNon = c["de_novo_ac_sibling"] + c["ac_ctrl"]
    keys = keys + ["AC", "AC_AUT", "AC_NONAUT", "AC_UNTRANS"]
    d.update(AC=acAut + acNon, AC_AUT=acAut, AC_NONAUT=acNon,
             AC_UNTRANS=c["untransmitted_ac_proband"])
    parts[7] = fmtInfo(keys, d)
    return "\t".join(parts) + "\n"

def main(inFn, outFn):
    nIn = nOut = nDup = 0
    prev = None   # [parts, keys, dict]
    with gzip.open(inFn, "rt") as fin, open(outFn, "w") as fout:
        for line in fin:
            if line.startswith("##INFO=<ID="):
                fid = line[len("##INFO=<ID="):].split(",", 1)[0]
                if fid in DESC:
                    line = line.replace('Description=""', f'Description="{DESC[fid]}"')
                fout.write(line)
                continue
            if line.startswith("#CHROM"):
                for h in NEWHDR:
                    fout.write(h + "\n")
                fout.write(line)
                continue
            if line.startswith("#"):
                fout.write(line)
                continue
            nIn += 1
            parts = line.rstrip("\n").split("\t")
            keys, d = parseInfo(parts[7])
            if "consequence" in d:
                d["consequence"] = d["consequence"].replace(" ", "_")
            if prev and prev[0][:2] == parts[:2] and prev[0][3:5] == parts[3:5]:
                pd = prev[2]
                for k in COUNTS:
                    pd[k] = str(int(pd.get(k, 0)) + int(d.get(k, 0)))
                if d.get("variant_class") != pd.get("variant_class"):
                    pd["variant_class"] += "|" + d["variant_class"]
                nDup += 1
                continue
            if prev:
                fout.write(finish(prev)); nOut += 1
            prev = [parts, keys, d]
        if prev:
            fout.write(finish(prev)); nOut += 1
    print(f"input records: {nIn}, output records: {nOut}, duplicates merged: {nDup}",
          file=sys.stderr)

if __name__ == "__main__":
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    main(sys.argv[1], sys.argv[2])
